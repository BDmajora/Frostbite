/*
 * snowfall/include/sf_drm.h — DRM/KMS framebuffer management.
 *
 * Owns the DRM file descriptor, mode selection, dumb-buffer creation,
 * and CRTC configuration. Exposes a pixel buffer that the renderer
 * draws into, plus a dirty-flush call for page updates.
 */

#ifndef SF_DRM_H
#define SF_DRM_H

#include <stdint.h>

typedef struct {
    int          fd;          /* DRM device fd                      */
    uint32_t     conn_id;
    uint32_t     crtc_id;
    uint32_t     fb_id;
    uint32_t     buf_handle;
    uint32_t     width;       /* display width in pixels            */
    uint32_t     height;      /* display height in pixels           */
    uint32_t     stride;      /* bytes per row                      */
    uint64_t     size;        /* total mmap size                    */
    uint32_t    *pixels;      /* mmap'd ARGB8888 framebuffer        */
} sf_drm_t;

/* Open the first available DRM device, pick the preferred mode on the
 * first connected output, allocate a dumb buffer, and set the CRTC.
 * Returns 0 on success, -1 on failure. */
int  sf_drm_open(sf_drm_t *d);

/* Flush the given rectangle to the display. */
void sf_drm_flush(sf_drm_t *d, int x, int y, int w, int h);

/* Drop DRM master. Call before exec'ing the user's compositor so it
 * can claim master. */
void sf_drm_drop_master(sf_drm_t *d);

/* Unmap, destroy the buffer, close the fd. */
void sf_drm_close(sf_drm_t *d);

#endif /* SF_DRM_H */
