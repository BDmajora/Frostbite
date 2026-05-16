/*
 * snowfall/src/drm.c — DRM/KMS framebuffer management.
 */

#include "sf_drm.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <xf86drm.h>
#include <xf86drmMode.h>

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static int find_drm_device(void) {
    /* Try /dev/dri/card0..3 */
    char path[32];
    for (int i = 0; i < 4; i++) {
        snprintf(path, sizeof(path), "/dev/dri/card%d", i);
        int fd = open(path, O_RDWR | O_CLOEXEC);
        if (fd >= 0) return fd;
    }
    return -1;
}

/*
 * Try to become DRM master, retrying briefly if another client (e.g. the
 * outgoing snowcone splash) hasn't released the device yet.
 *
 * snowfall's initscript signals snowcone before starting us, but signal
 * delivery + cleanup isn't instantaneous: snowcone needs to break out of
 * its frame loop, unmap, RMFB, close. On a slow VM that can take 100ms+.
 * Rather than have main.c bounce out and sleep 2s on every cold boot, we
 * absorb that window here with short backoffs totalling ~2s worst case.
 *
 * Returns 0 on success, -1 if we still can't get master after retries.
 */
static int acquire_master(int fd) {
    /* Backoff schedule: 20ms, 40ms, 80ms, 160ms, 320ms, 640ms, 1280ms.
     * Total ~2.5s, biased toward fast resolution in the common case
     * where snowcone is already half-gone. */
    const int delays_ms[] = { 0, 20, 40, 80, 160, 320, 640, 1280 };
    const int n_attempts = (int)(sizeof(delays_ms) / sizeof(delays_ms[0]));

    for (int i = 0; i < n_attempts; i++) {
        if (delays_ms[i] > 0) {
            struct timespec ts = {
                .tv_sec  = delays_ms[i] / 1000,
                .tv_nsec = (long)(delays_ms[i] % 1000) * 1000000L,
            };
            nanosleep(&ts, NULL);
        }

        if (drmSetMaster(fd) == 0) {
            if (i > 0)
                fprintf(stderr,
                        "snowfall: acquired DRM master after %d attempt(s)\n",
                        i + 1);
            return 0;
        }
        if (errno == EALREADY) return 0; /* already ours */

        /* EPERM/EACCES = someone else is master; EINVAL/ENOSYS = old
         * kernel that doesn't gate master, treat as success. */
        if (errno == EINVAL || errno == ENOSYS) return 0;

        if (i == n_attempts - 1) {
            fprintf(stderr,
                    "snowfall: drmSetMaster failed after %d attempts: %s\n",
                    n_attempts, strerror(errno));
            return -1;
        }
    }
    return -1;
}

static int pick_connector(int fd, drmModeRes *res,
                          drmModeConnector **out_conn) {
    for (int i = 0; i < res->count_connectors; i++) {
        drmModeConnector *c = drmModeGetConnector(fd, res->connectors[i]);
        if (!c) continue;
        if (c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) {
            *out_conn = c;
            return 0;
        }
        drmModeFreeConnector(c);
    }
    return -1;
}

