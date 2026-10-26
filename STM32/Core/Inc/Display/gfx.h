/*
 * gfx.h  —  tiny strip-based 2D renderer.
 *
 * Why strips: a full 320x240 RGB565 framebuffer is 150 KB, more than most F4s
 * can spare. Instead, a screen rectangle is rendered in horizontal strips
 * ("bands") of GFX_BAND_ROWS rows. For each band the paint callback is called
 * and draws *everything* in that rectangle using screen coordinates; the
 * primitives below silently clip to the current band. The band is then
 * streamed to the LCD. Same picture, ~20 KB of RAM.
 *
 *   static void paint_box(gfx_canvas_t *c, void *ctx) {
 *       gfx_fill_rect(c, 10, 10, 100, 50, COL_PANEL);
 *       gfx_text(c, 14, 14, "HELLO", 2, COL_TEXT);
 *   }
 *   gfx_render(10, 10, 100, 50, paint_box, NULL);
 */
#ifndef GFX_H
#define GFX_H

#include <stdint.h>
#include <stdbool.h>

/* Strip size: GFX_BAND_MAX_W * GFX_BAND_ROWS * 2 bytes, x2 if double-buffered */
#ifndef GFX_BAND_ROWS
#define GFX_BAND_ROWS   16
#endif
#define GFX_BAND_MAX_W  320

/* 24-bit RGB -> RGB565 */
#define RGB565(r, g, b) \
    ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))

typedef struct {
    uint16_t *buf;          /* pixels, byte-swapped RGB565, row-major       */
    int16_t   x, y;         /* screen position of buf[0]                    */
    int16_t   w, h;         /* size of this band                            */
} gfx_canvas_t;

typedef void (*gfx_paint_fn)(gfx_canvas_t *c, void *ctx);

/* Render screen rectangle (x,y,w,h) by calling `paint` once per band. */
void gfx_render(int16_t x, int16_t y, int16_t w, int16_t h,
                gfx_paint_fn paint, void *ctx);

/* ---- primitives (screen coordinates, clipped to the band) -------------- */
void gfx_fill(gfx_canvas_t *c, uint16_t color);   /* whole band */
void gfx_pixel(gfx_canvas_t *c, int16_t x, int16_t y, uint16_t color);
void gfx_fill_rect(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w, int16_t h,
                   uint16_t color);
void gfx_rect(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w, int16_t h,
              uint16_t color);                      /* 1 px outline */
void gfx_hline(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w, uint16_t color);
void gfx_vline(gfx_canvas_t *c, int16_t x, int16_t y, int16_t h, uint16_t color);
void gfx_line(gfx_canvas_t *c, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
              uint16_t color);
/* Dotted horizontal line: one pixel on, `gap` pixels off */
void gfx_hline_dotted(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w,
                      int16_t gap, uint16_t color);

/* ---- text: built-in 5x7 font, scaled by integer `scale` ----------------
 * Each glyph cell is 6*scale wide, 8*scale tall. Background is transparent
 * (draw a fill_rect first if you need one). Returns the drawn width.     */
typedef enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } gfx_align_t;

int16_t gfx_text(gfx_canvas_t *c, int16_t x, int16_t y, const char *s,
                 uint8_t scale, uint16_t color);
int16_t gfx_text_width(const char *s, uint8_t scale);
int16_t gfx_text_height(uint8_t scale);
/* Text aligned inside the horizontal span [x, x+w) */
void gfx_text_aligned(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w,
                      gfx_align_t align, const char *s, uint8_t scale,
                      uint16_t color);

/* 2-bpp anti-aliased bitmap (e.g. the logo, see logo_bitmap.h), drawn in
 * `color` and blended with the background at the edges. */
void gfx_bitmap2(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w, int16_t h,
                 const uint8_t *data, uint16_t color);

/* Font table (font5x8.c): 5 column bytes per char, LSB = top row */
extern const uint8_t gfx_font5x8[96][5];

#endif /* GFX_H */
