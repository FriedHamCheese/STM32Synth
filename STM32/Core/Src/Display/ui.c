/*
 * ui.c  —  synth status screen (CDJ-style), 320x240 landscape.
 *
 * How it works
 * ------------
 * The screen is split into independent panels ("widgets"): header, waveform,
 * envelope, volume-LFO, LFO blink LED, output. Every frame (UI_FRAME_MS):
 *
 *   1. The caller supplies a snapshot of the synth values.
 *   2. Each widget turns the values it shows into a small "view" (rounded
 *      integers / pixel positions) and hashes it.
 *   3. If the hash differs from the last drawn one, that widget alone is
 *      repainted via gfx_render() (strip renderer, see gfx.h).
 *
 * So a static screen costs almost nothing, and changing one knob only pushes
 * one panel over SPI. Every position/size/colour comes from ui_layout.h.
 *
 * Number formatting uses integer maths only (no printf("%f"), which needs
 * extra linker flags on newlib-nano and costs ~10 KB of flash).
 */
#include "ui.h"
#include "ui_data.h"
#include "ui_layout.h"
#include "gfx.h"
#include "lcd.h"
#include <string.h>
#include <math.h>

/* ======================================================================== */
/*  small helpers                                                           */
/* ======================================================================== */

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* FNV-1a hash, used to detect "did what this panel shows change?" */
static uint32_t hash_bytes(const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    uint32_t h = 2166136261u;
    while (n--) { h ^= *b++; h *= 16777619u; }
    return h;
}

/* Write unsigned int as decimal, returns pointer to the terminating NUL. */
static char *put_uint(char *out, uint32_t v)
{
    char tmp[11];
    int n = 0;
    do { tmp[n++] = (char)('0' + v % 10); v /= 10; } while (v);
    while (n) *out++ = tmp[--n];
    *out = '\0';
    return out;
}

/* Fixed-point: value with `dec` decimals, e.g. (1.234, 2) -> "1.23". */
static char *put_fixed(char *out, float v, int dec)
{
    if (v < 0) { *out++ = '-'; v = -v; }
    uint32_t scale = 1;
    for (int i = 0; i < dec; i++) scale *= 10;
    uint32_t n = (uint32_t)(v * (float)scale + 0.5f);
    out = put_uint(out, n / scale);
    if (dec > 0) {
        *out++ = '.';
        uint32_t frac = n % scale;
        for (uint32_t d = scale / 10; d > 0; d /= 10) {
            *out++ = (char)('0' + (frac / d) % 10);
        }
        *out = '\0';
    }
    return out;
}

/* A value split into big number + small unit, e.g. "250" + "ms". */
typedef struct {
    char num[8];
    char unit[4];
} numunit_t;

/* 0.1..10 s  ->  "100"ms / "1.25"s / "10.0"s */
static void fmt_time(numunit_t *o, float sec)
{
    uint32_t ms = (uint32_t)(clampf(sec, 0.0f, 99.0f) * 1000.0f + 0.5f);
    if (ms < 1000) {
        put_uint(o->num, ms);
        strcpy(o->unit, "ms");
    } else if (ms < 10000) {
        put_fixed(o->num, ms / 1000.0f, 2);
        strcpy(o->unit, "s");
    } else {
        put_fixed(o->num, ms / 1000.0f, 1);
        strcpy(o->unit, "s");
    }
}

/* 0.1..100 Hz -> "0.10" / "12.5" / "100" */
static void fmt_hz(numunit_t *o, float hz)
{
    hz = clampf(hz, 0.0f, 999.0f);
    if (hz < 9.995f)       put_fixed(o->num, hz, 2);
    else if (hz < 99.95f)  put_fixed(o->num, hz, 1);
    else                   put_fixed(o->num, hz, 0);
    strcpy(o->unit, "Hz");
}

