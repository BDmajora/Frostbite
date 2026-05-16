/*
 * snowfall/include/sf_renderer.h — Cairo-backed UI renderer.
 *
 * Paints the login screen onto a raw ARGB8888 pixel buffer.
 * Stateless: receives the current ui_state and draws it.
 */

#ifndef SF_RENDERER_H
#define SF_RENDERER_H

#include "sf_drm.h"
#include "sf_ui_state.h"

/* Draw the full login screen into the DRM framebuffer.
 * Call sf_drm_flush() afterwards to push it to the display. */
void sf_render(sf_drm_t *drm, const sf_ui_state_t *ui);

#endif /* SF_RENDERER_H */
