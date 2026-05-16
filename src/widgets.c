/*
 * snowfall/src/widgets.c — Individual UI section renderers.
 */

#include "sf_widgets.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Shared primitive                                                   */
/* ------------------------------------------------------------------ */

void sf_draw_rounded_rect(cairo_t *cr, double x, double y,
                          double w, double h, double r) {
    cairo_new_sub_path(cr);
    cairo_arc(cr, x + w - r, y + r,     r, -M_PI/2, 0);
    cairo_arc(cr, x + w - r, y + h - r, r, 0,        M_PI/2);
    cairo_arc(cr, x + r,     y + h - r, r, M_PI/2,   M_PI);
    cairo_arc(cr, x + r,     y + r,     r, M_PI,     3*M_PI/2);
    cairo_close_path(cr);
}

/* ------------------------------------------------------------------ */
/* Title                                                              */
/* ------------------------------------------------------------------ */

double sf_draw_title(cairo_t *cr, const sf_layout_t *l) {
    double y = l->cy - 220.0 * l->scale;

    cairo_select_font_face(cr, "sans-serif",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, l->font_lg);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, "snowfall", &ext);
    double tx = l->cx - ext.width / 2.0;
    cairo_move_to(cr, tx, y);
    cairo_set_source_rgb(cr, ACCENT_R, ACCENT_G, ACCENT_B);
    cairo_show_text(cr, "snowfall");

    return y + 30.0 * l->scale;
}

/* ------------------------------------------------------------------ */
/* User list                                                          */
/* ------------------------------------------------------------------ */

double sf_draw_user_list(cairo_t *cr, const sf_layout_t *l,
                         const sf_ui_state_t *ui, double y) {
    cairo_set_font_size(cr, l->font_sm);
    cairo_set_source_rgb(cr, DIM_R, DIM_G, DIM_B);
    cairo_move_to(cr, l->panel_x, y);
    cairo_show_text(cr, "USER");
    y += 8.0 * l->scale;

    double row_h = 32.0 * l->scale;
    double pad = 8.0 * l->scale;

    if (ui->user_count == 0) {
        y += row_h;
        cairo_set_source_rgb(cr, ERR_R, ERR_G, ERR_B);
        cairo_set_font_size(cr, l->font_md);
        cairo_move_to(cr, l->panel_x + pad, y);
        cairo_show_text(cr, "No users found");
        return y + row_h;
    }

    for (int i = 0; i < ui->user_count; i++) {
        y += row_h;
        int selected = (i == ui->user_index);
        int focused  = (ui->focus == SF_FOCUS_USER && selected);

        if (selected) {
            double bg_a = focused ? 0.3 : 0.15;
            cairo_set_source_rgba(cr, ACCENT_R, ACCENT_G, ACCENT_B, bg_a);
            sf_draw_rounded_rect(cr, l->panel_x, y - row_h + 6 * l->scale,
                                 l->panel_w, row_h, 4.0 * l->scale);
            cairo_fill(cr);
        }

        if (focused)
            cairo_set_source_rgb(cr, WHITE_R, WHITE_G, WHITE_B);
        else if (selected)
            cairo_set_source_rgb(cr, ACCENT_R, ACCENT_G, ACCENT_B);
        else
            cairo_set_source_rgba(cr, WHITE_R, WHITE_G, WHITE_B, 0.6);

        cairo_set_font_size(cr, l->font_md);
        cairo_move_to(cr, l->panel_x + pad, y);
        cairo_show_text(cr, ui->users[i].name);
    }

    return y + row_h * 0.5;
}

/* ------------------------------------------------------------------ */
/* Password field                                                     */
/* ------------------------------------------------------------------ */

double sf_draw_password_field(cairo_t *cr, const sf_layout_t *l,
                              const sf_ui_state_t *ui, double y) {
    y += 16.0 * l->scale;

    cairo_set_font_size(cr, l->font_sm);
    cairo_set_source_rgb(cr, DIM_R, DIM_G, DIM_B);
    cairo_move_to(cr, l->panel_x, y);
    cairo_show_text(cr, "PASSWORD");
    y += 12.0 * l->scale;

    int focused = (ui->focus == SF_FOCUS_PASSWORD);
    double field_h = 36.0 * l->scale;
    double pad = 10.0 * l->scale;

    cairo_set_source_rgba(cr, 1, 1, 1, focused ? 0.12 : 0.06);
    sf_draw_rounded_rect(cr, l->panel_x, y, l->panel_w, field_h,
                         4.0 * l->scale);
    cairo_fill(cr);

    if (focused) {
        cairo_set_source_rgb(cr, ACCENT_R, ACCENT_G, ACCENT_B);
        cairo_set_line_width(cr, 1.5 * l->scale);
        sf_draw_rounded_rect(cr, l->panel_x, y, l->panel_w, field_h,
                             4.0 * l->scale);
        cairo_stroke(cr);
    }

    /* Password dots. */
    cairo_set_source_rgb(cr, WHITE_R, WHITE_G, WHITE_B);
    double dx = l->panel_x + pad;
    double dot_y = y + field_h / 2.0 + 2.0 * l->scale;
    double dot_r = 3.5 * l->scale;
    for (int i = 0; i < ui->password_len && i < 30; i++) {
        cairo_arc(cr, dx + dot_r, dot_y, dot_r, 0, 2.0 * M_PI);
        cairo_fill(cr);
        dx += dot_r * 3.0;
    }

    /* Cursor. */
    if (focused) {
        cairo_set_source_rgba(cr, WHITE_R, WHITE_G, WHITE_B, 0.7);
        cairo_rectangle(cr, dx + 2.0, y + 8 * l->scale,
                        1.5 * l->scale, field_h - 16 * l->scale);
        cairo_fill(cr);
    }

    return y + field_h;
}