/* 0..10x -> "0.85" / "1.00" / "10.0" */
static void fmt_gain(numunit_t *o, float g)
{
    g = clampf(g, 0.0f, 99.0f);
    if (g < 9.995f) put_fixed(o->num, g, 2);
    else            put_fixed(o->num, g, 1);
    strcpy(o->unit, "x");
}

/* Draw "<num><unit>" with the unit in a smaller font, bottoms aligned. */
static int16_t draw_numunit(gfx_canvas_t *c, int16_t x, int16_t y,
                            const numunit_t *v, uint8_t big, uint8_t small,
                            uint16_t col)
{
    int16_t w = gfx_text(c, x, y, v->num, big, col);
    if (v->unit[0]) {
        int16_t uy = y + gfx_text_height(big) - gfx_text_height(small);
        w += small + gfx_text(c, x + w + small * 2, uy, v->unit, small, col);
    }
    return w;
}

/* Standard panel: background, title strip, 1 px outline. */
static void draw_panel(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w,
                       int16_t h, const char *title)
{
    gfx_fill_rect(c, x, y, w, h, COL_PANEL);
    gfx_fill_rect(c, x, y, w, TITLE_H, COL_TITLE_BG);
    gfx_rect(c, x, y, w, h, COL_PANEL_EDGE);
    gfx_text(c, x + 4, y + (TITLE_H - 7) / 2, title, FONT_SMALL, COL_TEXT_DIM);
}

/* "label" small + value below it */
static void draw_cell(gfx_canvas_t *c, int16_t x, int16_t y, const char *label,
                      const numunit_t *v, bool valid)
{
    gfx_text(c, x, y, label, FONT_SMALL, COL_TEXT_DIM);
    if (valid) {
        draw_numunit(c, x, y + 10, v, FONT_MED, FONT_SMALL, COL_TEXT);
    } else {
        gfx_text(c, x, y + 10, "--", FONT_MED, COL_TEXT_DIM);
    }
}

/* ======================================================================== */
/*  per-frame state                                                         */
/* ======================================================================== */

static ui_data_t g_data;                 /* latest snapshot from ui_port     */
static bool      g_force = true;         /* repaint everything               */
static uint32_t  g_last_frame_ms;

/* ---- header ------------------------------------------------------------- */
typedef struct { uint8_t rise, fall; } hdr_view_t;
static hdr_view_t hdr_v;
static uint32_t   hdr_hash;

static void paint_header(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    char buf[8];
    gfx_fill(c, COL_BG);

    /* "WAVEFORM" tag, outlined like the CDJ source buttons */
    int16_t tw = gfx_text_width("WAVEFORM", FONT_SMALL);
    gfx_rect(c, HDR_X + 2, HDR_Y + 2, tw + 8, HDR_H - 4, COL_ACCENT);
    gfx_text(c, HDR_X + 6, HDR_Y + (HDR_H - 7) / 2, "WAVEFORM", FONT_SMALL,
             COL_ACCENT);

    /* right side: RISE nn%   FALL nn%  (laid out right-to-left) */
    int16_t x  = HDR_X + HDR_W - 4;
    int16_t vy = HDR_Y + (HDR_H - 14) / 2;           /* FONT_MED is 14 tall */
    int16_t ly = vy + 7;                             /* label baseline-ish  */

    char *e = put_uint(buf, hdr_v.fall); e[0] = '%'; e[1] = '\0';
    x -= gfx_text_width(buf, FONT_MED);
    gfx_text(c, x, vy, buf, FONT_MED, COL_TEXT);
    x -= gfx_text_width("FALL", FONT_SMALL) + 4;
    gfx_text(c, x, ly, "FALL", FONT_SMALL, COL_TEXT_DIM);

    x -= 14;
    e = put_uint(buf, hdr_v.rise); e[0] = '%'; e[1] = '\0';
    x -= gfx_text_width(buf, FONT_MED);
    gfx_text(c, x, vy, buf, FONT_MED, COL_TEXT);
    x -= gfx_text_width("RISE", FONT_SMALL) + 4;
    gfx_text(c, x, ly, "RISE", FONT_SMALL, COL_TEXT_DIM);
}

