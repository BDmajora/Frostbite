/*
 * snowfall/src/renderer.c — Top-level render call.
 *
 * Creates a Cairo surface over the DRM pixel buffer, paints the
 * background, then delegates to the widget functions for each section.
 */

#include "sf_renderer.h"
#include "sf_layout.h"
#include "sf_widgets.h"

#include <cairo.h>
#include <math.h>

void sf_render(sf_drm_t *drm, const sf_ui_state_t *ui) {
    cairo_surface_t *surf = cairo_image_surface_create_for_data(
        (unsigned char *)drm->pixels, CAIRO_FORMAT_ARGB32,
        drm->width, drm->height, drm->stride);
    cairo_t *cr = cairo_create(surf);

    sf_layout_t l = sf_compute_layout(drm->width, drm->height);

    /* Background. */
    cairo_set_source_rgb(cr, BG_R, BG_G, BG_B);
    cairo_paint(cr);

    /* Subtle decorative snowfall dots. */
    cairo_set_source_rgba(cr, ACCENT_R, ACCENT_G, ACCENT_B, 0.04);
    for (int i = 0; i < 60; i++) {
        double fx = ((i * 137 + 59) % (int)l.w);
        double fy = ((i * 211 + 97) % (int)l.h);
        double r  = (2.0 + (i % 5)) * l.scale;
        cairo_arc(cr, fx, fy, r, 0, 2.0 * M_PI);
        cairo_fill(cr);
    }

    /* Draw sections top to bottom. */
    double y = sf_draw_title(cr, &l);
    y = sf_draw_user_list(cr, &l, ui, y);
    y = sf_draw_password_field(cr, &l, ui, y);
    y = sf_draw_session_picker(cr, &l, ui, y);
    sf_draw_status(cr, &l, ui, y);
    sf_draw_copyright(cr, &l);

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}
