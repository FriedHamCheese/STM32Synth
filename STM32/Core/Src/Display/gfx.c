/*
 * gfx.c  —  strip renderer + clipped primitives. See gfx.h for the idea.
 */
#include "gfx.h"
#include "lcd.h"
#include <stdlib.h>

/* Pixels are stored byte-swapped so the band can be DMA'd to the LCD as-is
 * (controller wants high byte first; Cortex-M is little-endian). */
#define SWAP16(c) ((uint16_t)(((c) << 8) | ((c) >> 8)))

#if LCD_USE_DMA
static uint16_t band_buf[2][GFX_BAND_MAX_W * GFX_BAND_ROWS];
#else
static uint16_t band_buf[1][GFX_BAND_MAX_W * GFX_BAND_ROWS];
#endif

void gfx_render(int16_t x, int16_t y, int16_t w, int16_t h,
                gfx_paint_fn paint, void *ctx)
{
    /* clip the rectangle to the screen */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > LCD_WIDTH)  w = LCD_WIDTH - x;
    if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
    if (w <= 0 || h <= 0 || w > GFX_BAND_MAX_W) return;

    int buf_idx = 0;
    lcd_write_begin((uint16_t)x, (uint16_t)y, (uint16_t)w, (uint16_t)h);

    for (int16_t by = y; by < y + h; by += GFX_BAND_ROWS) {
        gfx_canvas_t c;
        c.buf = band_buf[buf_idx];
        c.x = x;
        c.y = by;
        c.w = w;
        c.h = (y + h - by) < GFX_BAND_ROWS ? (y + h - by) : GFX_BAND_ROWS;

        paint(&c, ctx);
        lcd_write_pixels((const uint8_t *)c.buf, (uint32_t)c.w * c.h * 2u);
#if LCD_USE_DMA
        buf_idx ^= 1;    /* render next band while this one is DMA'd out */
#endif
    }
    lcd_write_end();
}

/* ---- primitives ---------------------------------------------------------- */

void gfx_fill(gfx_canvas_t *c, uint16_t color)
{
    uint16_t s = SWAP16(color);
    int n = c->w * c->h;
    for (int i = 0; i < n; i++) c->buf[i] = s;
}

void gfx_fill_rect(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w, int16_t h,
                   uint16_t color)
{
    /* intersect with band */
    int x0 = x, y0 = y, x1 = x + w, y1 = y + h;
    if (x0 < c->x) x0 = c->x;
    if (y0 < c->y) y0 = c->y;
    if (x1 > c->x + c->w) x1 = c->x + c->w;
    if (y1 > c->y + c->h) y1 = c->y + c->h;
    if (x0 >= x1 || y0 >= y1) return;

    uint16_t s = SWAP16(color);
    for (int yy = y0; yy < y1; yy++) {
        uint16_t *row = &c->buf[(yy - c->y) * c->w];
        for (int xx = x0; xx < x1; xx++) row[xx - c->x] = s;
    }
}

void gfx_pixel(gfx_canvas_t *c, int16_t x, int16_t y, uint16_t color)
{
    if (x < c->x || y < c->y || x >= c->x + c->w || y >= c->y + c->h) return;
    c->buf[(y - c->y) * c->w + (x - c->x)] = SWAP16(color);
}

void gfx_hline(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w, uint16_t color)
{
    gfx_fill_rect(c, x, y, w, 1, color);
}

void gfx_vline(gfx_canvas_t *c, int16_t x, int16_t y, int16_t h, uint16_t color)
{
    gfx_fill_rect(c, x, y, 1, h, color);
}

void gfx_rect(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w, int16_t h,
              uint16_t color)
{
    gfx_hline(c, x, y, w, color);
    gfx_hline(c, x, y + h - 1, w, color);
    gfx_vline(c, x, y, h, color);
    gfx_vline(c, x + w - 1, y, h, color);
}

void gfx_hline_dotted(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w,
                      int16_t gap, uint16_t color)
{
    if (y < c->y || y >= c->y + c->h) return;
    for (int16_t i = 0; i < w; i += gap + 1) gfx_pixel(c, x + i, y, color);
}