static void update_header(void)
{
    hdr_v.rise = (uint8_t)clampf(g_data.rise_pct + 0.5f, 0, 100);
    hdr_v.fall = (uint8_t)clampf(g_data.fall_pct + 0.5f, 0, 100);
    uint32_t h = hash_bytes(&hdr_v, sizeof hdr_v);
    if (g_force || h != hdr_hash) {
        hdr_hash = h;
        gfx_render(HDR_X, HDR_Y, HDR_W, HDR_H, paint_header, NULL);
    }
}

/* ---- waveform ----------------------------------------------------------- */
#define WAVE_IX   (WAVE_X + WAVE_PAD_X)          /* inner drawing area      */
#define WAVE_IY   (WAVE_Y + WAVE_PAD_Y)
#define WAVE_IW   (WAVE_W - 2 * WAVE_PAD_X)
#define WAVE_IH   (WAVE_H - 2 * WAVE_PAD_Y)
#define WAVE_CY   (WAVE_IY + WAVE_IH / 2)        /* zero line               */

static int16_t  wave_y[WAVE_IW];                 /* screen y per column     */
static int16_t  wave_rise_x;                     /* x of rise/fall split    */
static uint32_t wave_hash;

static void paint_wave(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill(c, COL_PANEL);
    gfx_rect(c, WAVE_X, WAVE_Y, WAVE_W, WAVE_H, COL_PANEL_EDGE);

    /* grid: zero line + dotted quarter-period lines */
    gfx_hline(c, WAVE_IX, WAVE_CY, WAVE_IW, COL_GRID);
    gfx_hline_dotted(c, WAVE_IX, WAVE_IY, WAVE_IW, 3, COL_GRID);
    gfx_hline_dotted(c, WAVE_IX, WAVE_IY + WAVE_IH - 1, WAVE_IW, 3, COL_GRID);
    for (int q = 1; q < 4 * WAVE_PERIODS; q++) {
        int16_t gx = WAVE_IX + (int16_t)((int32_t)WAVE_IW * q / (4 * WAVE_PERIODS));
        for (int16_t yy = WAVE_IY; yy < WAVE_IY + WAVE_IH; yy += 4)
            gfx_pixel(c, gx, yy, COL_GRID);
    }

    /* waveform: body filled to the zero line, then a 2 px bright outline,
     * outline drawn as vertical runs between neighbours so steep edges
     * (square/saw) stay connected. */
    for (int i = 0; i < WAVE_IW; i++) {
        int16_t x = WAVE_IX + i;
        int16_t y = wave_y[i];
        if (y < WAVE_CY) gfx_vline(c, x, y, WAVE_CY - y, COL_WAVE_FILL);
        else             gfx_vline(c, x, WAVE_CY, y - WAVE_CY + 1, COL_WAVE_FILL);
    }
    for (int i = 0; i < WAVE_IW; i++) {
        int16_t x  = WAVE_IX + i;
        int16_t y0 = wave_y[i];
        int16_t y1 = (i + 1 < WAVE_IW) ? wave_y[i + 1] : y0;
        int16_t top = y0 < y1 ? y0 : y1;
        int16_t len = (y0 < y1 ? y1 - y0 : y0 - y1) + 1;
        gfx_vline(c, x, top - 1, len + 1, COL_WAVE_LINE);
        gfx_pixel(c, x, y0, COL_WAVE_CORE);
    }

    /* rise/fall split marker: orange dashed line + small triangle on top */
    for (int16_t yy = WAVE_IY; yy < WAVE_IY + WAVE_IH; yy += 3)
        gfx_pixel(c, wave_rise_x, yy, COL_ACCENT);
    for (int k = 0; k < 4; k++)
        gfx_hline(c, wave_rise_x - 3 + k, WAVE_Y + 1 + k, 7 - 2 * k, COL_ACCENT);
}