/* ------------------------------------------------------------------ */
/* Session picker                                                     */
/* ------------------------------------------------------------------ */

double sf_draw_session_picker(cairo_t *cr, const sf_layout_t *l,
                              const sf_ui_state_t *ui, double y) {
    y += 20.0 * l->scale;

    cairo_set_font_size(cr, l->font_sm);
    cairo_set_source_rgb(cr, DIM_R, DIM_G, DIM_B);
    cairo_move_to(cr, l->panel_x, y);
    cairo_show_text(cr, "SESSION");
    y += 12.0 * l->scale;

    if (ui->sessions.count == 0) {
        cairo_set_source_rgba(cr, WHITE_R, WHITE_G, WHITE_B, 0.4);
        cairo_set_font_size(cr, l->font_sm);
        cairo_move_to(cr, l->panel_x, y + 16.0 * l->scale);
        cairo_show_text(cr, "No sessions in /usr/share/wayland-sessions/");
        return y + 32.0 * l->scale;
    }

    double tag_h = 28.0 * l->scale;
    double gap = 8.0 * l->scale;
    double x = l->panel_x;

    cairo_set_font_size(cr, l->font_sm);

    for (int i = 0; i < ui->sessions.count; i++) {
        cairo_text_extents_t ext;
        cairo_text_extents(cr, ui->sessions.entries[i].name, &ext);
        double tw = ext.width + 16.0 * l->scale;

        int selected = (i == ui->session_index);
        int focused  = (ui->focus == SF_FOCUS_SESSION && selected);

        double bg_a = selected ? (focused ? 0.35 : 0.2) : 0.08;
        cairo_set_source_rgba(cr, ACCENT_R, ACCENT_G, ACCENT_B, bg_a);
        sf_draw_rounded_rect(cr, x, y, tw, tag_h, 4.0 * l->scale);
        cairo_fill(cr);

        if (focused) {
            cairo_set_source_rgb(cr, ACCENT_R, ACCENT_G, ACCENT_B);
            cairo_set_line_width(cr, 1.0 * l->scale);
            sf_draw_rounded_rect(cr, x, y, tw, tag_h, 4.0 * l->scale);
            cairo_stroke(cr);
        }

        cairo_set_source_rgb(cr, selected ? WHITE_R : DIM_R,
                                 selected ? WHITE_G : DIM_G,
                                 selected ? WHITE_B : DIM_B);
        cairo_move_to(cr, x + 8.0 * l->scale,
                      y + tag_h / 2.0 + ext.height / 2.0);
        cairo_show_text(cr, ui->sessions.entries[i].name);

        x += tw + gap;
    }

    return y + tag_h;
}

/* ------------------------------------------------------------------ */
/* Status + copyright                                                 */
/* ------------------------------------------------------------------ */

void sf_draw_status(cairo_t *cr, const sf_layout_t *l,
                    const sf_ui_state_t *ui, double y) {
    if (!ui->status[0]) return;

    y += 20.0 * l->scale;
    cairo_set_font_size(cr, l->font_md);
    cairo_set_source_rgb(cr, ERR_R, ERR_G, ERR_B);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, ui->status, &ext);
    cairo_move_to(cr, l->cx - ext.width / 2.0, y);
    cairo_show_text(cr, ui->status);
}

void sf_draw_copyright(cairo_t *cr, const sf_layout_t *l) {
    cairo_set_font_size(cr, l->font_sm * 0.85);
    cairo_set_source_rgba(cr, WHITE_R, WHITE_G, WHITE_B, 0.3);
    cairo_move_to(cr, 20.0 * l->scale, l->h - 30.0 * l->scale);
    cairo_show_text(cr, "YetiOS Project  —  GNU GPL v3.0");
}