void gfx_line(gfx_canvas_t *c, int16_t x0, int16_t y0, int16_t x1, int16_t y1,
              uint16_t color)
{
    /* quick reject: line entirely above/below the band */
    int16_t ymin = y0 < y1 ? y0 : y1, ymax = y0 < y1 ? y1 : y0;
    if (ymax < c->y || ymin >= c->y + c->h) return;

    /* Bresenham */
    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        gfx_pixel(c, x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* ---- text ---------------------------------------------------------------- */

int16_t gfx_text_width(const char *s, uint8_t scale)
{
    int n = 0;
    while (s[n]) n++;
    if (n == 0) return 0;
    return (int16_t)(n * 6 * scale - scale);   /* no trailing gap */
}

int16_t gfx_text_height(uint8_t scale)
{
    return (int16_t)(7 * scale);
}

int16_t gfx_text(gfx_canvas_t *c, int16_t x, int16_t y, const char *s,
                 uint8_t scale, uint16_t color)
{
    int16_t start = x;
    /* whole string outside this band? skip the glyph loops */
    if (y + 8 * scale <= c->y || y >= c->y + c->h) return gfx_text_width(s, scale);

    for (; *s; s++) {
        unsigned ch = (unsigned char)*s;
        if (ch < 32 || ch > 127) ch = '?';
        const uint8_t *g = gfx_font5x8[ch - 32];
        for (int col = 0; col < 5; col++) {
            uint8_t bits = g[col];
            for (int row = 0; row < 8; row++) {
                if (bits & (1u << row)) {
                    if (scale == 1)
                        gfx_pixel(c, x + col, y + row, color);
                    else
                        gfx_fill_rect(c, x + col * scale, y + row * scale,
                                      scale, scale, color);
                }
            }
        }
        x += 6 * scale;
    }
    return (int16_t)(x - start - scale);
}

void gfx_text_aligned(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w,
                      gfx_align_t align, const char *s, uint8_t scale,
                      uint16_t color)
{
    int16_t tw = gfx_text_width(s, scale);
    if (align == ALIGN_CENTER) x += (w - tw) / 2;
    else if (align == ALIGN_RIGHT) x += w - tw;
    gfx_text(c, x, y, s, scale, color);
}

/* ---- 2-bit anti-aliased bitmap ------------------------------------------
 * data: 2 bpp coverage (0 = transparent .. 3 = solid), row-major, rows padded
 * to whole bytes, MSB first. Edge pixels are blended with what's already in
 * the band, so the logo looks smooth on any background. */
static uint16_t blend565(uint16_t bg, uint16_t fg, unsigned a /* 0..3 */)
{
    unsigned r = (((bg >> 11) & 31) * (3 - a) + ((fg >> 11) & 31) * a) / 3;
    unsigned g = (((bg >> 5) & 63) * (3 - a) + ((fg >> 5) & 63) * a) / 3;
    unsigned b = ((bg & 31) * (3 - a) + (fg & 31) * a) / 3;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

void gfx_bitmap2(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w, int16_t h,
                 const uint8_t *data, uint16_t color)
{
    int16_t stride = (int16_t)((w + 3) / 4);
    int16_t y0 = y < c->y ? c->y : y;
    int16_t y1 = (y + h) < (c->y + c->h) ? (y + h) : (c->y + c->h);
    for (int16_t yy = y0; yy < y1; yy++) {
        const uint8_t *row = data + (yy - y) * stride;
        for (int16_t i = 0; i < w; i++) {
            int16_t xx = x + i;
            if (xx < c->x || xx >= c->x + c->w) continue;
            unsigned a = (row[i >> 2] >> (6 - 2 * (i & 3))) & 3;
            if (!a) continue;
            uint16_t *p = &c->buf[(yy - c->y) * c->w + (xx - c->x)];
            uint16_t out = (a == 3) ? color : blend565(SWAP16(*p), color, a);
            *p = SWAP16(out);
        }
    }
}