static void update_wave(void)
{
    /* Sample one (or WAVE_PERIODS) period(s): one call per pixel column.
     * If waveform_get_point() is expensive, lower WAVE_W or sample every
     * 2nd column and interpolate. */
    for (int i = 0; i < WAVE_IW; i++) {
        float phase = (float)i * WAVE_PERIODS / (float)(WAVE_IW - 1);
        if (phase >= 1.0f && WAVE_PERIODS == 1) phase = 0.99999f;
        float v = g_data.wave_sample != NULL
                ? clampf(g_data.wave_sample(phase - floorf(phase)), -1.0f, 1.0f)
                : 0.0f;
        wave_y[i] = (int16_t)(WAVE_CY - v * (WAVE_IH / 2 - 1) + 0.5f);
    }
    wave_rise_x = WAVE_IX +
        (int16_t)(clampf(g_data.rise_pct, 0, 100) / 100.0f / WAVE_PERIODS
                  * (WAVE_IW - 1) + 0.5f);

    uint32_t h = hash_bytes(wave_y, sizeof wave_y) ^
                 hash_bytes(&wave_rise_x, sizeof wave_rise_x);
    if (g_force || h != wave_hash) {
        wave_hash = h;
        gfx_render(WAVE_X, WAVE_Y, WAVE_W, WAVE_H, paint_wave, NULL);
    }
}

/* ---- envelope (ADSR) ---------------------------------------------------- */
#define ENV_GX   (ENV_X + 6)
#define ENV_GY   (ENV_Y + TITLE_H + 4)
#define ENV_GW   (ENV_W - 12)
#define ENV_GH   ENV_GRAPH_H

typedef struct {
    bool     valid;
    uint16_t a_ms, d_ms, r_ms;
    uint8_t  s_pct;
} env_view_t;

static env_view_t env_v;
static uint32_t   env_hash;
static int16_t    env_px[5], env_py[5];   /* graph polyline: 0,A,D,S-end,R   */
static numunit_t  env_txt[4];

/* Map 0.1..10 s onto a log-ish width so 100 ms is still visible next to 10 s */
static float env_seg_weight(float sec)
{
    return 0.6f + log10f(clampf(sec, 0.1f, 10.0f) / 0.1f);   /* 0.6 .. 2.6 */
}

static void env_build_shape(void)
{
    const float sustain_w = 1.6f;                /* fixed width for "hold"  */
    float wa = env_seg_weight(g_data.attack_s);
    float wd = env_seg_weight(g_data.decay_s);
    float wr = env_seg_weight(g_data.release_s);
    float k  = (float)(ENV_GW - 1) / (wa + wd + sustain_w + wr);

    int16_t bottom = ENV_GY + ENV_GH - 1, top = ENV_GY + 1;
    int16_t sy = bottom - (int16_t)(clampf(g_data.sustain, 0, 1) * (bottom - top) + 0.5f);
    if (!g_data.adsr_valid) sy = bottom;

    env_px[0] = ENV_GX;                                env_py[0] = bottom;
    env_px[1] = env_px[0] + (int16_t)(wa * k);         env_py[1] = g_data.adsr_valid ? top : bottom;
    env_px[2] = env_px[1] + (int16_t)(wd * k);         env_py[2] = sy;
    env_px[3] = env_px[2] + (int16_t)(sustain_w * k);  env_py[3] = sy;
    env_px[4] = ENV_GX + ENV_GW - 1;                   env_py[4] = bottom;
}

/* y on the envelope polyline at column x (for the filled area) */
static int16_t env_y_at(int16_t x)
{
    for (int s = 0; s < 4; s++) {
        if (x <= env_px[s + 1]) {
            int16_t dx = env_px[s + 1] - env_px[s];
            if (dx <= 0) return env_py[s + 1];
            return env_py[s] + (int16_t)((int32_t)(env_py[s + 1] - env_py[s])
                                         * (x - env_px[s]) / dx);
        }
    }
    return env_py[4];
}