static uint32_t find_crtc(int fd, drmModeRes *res,
                          drmModeConnector *conn) {
    drmModeEncoder *enc = NULL;
    if (conn->encoder_id)
        enc = drmModeGetEncoder(fd, conn->encoder_id);

    if (enc) {
        uint32_t crtc = enc->crtc_id;
        drmModeFreeEncoder(enc);
        if (crtc) return crtc;
    }

    /* Walk encoders to find a usable CRTC. */
    for (int i = 0; i < conn->count_encoders; i++) {
        enc = drmModeGetEncoder(fd, conn->encoders[i]);
        if (!enc) continue;
        for (int j = 0; j < res->count_crtcs; j++) {
            if (enc->possible_crtcs & (1u << j)) {
                uint32_t crtc = res->crtcs[j];
                drmModeFreeEncoder(enc);
                return crtc;
            }
        }
        drmModeFreeEncoder(enc);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Public API                                                         */
/* ------------------------------------------------------------------ */

int sf_drm_open(sf_drm_t *d) {
    memset(d, 0, sizeof(*d));
    d->fd = -1;

    d->fd = find_drm_device();
    if (d->fd < 0) {
        fprintf(stderr, "snowfall: no DRM device found\n");
        return -1;
    }

    /*
     * Grab master before any mode-setting work. Without master,
     * drmModeSetCrtc later will silently fail and we'll end up with
     * a valid framebuffer that the kernel refuses to display — the
     * "stuck on splash" symptom.
     *
     * Treat failure as fatal. main.c's outer loop will sleep and
     * retry, by which time OpenRC should have stopped snowcone (or
     * snowcone's own master-loss watchdog will have made it exit).
     */
    if (acquire_master(d->fd) < 0) {
        fprintf(stderr,
                "snowfall: could not acquire DRM master "
                "(is snowcone still running?)\n");
        goto fail;
    }

    drmModeRes *res = drmModeGetResources(d->fd);
    if (!res) {
        fprintf(stderr, "snowfall: drmModeGetResources failed\n");
        goto fail;
    }

    drmModeConnector *conn = NULL;
    if (pick_connector(d->fd, res, &conn) < 0) {
        fprintf(stderr, "snowfall: no connected display found\n");
        drmModeFreeResources(res);
        goto fail;
    }

    d->conn_id = conn->connector_id;
    d->crtc_id = find_crtc(d->fd, res, conn);
    if (!d->crtc_id) {
        fprintf(stderr, "snowfall: no usable CRTC\n");
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        goto fail;
    }

    /* Use the preferred mode (first in the list). */
    drmModeModeInfo mode = conn->modes[0];
    d->width  = mode.hdisplay;
    d->height = mode.vdisplay;

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);

    /* Create a dumb buffer. */
    struct drm_mode_create_dumb create = {
        .width  = d->width,
        .height = d->height,
        .bpp    = 32,
    };
    if (drmIoctl(d->fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
        fprintf(stderr, "snowfall: CREATE_DUMB: %s\n", strerror(errno));
        goto fail;
    }
    d->buf_handle = create.handle;
    d->stride     = create.pitch;
    d->size       = create.size;

    /* Add framebuffer. */
    if (drmModeAddFB(d->fd, d->width, d->height, 24, 32,
                     d->stride, d->buf_handle, &d->fb_id) < 0) {
        fprintf(stderr, "snowfall: drmModeAddFB: %s\n", strerror(errno));
        goto fail;
    }

    /* mmap. */
    struct drm_mode_map_dumb map = { .handle = d->buf_handle };
    if (drmIoctl(d->fd, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
        fprintf(stderr, "snowfall: MAP_DUMB: %s\n", strerror(errno));
        goto fail;
    }
    d->pixels = mmap(NULL, d->size, PROT_READ | PROT_WRITE,
                     MAP_SHARED, d->fd, map.offset);
    if (d->pixels == MAP_FAILED) {
        d->pixels = NULL;
        fprintf(stderr, "snowfall: mmap: %s\n", strerror(errno));
        goto fail;
    }

    /*
     * Paint the buffer black before we hand it to the CRTC. The dumb
     * buffer's initial contents are undefined; without this, scanout
     * activates on uninitialized memory and you'll see a flash of
     * garbage for one frame between snowcone exiting and our first
     * sf_render() call.
     */
    memset(d->pixels, 0, (size_t)d->size);

    /* Set CRTC. */
    if (drmModeSetCrtc(d->fd, d->crtc_id, d->fb_id, 0, 0,
                       &d->conn_id, 1, &mode) < 0) {
        fprintf(stderr, "snowfall: drmModeSetCrtc: %s\n", strerror(errno));
        goto fail;
    }

    return 0;

fail:
    sf_drm_close(d);
    return -1;
}

void sf_drm_flush(sf_drm_t *d, int x, int y, int w, int h) {
    struct drm_mode_fb_dirty_cmd dirty = {
        .fb_id     = d->fb_id,
        .num_clips = 1,
    };
    struct drm_clip_rect clip = {
        .x1 = (uint16_t)x,
        .y1 = (uint16_t)y,
        .x2 = (uint16_t)(x + w),
        .y2 = (uint16_t)(y + h),
    };
    dirty.clips_ptr = (uintptr_t)&clip;
    drmIoctl(d->fd, DRM_IOCTL_MODE_DIRTYFB, &dirty);
    /* DIRTYFB is advisory — failure is non-fatal. */
}

void sf_drm_drop_master(sf_drm_t *d) {
    if (d->fd >= 0)
        drmDropMaster(d->fd);
}

void sf_drm_close(sf_drm_t *d) {
    if (d->pixels) {
        munmap(d->pixels, d->size);
        d->pixels = NULL;
    }
    if (d->fb_id) {
        drmModeRmFB(d->fd, d->fb_id);
        d->fb_id = 0;
    }
    if (d->buf_handle) {
        struct drm_mode_destroy_dumb dd = { .handle = d->buf_handle };
        drmIoctl(d->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dd);
        d->buf_handle = 0;
    }
    if (d->fd >= 0) {
        drmDropMaster(d->fd);
        close(d->fd);
        d->fd = -1;
    }
}