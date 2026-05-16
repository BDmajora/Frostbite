/*
 * snowfall/include/sf_widgets.h — Individual UI section renderers.
 */

#ifndef SF_WIDGETS_H
#define SF_WIDGETS_H

#include "sf_layout.h"
#include "sf_ui_state.h"

#include <cairo.h>

/* Each returns the y-coordinate after its content, for vertical flow. */
double sf_draw_title(cairo_t *cr, const sf_layout_t *l);
double sf_draw_user_list(cairo_t *cr, const sf_layout_t *l,
                         const sf_ui_state_t *ui, double y);
double sf_draw_password_field(cairo_t *cr, const sf_layout_t *l,
                              const sf_ui_state_t *ui, double y);
double sf_draw_session_picker(cairo_t *cr, const sf_layout_t *l,
                              const sf_ui_state_t *ui, double y);
void   sf_draw_status(cairo_t *cr, const sf_layout_t *l,
                      const sf_ui_state_t *ui, double y);
void   sf_draw_copyright(cairo_t *cr, const sf_layout_t *l);

/* Shared primitive. */
void sf_draw_rounded_rect(cairo_t *cr, double x, double y,
                          double w, double h, double r);

#endif /* SF_WIDGETS_H */