static void paint_env(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill(c, COL_BG);
    draw_panel(c, ENV_X, ENV_Y, ENV_W, ENV_H, "ENVELOPE");

    /* graph */
    int16_t bottom = ENV_GY + ENV_GH - 1;
    gfx_hline(c, ENV_GX, bottom, ENV_GW, COL_GRID);
    if (g_data.adsr_valid) {
        for (int16_t x = ENV_GX; x < ENV_GX + ENV_GW; x++) {
            int16_t y = env_y_at(x);
            if (y < bottom) gfx_vline(c, x, y, bottom - y, COL_ENV_FILL);
        }
        /* phase separators */
        for (int s = 1; s <= 3; s++)
            for (int16_t yy = ENV_GY; yy < bottom; yy += 3)
                gfx_pixel(c, env_px[s], yy, COL_GRID);
        for (int s = 0; s < 4; s++) {           /* 2 px line */
            gfx_line(c, env_px[s], env_py[s], env_px[s + 1], env_py[s + 1], COL_ENV_LINE);
            gfx_line(c, env_px[s], env_py[s] - 1, env_px[s + 1], env_py[s + 1] - 1, COL_ENV_LINE);
        }
    } else {
        gfx_text_aligned(c, ENV_GX, ENV_GY + ENV_GH / 2 - 4, ENV_GW, ALIGN_CENTER,
                         "NO ADSR YET", FONT_SMALL, COL_TEXT_DIM);
    }

    /* 2x2 value grid */
    int16_t cw  = (ENV_W - 8) / 2;
    int16_t cy0 = ENV_GY + ENV_GH + 4;
    int16_t ch  = (ENV_Y + ENV_H - cy0) / 2;
    draw_cell(c, ENV_X + 6,      cy0,      "ATTACK",  &env_txt[0], env_v.valid);
    draw_cell(c, ENV_X + 6 + cw, cy0,      "DECAY",   &env_txt[1], env_v.valid);
    draw_cell(c, ENV_X + 6,      cy0 + ch, "SUSTAIN", &env_txt[2], env_v.valid);
    draw_cell(c, ENV_X + 6 + cw, cy0 + ch, "RELEASE", &env_txt[3], env_v.valid);
}

static void update_env(void)
{
    memset(&env_v, 0, sizeof env_v);
    env_v.valid = g_data.adsr_valid;
    if (env_v.valid) {
        env_v.a_ms  = (uint16_t)(clampf(g_data.attack_s,  0, 60) * 1000.0f + 0.5f);
        env_v.d_ms  = (uint16_t)(clampf(g_data.decay_s,   0, 60) * 1000.0f + 0.5f);
        env_v.r_ms  = (uint16_t)(clampf(g_data.release_s, 0, 60) * 1000.0f + 0.5f);
        env_v.s_pct = (uint8_t)(clampf(g_data.sustain, 0, 1) * 100.0f + 0.5f);
    }
    uint32_t h = hash_bytes(&env_v, sizeof env_v);
    if (g_force || h != env_hash) {
        env_hash = h;
        fmt_time(&env_txt[0], g_data.attack_s);
        fmt_time(&env_txt[1], g_data.decay_s);
        put_fixed(env_txt[2].num, env_v.s_pct / 100.0f, 2);
        strcpy(env_txt[2].unit, "x");
        fmt_time(&env_txt[3], g_data.release_s);
        env_build_shape();
        gfx_render(ENV_X, ENV_Y, ENV_W, ENV_H, paint_env, NULL);
    }
}

/* ---- volume LFO --------------------------------------------------------- */
typedef struct { uint8_t depth_pct; uint16_t hz_x100; } lfo_view_t;
static lfo_view_t lfo_v;
static uint32_t   lfo_hash;
static numunit_t  lfo_txt[2];
static bool       led_needs_redraw;

