/*
 * snowfall/include/sf_layout.h — Shared layout constants for rendering.
 */

#ifndef SF_LAYOUT_H
#define SF_LAYOUT_H

#include <math.h>
#include <stdint.h>

/* Color palette (matches YetiOS / snowcone theme). */
#define BG_R 0.04
#define BG_G 0.04
#define BG_B 0.06

#define ACCENT_R 0.557  /* baby blue — 0x8ec5ff */
#define ACCENT_G 0.773
#define ACCENT_B 1.0

#define DIM_R 0.353     /* dim blue — 0x5a8fcf */
#define DIM_G 0.561
#define DIM_B 0.812

#define WHITE_R 1.0
#define WHITE_G 1.0
#define WHITE_B 1.0

#define ERR_R 0.9
#define ERR_G 0.3
#define ERR_B 0.3

typedef struct {
    double cx, cy;       /* center of the screen          */
    double w, h;         /* screen dimensions             */
    double scale;        /* DPI-ish scale factor          */
    double panel_x;      /* left edge of the center panel */
    double panel_w;      /* panel width                   */
    double font_sm;      /* small font size               */
    double font_md;      /* medium font size              */
    double font_lg;      /* large font size               */
} sf_layout_t;

static inline sf_layout_t sf_compute_layout(uint32_t w, uint32_t h) {
    sf_layout_t l;
    l.w  = w;
    l.h  = h;
    l.cx = w / 2.0;
    l.cy = h / 2.0;
    l.scale = h / 800.0;
    if (l.scale < 0.8) l.scale = 0.8;
    l.panel_w = 360.0 * l.scale;
    l.panel_x = l.cx - l.panel_w / 2.0;
    l.font_sm = 13.0 * l.scale;
    l.font_md = 16.0 * l.scale;
    l.font_lg = 28.0 * l.scale;
    return l;
}

#endif /* SF_LAYOUT_H */