static void paint_lfo(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill(c, COL_BG);
    draw_panel(c, LFO_X, LFO_Y, LFO_W, LFO_H, "VOL LFO");

    int16_t cw = (LFO_W - 8) / 2;
    int16_t y  = LFO_Y + TITLE_H + 4;
    draw_cell(c, LFO_X + 6,      y, "DEPTH", &lfo_txt[0], true);
    draw_cell(c, LFO_X + 6 + cw, y, "RATE",  &lfo_txt[1], true);

    /* depth bar along the bottom */
    int16_t bx = LFO_X + 6, bw = LFO_W - 12, by = LFO_Y + LFO_H - 8;
    gfx_fill_rect(c, bx, by, bw, 4, COL_BAR_BG);
    gfx_fill_rect(c, bx, by, (int16_t)(bw * lfo_v.depth_pct / 100), 4, COL_BAR);
}

static void update_lfo(void)
{
    lfo_v.depth_pct = (uint8_t)(clampf(g_data.lfo_depth, 0, 1) * 100.0f + 0.5f);
    lfo_v.hz_x100   = (uint16_t)(clampf(g_data.lfo_hz, 0, 650) * 100.0f + 0.5f);
    uint32_t h = hash_bytes(&lfo_v, sizeof lfo_v);
    if (g_force || h != lfo_hash) {
        lfo_hash = h;
        put_fixed(lfo_txt[0].num, lfo_v.depth_pct / 100.0f, 2);
        lfo_txt[0].unit[0] = '\0';
        fmt_hz(&lfo_txt[1], g_data.lfo_hz);
        gfx_render(LFO_X, LFO_Y, LFO_W, LFO_H, paint_lfo, NULL);
        led_needs_redraw = true;              /* panel repaint erased the LED */
    }
}

/* ---- LFO blink LED (own tiny widget so blinking doesn't repaint the panel)
 * NOTE: this blinks at the LFO rate using the UI clock; it is not phase-locked
 * to the audio oscillator. Above ~8 Hz (> half the frame rate) blinking would
 * alias, so the LED just stays lit.                                         */
#define LED_X  (LFO_X + LFO_W - LFO_LED_SIZE - 4)
#define LED_Y  (LFO_Y + (TITLE_H - LFO_LED_SIZE) / 2)

static bool led_on;

static void paint_led(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill(c, COL_TITLE_BG);
    uint16_t col = led_on ? COL_ACCENT : COL_LED_OFF;
    /* rounded-ish dot */
    gfx_fill_rect(c, LED_X + 1, LED_Y, LFO_LED_SIZE - 2, LFO_LED_SIZE, col);
    gfx_fill_rect(c, LED_X, LED_Y + 1, LFO_LED_SIZE, LFO_LED_SIZE - 2, col);
}

static void update_led(void)
{
    bool on;
    if (g_data.lfo_depth <= 0.005f || g_data.lfo_hz <= 0.0f) {
        on = false;
    } else if (g_data.lfo_hz > 8.0f) {
        on = true;
    } else {
        double t = ui_port_millis() / 1000.0;           /* once per frame */
        double phase = t * (double)g_data.lfo_hz;
        on = (phase - floor(phase)) < 0.5;
    }
    if (g_force || led_needs_redraw || on != led_on) {
        led_on = on;
        led_needs_redraw = false;
        gfx_render(LED_X, LED_Y, LFO_LED_SIZE, LFO_LED_SIZE, paint_led, NULL);
    }
}

/* ---- output volume -----------------------------------------------------
 * Bar is split in two halves so normal levels are readable:
 *   left half  = 0.0 .. 1.0 x   (blue)
 *   right half = 1.0 .. 10.0 x  (red, overdrive)
 * Above 1.0 x the value box turns red with black text.                    */
typedef struct { bool valid; uint16_t vol_x100; } out_view_t;
static out_view_t out_v;
static uint32_t   out_hash;
static numunit_t  out_txt;
static char       out_db[10];

static void paint_out(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    bool od = out_v.valid && out_v.vol_x100 > 100;

    gfx_fill(c, COL_BG);
    draw_panel(c, OUT_X, OUT_Y, OUT_W, OUT_H, "OUTPUT");
    if (od) {
        gfx_text_aligned(c, OUT_X, OUT_Y + (TITLE_H - 7) / 2, OUT_W - 4,
                         ALIGN_RIGHT, "OVERDRIVE", FONT_SMALL, COL_OVERDRIVE);
    }

    /* value box */
    int16_t bx = OUT_X + 4, by = OUT_Y + TITLE_H + 3;
    int16_t bw = 92, bh = gfx_text_height(FONT_BIG) + 6;
    if (!out_v.valid) {
        gfx_text(c, bx + 4, by + 3, "--", FONT_BIG, COL_TEXT_DIM);
    } else {
        if (od) gfx_fill_rect(c, bx, by, bw, bh, COL_OVERDRIVE);
        draw_numunit(c, bx + 4, by + 3, &out_txt, FONT_BIG, FONT_MED,
                     od ? COL_OD_TEXT : COL_TEXT);
        /* gain in dB to the right of the box */
        gfx_text_aligned(c, bx + bw, by + bh - 8, OUT_X + OUT_W - 4 - (bx + bw),
                         ALIGN_RIGHT, out_db, FONT_SMALL, COL_TEXT_DIM);
    }

    /* split bar */
    int16_t gx = OUT_X + 6, gw = OUT_W - 12, gy = OUT_Y + OUT_H - 9, gh = 5;
    int16_t half = gw / 2;
    gfx_fill_rect(c, gx, gy, gw, gh, COL_BAR_BG);
    if (out_v.valid) {
        float v = out_v.vol_x100 / 100.0f;
        int16_t lw = (int16_t)(clampf(v, 0, 1) * half);
        gfx_fill_rect(c, gx, gy, lw, gh, COL_BAR);
        if (v > 1.0f) {
            int16_t rw = (int16_t)(clampf((v - 1.0f) / (OUT_MAX - 1.0f), 0, 1) * (gw - half));
            gfx_fill_rect(c, gx + half, gy, rw, gh, COL_OVERDRIVE);
        }
    }
    gfx_vline(c, gx + half, gy - 2, gh + 4, COL_TEXT);    /* 1.0x tick */
}

static void update_out(void)
{
    out_v.valid    = g_data.volume_valid;
    out_v.vol_x100 = out_v.valid
                   ? (uint16_t)(clampf(g_data.volume, 0, OUT_MAX) * 100.0f + 0.5f) : 0;
    uint32_t h = hash_bytes(&out_v, sizeof out_v);
    if (g_force || h != out_hash) {
        out_hash = h;
        float v = out_v.vol_x100 / 100.0f;
        fmt_gain(&out_txt, v);
        /* dB = 20*log10(gain); show "-INF" at 0 */
        if (out_v.vol_x100 == 0) {
            strcpy(out_db, "-INF dB");
        } else {
            float db = 20.0f * log10f(v);
            char *e = out_db;
            if (db >= 0.05f) *e++ = '+';
            e = put_fixed(e, db, 1);
            strcpy(e, "dB");
        }
        gfx_render(OUT_X, OUT_Y, OUT_W, OUT_H, paint_out, NULL);
    }
}

/* ======================================================================== */
/*  public API                                                              */
/* ======================================================================== */

void ui_init(const ui_data_t *data)
{
    lcd_init();
    lcd_fill_screen(COL_BG);
    if (data != NULL)
        g_data = *data;
    g_force = true;
    g_last_frame_ms = ui_port_millis() - UI_FRAME_MS;
    ui_task(data);
}

void ui_force_redraw(void)
{
    g_force = true;
}

void ui_task(const ui_data_t *data)
{
    uint32_t now = ui_port_millis();
    if (!g_force && (uint32_t)(now - g_last_frame_ms) < UI_FRAME_MS) return;
    g_last_frame_ms = now;

    if (data != NULL)
        g_data = *data;

    update_header();
    update_wave();
    update_env();
    update_lfo();
    update_led();
    update_out();

    g_force = false;
}
