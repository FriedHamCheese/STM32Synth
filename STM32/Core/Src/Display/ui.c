/*
 * ui.c  —  synth status screen (CDJ-style), 320x240 landscape.
 *
 * How it works
 * ------------
 * The firmware fills the global `ui_data` (see ui_data.h) and calls
 * ui_task(). The screen is split into independent panels ("widgets"):
 * header, waveform, wave info, envelope, LFO, LFO blink LED, filter, output.
 * Every frame (UI_FRAME_MS):
 *
 *   1. ui_data is copied into g_data, so one frame sees consistent values.
 *   2. Each widget turns the values it shows into a small "view" (rounded
 *      numbers / formatted text / pixel positions) and hashes it.
 *   3. If the hash differs from the last drawn one, that widget alone is
 *      repainted via gfx_render() (strip renderer, see gfx.h).
 *
 * So a static screen costs almost nothing, and turning one knob only pushes
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
#include "logo_bitmap.h"
#include <string.h>
#include <math.h>

/* The one shared object between firmware and screen. */
ui_data_t ui_data;

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

/* Frequency with 3 significant digits: "0.10"Hz "12.5"Hz "850"Hz "1.20"kHz
 * "48.0"kHz. Works for LFO rates up to filter cutoffs. */
static void fmt_freq(numunit_t *o, float hz)
{
    hz = clampf(hz, 0.0f, 999000.0f);
    const char *unit = "Hz";
    if (hz >= 999.5f) { hz /= 1000.0f; unit = "kHz"; }
    if (hz < 9.995f)       put_fixed(o->num, hz, 2);
    else if (hz < 99.95f)  put_fixed(o->num, hz, 1);
    else                   put_fixed(o->num, hz, 0);
    strcpy(o->unit, unit);
}

/* 0..1 -> "0.85", no unit */
static void fmt_amount(numunit_t *o, float v)
{
    put_fixed(o->num, clampf(v, 0.0f, 9.99f), 2);
    o->unit[0] = '\0';
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

/* Panel background + 1 px outline, optional title strip. */
static void draw_panel(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w,
                       int16_t h, const char *title)
{
    gfx_fill_rect(c, x, y, w, h, COL_PANEL);
    if (title) {
        gfx_fill_rect(c, x, y, w, TITLE_H, COL_TITLE_BG);
        gfx_text(c, x + 4, y + (TITLE_H - 7) / 2, title, FONT_SMALL, COL_TEXT_DIM);
    }
    gfx_rect(c, x, y, w, h, COL_PANEL_EDGE);
}

/* What a value cell shows */
typedef enum { CELL_VALUE, CELL_NA, CELL_OFF } cell_state_t;

/* small "label" + medium value 10 px below it */
static void draw_cell(gfx_canvas_t *c, int16_t x, int16_t y, const char *label,
                      const numunit_t *v, cell_state_t st)
{
    gfx_text(c, x, y, label, FONT_SMALL, COL_TEXT_DIM);
    if (st == CELL_VALUE)
        draw_numunit(c, x, y + 10, v, FONT_MED, FONT_SMALL, COL_TEXT);
    else
        gfx_text(c, x, y + 10, st == CELL_OFF ? "OFF" : "--", FONT_MED, COL_TEXT_OFF);
}

/* thin horizontal level bar, frac 0..1 */
static void draw_bar(gfx_canvas_t *c, int16_t x, int16_t y, int16_t w,
                     int16_t h, float frac, uint16_t col)
{
    gfx_fill_rect(c, x, y, w, h, COL_BAR_BG);
    gfx_fill_rect(c, x, y, (int16_t)(w * clampf(frac, 0, 1)), h, col);
}

/* ======================================================================== */
/*  per-frame state                                                         */
/* ======================================================================== */

static ui_data_t g_data;                 /* this frame's copy of ui_data     */
static bool      g_force = true;         /* repaint everything               */
static uint32_t  g_last_frame_ms;

/* ---- header: autoplay song ---------------------------------------------- */
typedef struct {
    bool manual;
    bool bpm_valid;
    uint16_t bpm;
    const char *division;    /* note division while tempo-synced, else NULL */
    char title[HDR_LABEL_MAX + 1];
} hdr_view_t;
static hdr_view_t hdr_v;
static uint32_t   hdr_hash;

static void paint_header(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill(c, COL_BG);

    /* source tag, outlined like the CDJ source buttons */
    const char *tag = hdr_v.manual ? "MANUAL PLAY" : "AUTOPLAY";
    uint16_t tcol   = hdr_v.manual ? COL_TEXT_DIM : COL_ACCENT;
    int16_t  tw     = gfx_text_width(tag, FONT_SMALL);
    gfx_rect(c, HDR_X + 2, HDR_Y + 2, tw + 8, HDR_H - 4, tcol);
    gfx_text(c, HDR_X + 6, HDR_Y + (HDR_H - 7) / 2, tag, FONT_SMALL, tcol);

    /* tempo, right-aligned; the song name below reserves that width */
    int16_t bpm_w = 0;
    if (hdr_v.bpm_valid) {
        char bpm_txt[20];
        char *p = bpm_txt;
        if (hdr_v.bpm >= 100) *p++ = (char)('0' + hdr_v.bpm / 100);
        if (hdr_v.bpm >= 10)  *p++ = (char)('0' + (hdr_v.bpm / 10) % 10);
        *p++ = (char)('0' + hdr_v.bpm % 10);
        strcpy(p, " BPM");
        if (hdr_v.division != NULL) {
            strcat(bpm_txt, " ");
            strcat(bpm_txt, hdr_v.division);
        }
        bpm_w = gfx_text_width(bpm_txt, FONT_SMALL);
        gfx_text(c, HDR_X + HDR_W - 4 - bpm_w,
                 HDR_Y + (HDR_H - 7) / 2, bpm_txt, FONT_SMALL, COL_ACCENT);
    }

    if (hdr_v.manual) return;

    /* play triangle */
    int16_t px = HDR_X + tw + 16, pcy = HDR_Y + HDR_H / 2;
    for (int k = 0; k < 5; k++)
        gfx_vline(c, px + k, pcy - 5 + k, 11 - 2 * k, COL_ACCENT);

    /* song name, truncated with ".." to fit */
    int16_t x     = px + 10;
    int     maxch = (HDR_X + HDR_W - 4 - (bpm_w > 0 ? bpm_w + 8 : 0) - x)
                    / (6 * FONT_MED);
    char    buf[HDR_LABEL_MAX + 1];
    int     n     = (int)strlen(hdr_v.title);
    if (maxch > HDR_LABEL_MAX) maxch = HDR_LABEL_MAX;
    if (n > maxch && maxch > 2) {
        memcpy(buf, hdr_v.title, (size_t)(maxch - 2));
        buf[maxch - 2] = '.'; buf[maxch - 1] = '.'; buf[maxch] = '\0';
    } else {
        strcpy(buf, hdr_v.title);
    }
    gfx_text(c, x, HDR_Y + (HDR_H - 14) / 2, buf, FONT_MED, COL_TEXT);
}

static void update_header(void)
{
    memset(&hdr_v, 0, sizeof hdr_v);
    hdr_v.bpm_valid = g_data.bpm_valid;
    hdr_v.bpm = (uint16_t)(clampf(g_data.bpm, 0.0f, 999.0f) + 0.5f);
    hdr_v.division = g_data.lfo_division;
    const char *s = g_data.autoplay_label;
    hdr_v.manual = (s == NULL || s[0] == '\0');
    if (!hdr_v.manual) {
        strncpy(hdr_v.title, s, HDR_LABEL_MAX);
        hdr_v.title[HDR_LABEL_MAX] = '\0';
    }
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
     * drawn as vertical runs between neighbours so steep edges
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
    /* One call per pixel column through the firmware's function pointer.
     * If it's expensive, lower WAVE_W or sample every 2nd column. */
    ui_wave_fn fn = g_data.wave_sample;
    for (int i = 0; i < WAVE_IW; i++) {
        float phase = (float)i * WAVE_PERIODS / (float)(WAVE_IW - 1);
        if (phase >= 1.0f && WAVE_PERIODS == 1) phase = 0.99999f;
        float v = fn ? clampf(fn(phase - floorf(phase)), -1.0f, 1.0f) : 0.0f;
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

/* ---- wave info strip: RISE / FALL / CENTER OSC -------------------------- */
typedef struct {
    char         rise[6], fall[6];
    cell_state_t osc_state;
    numunit_t    osc;
} info_view_t;
static info_view_t info_v;
static uint32_t    info_hash;

/* small label, then the value right after it, bottoms aligned */
static int16_t draw_inline(gfx_canvas_t *c, int16_t x, int16_t y,
                           const char *label, const numunit_t *v,
                           cell_state_t st)
{
    int16_t ly = y + gfx_text_height(FONT_MED) - gfx_text_height(FONT_SMALL);
    x += gfx_text(c, x, ly, label, FONT_SMALL, COL_TEXT_DIM) + 5;
    if (st == CELL_VALUE)
        return draw_numunit(c, x, y, v, FONT_MED, FONT_SMALL, COL_TEXT);
    return gfx_text(c, x, y, st == CELL_OFF ? "OFF" : "--", FONT_MED, COL_TEXT_OFF);
}

static void paint_info(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill(c, COL_BG);
    draw_panel(c, INFO_X, INFO_Y, INFO_W, INFO_H, NULL);

    int16_t y = INFO_Y + (INFO_H - 14) / 2;
    numunit_t v;
    strcpy(v.num, info_v.rise); strcpy(v.unit, "%");
    draw_inline(c, INFO_X + 6, y, "RISE", &v, CELL_VALUE);
    strcpy(v.num, info_v.fall);
    draw_inline(c, INFO_X + INFO_FALL_X, y, "FALL", &v, CELL_VALUE);
    draw_inline(c, INFO_X + INFO_OSC_X, y, "PWM", &info_v.osc, info_v.osc_state);
}

static void update_info(void)
{
    memset(&info_v, 0, sizeof info_v);
    put_uint(info_v.rise, (uint32_t)clampf(g_data.rise_pct + 0.5f, 0, 100));
    put_uint(info_v.fall, (uint32_t)clampf(g_data.fall_pct + 0.5f, 0, 100));
    if (!g_data.wave_center_pwm_valid) info_v.osc_state = CELL_NA;
    else {
        info_v.osc_state = CELL_VALUE;
        put_uint(info_v.osc.num, (uint32_t)clampf(
            g_data.wave_center_pwm_pct + 0.5f, 0, 100));
        strcpy(info_v.osc.unit, "%");
    }

    uint32_t h = hash_bytes(&info_v, sizeof info_v);
    if (g_force || h != info_hash) {
        info_hash = h;
        gfx_render(INFO_X, INFO_Y, INFO_W, INFO_H, paint_info, NULL);
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

    env_px[0] = ENV_GX;                                env_py[0] = bottom;
    env_px[1] = env_px[0] + (int16_t)(wa * k);         env_py[1] = top;
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

    int16_t bottom = ENV_GY + ENV_GH - 1;
    gfx_hline(c, ENV_GX, bottom, ENV_GW, COL_GRID);
    if (env_v.valid) {
        for (int16_t x = ENV_GX; x < ENV_GX + ENV_GW; x++) {
            int16_t y = env_y_at(x);
            if (y < bottom) gfx_vline(c, x, y, bottom - y, COL_ENV_FILL);
        }
        for (int s = 1; s <= 3; s++)            /* phase separators */
            for (int16_t yy = ENV_GY; yy < bottom; yy += 3)
                gfx_pixel(c, env_px[s], yy, COL_GRID);
        for (int s = 0; s < 4; s++) {           /* 2 px line */
            gfx_line(c, env_px[s], env_py[s], env_px[s + 1], env_py[s + 1], COL_ENV_LINE);
            gfx_line(c, env_px[s], env_py[s] - 1, env_px[s + 1], env_py[s + 1] - 1, COL_ENV_LINE);
        }
    } else {
        gfx_text_aligned(c, ENV_GX, ENV_GY + ENV_GH / 2 - 4, ENV_GW, ALIGN_CENTER,
                         "NO ADSR", FONT_SMALL, COL_TEXT_OFF);
    }

    /* 2x2 value grid */
    cell_state_t st = env_v.valid ? CELL_VALUE : CELL_NA;
    int16_t cw  = (ENV_W - 8) / 2;
    int16_t cy0 = ENV_GY + ENV_GH + 5;
    int16_t ch  = (ENV_Y + ENV_H - cy0) / 2;
    draw_cell(c, ENV_X + 6,      cy0,      "ATTACK",  &env_txt[0], st);
    draw_cell(c, ENV_X + 6 + cw, cy0,      "DECAY",   &env_txt[1], st);
    draw_cell(c, ENV_X + 6,      cy0 + ch, "SUSTAIN", &env_txt[2], st);
    draw_cell(c, ENV_X + 6 + cw, cy0 + ch, "RELEASE", &env_txt[3], st);
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

/* ---- volume LFO row ----------------------------------------------------- */
typedef struct { uint8_t depth_pct; numunit_t depth, rate; } lfo_view_t;
static lfo_view_t lfo_v;
static uint32_t   lfo_hash;
static bool       led_needs_redraw;

static void paint_lfo(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill(c, COL_BG);
    draw_panel(c, LFO_X, LFO_Y, LFO_W, LFO_H, NULL);
    draw_cell(c, LFO_X + 6,       LFO_Y + 4, "LFO DEPTH", &lfo_v.depth, CELL_VALUE);
    draw_cell(c, LFO_X + COL2_C2, LFO_Y + 4, "LFO RATE",  &lfo_v.rate,  CELL_VALUE);
    draw_bar(c, LFO_X + 6, LFO_Y + LFO_H - 5, LFO_W - 12, 2,
             lfo_v.depth_pct / 100.0f, COL_BAR);
}

static void update_lfo(void)
{
    memset(&lfo_v, 0, sizeof lfo_v);
    lfo_v.depth_pct = (uint8_t)(clampf(g_data.lfo_depth, 0, 1) * 100.0f + 0.5f);
    fmt_amount(&lfo_v.depth, lfo_v.depth_pct / 100.0f);
    fmt_freq(&lfo_v.rate, g_data.lfo_hz);
    uint32_t h = hash_bytes(&lfo_v, sizeof lfo_v);
    if (g_force || h != lfo_hash) {
        lfo_hash = h;
        gfx_render(LFO_X, LFO_Y, LFO_W, LFO_H, paint_lfo, NULL);
        led_needs_redraw = true;              /* panel repaint erased the LED */
    }
}

/* ---- LFO blink LED (own tiny widget so blinking doesn't repaint the row)
 * Blinks at the LFO rate using the UI clock; not phase-locked to the audio.
 * Above ~8 Hz (> half the frame rate) blinking would alias, so it stays lit. */
#define LED_X  (LFO_X + LFO_W - LFO_LED_SIZE - 5)
#define LED_Y  (LFO_Y + 4)

static bool led_on;

static void paint_led(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill(c, COL_PANEL);
    uint16_t col = led_on ? COL_ACCENT : COL_LED_OFF;
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
        double t = ui_port_millis() / 1000.0;
        double phase = t * (double)g_data.lfo_hz;
        on = (phase - floor(phase)) < 0.5;
    }
    if (g_force || led_needs_redraw || on != led_on) {
        led_on = on;
        led_needs_redraw = false;
        gfx_render(LED_X, LED_Y, LFO_LED_SIZE, LFO_LED_SIZE, paint_led, NULL);
    }
}

/* ---- filter row ----------------------------------------------------------
 * Cutoff column: LOW PASS / HIGH PASS / FILTER OFF (one knob).
 * Amount column: ENV AMT (ADSR) / FIXED AMT / OFF (one knob).          */
typedef struct {
    const char  *cut_label, *amt_label;
    cell_state_t cut_state,  amt_state;
    numunit_t    cut, amt;
    uint8_t      amt_pct;
} flt_view_t;
static flt_view_t flt_v;
static uint32_t   flt_hash;

static void paint_filter(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill(c, COL_BG);
    draw_panel(c, FLT_X, FLT_Y, FLT_W, FLT_H, NULL);
    draw_cell(c, FLT_X + 6,       FLT_Y + 4, flt_v.cut_label, &flt_v.cut, flt_v.cut_state);
    draw_cell(c, FLT_X + COL2_C2, FLT_Y + 4, flt_v.amt_label, &flt_v.amt, flt_v.amt_state);
    draw_bar(c, FLT_X + 6, FLT_Y + FLT_H - 5, FLT_W - 12, 2,
             flt_v.amt_pct / 100.0f, COL_BAR);
}

static void update_filter(void)
{
    memset(&flt_v, 0, sizeof flt_v);
    flt_v.cut_label = "FILTER";
    flt_v.amt_label = "AMOUNT";
    flt_v.cut_state = flt_v.amt_state = CELL_NA;

    if (g_data.filter_valid) {
        float lp = g_data.lowpass_cutoff_hz,     hp = g_data.highpass_cutoff_hz;
        float ea = g_data.filter_adsr_strength,  fa = g_data.filter_fixed_strength;

        if (lp > 0.0f)      { flt_v.cut_label = "LOW PASS";  fmt_freq(&flt_v.cut, lp); flt_v.cut_state = CELL_VALUE; }
        else if (hp > 0.0f) { flt_v.cut_label = "HIGH PASS"; fmt_freq(&flt_v.cut, hp); flt_v.cut_state = CELL_VALUE; }
        else                  flt_v.cut_state = CELL_OFF;

        float amt = 0.0f;
        if (ea > 0.005f)      { flt_v.amt_label = "ENV AMT";   amt = ea; flt_v.amt_state = CELL_VALUE; }
        else if (fa > 0.005f) { flt_v.amt_label = "FIXED AMT"; amt = fa; flt_v.amt_state = CELL_VALUE; }
        else                    flt_v.amt_state = CELL_OFF;
        flt_v.amt_pct = (uint8_t)(clampf(amt, 0, 1) * 100.0f + 0.5f);
        fmt_amount(&flt_v.amt, flt_v.amt_pct / 100.0f);
    }

    /* labels are string literals, so hashing the pointers is fine */
    uint32_t h = hash_bytes(&flt_v, sizeof flt_v);
    if (g_force || h != flt_hash) {
        flt_hash = h;
        gfx_render(FLT_X, FLT_Y, FLT_W, FLT_H, paint_filter, NULL);
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
    draw_panel(c, OUT_X, OUT_Y, OUT_W, OUT_H, NULL);
    gfx_text(c, OUT_X + 6, OUT_Y + 3, "OUTPUT", FONT_SMALL, COL_TEXT_DIM);
    if (od) {
        gfx_text_aligned(c, OUT_X, OUT_Y + 3, OUT_W - 6, ALIGN_RIGHT,
                         "OVERDRIVE", FONT_SMALL, COL_OVERDRIVE);
    }

    /* value box */
    int16_t bx = OUT_X + 4, by = OUT_Y + 12;
    int16_t bw = 92, bh = gfx_text_height(FONT_BIG) + 6;
    if (!out_v.valid) {
        gfx_text(c, bx + 4, by + 3, "--", FONT_BIG, COL_TEXT_OFF);
    } else {
        if (od) gfx_fill_rect(c, bx, by, bw, bh, COL_OVERDRIVE);
        draw_numunit(c, bx + 4, by + 3, &out_txt, FONT_BIG, FONT_MED,
                     od ? COL_OD_TEXT : COL_TEXT);
        gfx_text_aligned(c, bx + bw, by + bh - 8, OUT_X + OUT_W - 6 - (bx + bw),
                         ALIGN_RIGHT, out_db, FONT_SMALL, COL_TEXT_DIM);
    }

    /* split bar */
    int16_t gx = OUT_X + 6, gw = OUT_W - 12, gy = OUT_Y + OUT_H - 5, gh = 3;
    int16_t half = gw / 2;
    gfx_fill_rect(c, gx, gy, gw, gh, COL_BAR_BG);
    if (out_v.valid) {
        float v = out_v.vol_x100 / 100.0f;
        gfx_fill_rect(c, gx, gy, (int16_t)(clampf(v, 0, 1) * half), gh, COL_BAR);
        if (v > 1.0f) {
            int16_t rw = (int16_t)(clampf((v - 1.0f) / (OUT_MAX - 1.0f), 0, 1) * (gw - half));
            gfx_fill_rect(c, gx + half, gy, rw, gh, COL_OVERDRIVE);
        }
    }
    gfx_vline(c, gx + half, gy - 1, gh + 2, COL_TEXT);    /* 1.0x tick */
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
/*  activity detection                                                      */
/* ======================================================================== */
/*
 * Knob movement is judged in KNOB SPACE: each value is converted back to the
 * knob's physical position, 0.0 .. 1.0 of its travel, using the same curves
 * as the firmware's controls.c (the ranges are in ui_layout.h, UI_KNOB_*).
 * That way the ADSR knobs (exponential 1 ms .. 2-4 s), the volume knob
 * (two-part curve up to 10x) and plain linear knobs are all judged the same.
 *
 * A knob counts as moved only when it is more than UI_KNOB_DEADBAND away
 * from where it was at the last activity, for UI_KNOB_HOLD_FRAMES frames in
 * a row (~0.1 s). Pot noise stays inside the band, and single noise spikes
 * don't last long enough to count.
 *
 * While the LFO is tempo-synced (lfo_division set), the LFO rate jumps
 * between note divisions, so a noisy rate knob sitting on a boundary could
 * flip it back and forth. The rate isn't tracked in that mode.
 */
#define N_TRACK 13
static float       track_ref[N_TRACK];
static bool        track_on[N_TRACK];
static uint8_t     g_move_frames;          /* consecutive frames outside band */
static const char *ref_label;
static volatile bool g_activity_flag;
static uint32_t    g_last_activity_ms;

static float knob_lin(float v, float lo, float hi)
{
    return clampf((v - lo) / (hi - lo), 0.0f, 1.0f);
}

/* controls.c pot_to_ms(): ms = MIN * (MAX/MIN)^pot  ->  pot = log(ms/MIN)/log(MAX/MIN) */
static float knob_exp(float v, float lo, float hi)
{
    if (v <= lo) return 0.0f;
    return clampf(logf(v / lo) / logf(hi / lo), 0.0f, 1.0f);
}

/* controls.c ratio_to_master_volume(): 0..5% off, 5..50% -> 0..MID, 50..100% -> MID..MAX */
static float knob_volume(float v)
{
    if (v <= 0.0f) return UI_KNOB_VOL_CUTOFF;
    if (v <= UI_KNOB_VOL_MID)
        return UI_KNOB_VOL_CUTOFF + v / UI_KNOB_VOL_MID * (0.5f - UI_KNOB_VOL_CUTOFF);
    return 0.5f + clampf((v - UI_KNOB_VOL_MID) / (UI_KNOB_VOL_MAX - UI_KNOB_VOL_MID), 0, 1) * 0.5f;
}

static void track_values(float v[N_TRACK], bool on[N_TRACK])
{
    bool synced = (g_data.lfo_division != NULL);
    v[0]  = knob_lin(g_data.rise_pct, 0, 100);                          on[0]  = true;
    v[1]  = knob_exp(g_data.attack_s  * 1000.0f, UI_KNOB_ADSR_MIN_MS, UI_KNOB_ATTACK_MAX_MS);
    v[2]  = knob_exp(g_data.decay_s   * 1000.0f, UI_KNOB_ADSR_MIN_MS, UI_KNOB_DECAY_MAX_MS);
    v[3]  = knob_lin(g_data.sustain, 0, 1);
    v[4]  = knob_exp(g_data.release_s * 1000.0f, UI_KNOB_ADSR_MIN_MS, UI_KNOB_RELEASE_MAX_MS);
    on[1] = on[2] = on[3] = on[4] = g_data.adsr_valid;
    v[5]  = knob_lin(g_data.lfo_depth, 0, 1);                           on[5]  = true;
    v[6]  = knob_lin(g_data.lfo_hz, UI_KNOB_LFO_MIN_HZ, UI_KNOB_LFO_MAX_HZ); on[6] = !synced;
    v[7]  = knob_volume(g_data.volume);                                 on[7]  = g_data.volume_valid;
    v[8]  = knob_lin(g_data.wave_center_pwm_pct, 0, 100);               on[8]  = g_data.wave_center_pwm_valid;
    v[9]  = knob_lin(g_data.bpm, UI_KNOB_BPM_MIN, UI_KNOB_BPM_MAX);     on[9]  = g_data.bpm_valid && !synced;
    v[10] = knob_exp(g_data.lowpass_cutoff_hz  + 20.0f, 20.0f, 20020.0f);
    v[11] = knob_exp(g_data.highpass_cutoff_hz + 20.0f, 20.0f, 20020.0f);
    v[12] = knob_lin(g_data.filter_adsr_strength, 0, 1);
    on[10] = on[11] = on[12] = g_data.filter_valid;
}

void ui_notify_activity(void)
{
    g_activity_flag = true;
}

/* Take the current values as the new reference (no activity reported). */
static void activity_rebase(uint32_t now)
{
    track_values(track_ref, track_on);
    ref_label     = g_data.autoplay_label;
    g_move_frames = 0;
    g_last_activity_ms = now;
}

static bool autoplay_running(void)
{
    return g_data.autoplay_label != NULL && g_data.autoplay_label[0] != '\0';
}

/* true if something happened since the last call; updates the idle timer.
 * label_counts: whether a change of autoplay song counts as activity (on the
 * main screen yes; on the idle screen it just switches the idle view). */
static bool activity_check(uint32_t now, bool label_counts)
{
    bool act = false;
    if (g_activity_flag) { g_activity_flag = false; act = true; }

    float v[N_TRACK];
    bool  on[N_TRACK];
    track_values(v, on);
    bool moved = false;
    for (int i = 0; i < N_TRACK; i++) {
        if (on[i] != track_on[i]) { track_ref[i] = v[i]; track_on[i] = on[i]; }
        else if (on[i] && fabsf(v[i] - track_ref[i]) > UI_KNOB_DEADBAND) moved = true;
    }
    if (moved) {
        if (++g_move_frames >= UI_KNOB_HOLD_FRAMES) act = true;
    } else {
        g_move_frames = 0;
    }
    if (label_counts && g_data.autoplay_label != ref_label) act = true;
    ref_label = g_data.autoplay_label;

    if (act) activity_rebase(now);
    return act;
}

/* ======================================================================== */
/*  boot screen                                                             */
/* ======================================================================== */

/* Just the ORPHEUS logo, centred on black, for UI_BOOT_MS. Drawn once. */
static uint32_t g_boot_start_ms;

static void paint_boot(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill(c, COL_BG);
    gfx_bitmap2(c, (LCD_WIDTH - LOGO_BOOT_W) / 2, (LCD_HEIGHT - LOGO_BOOT_H) / 2,
                LOGO_BOOT_W, LOGO_BOOT_H, logo_boot, COL_LOGO);
}

/* ======================================================================== */
/*  idle screen: DVD-style bouncing logo                                    */
/* ======================================================================== */
/*
 * The ORPHEUS logo bitmap drifts diagonally and
 * bounces off the screen edges, changing colour on every bounce.
 * Each frame only the rectangle covering the old + new logo position is
 * redrawn (~20 KB over SPI), so the rest of the black screen is never resent.
 * Movement is time-based, so speed doesn't depend on the frame rate.
 */
static const uint16_t idle_colors[] = {
    COL_LOGO,                   /* logo yellow */
    RGB565(0xFF, 0x8C, 0x00),   /* orange  */
    RGB565(0x40, 0xA8, 0xFF),   /* blue    */
    RGB565(0x40, 0xE0, 0x70),   /* green   */
    RGB565(0xFF, 0x50, 0xC0),   /* magenta */
    RGB565(0xFF, 0xE0, 0x30),   /* yellow  */
    RGB565(0x30, 0xE0, 0xE0),   /* cyan    */
    RGB565(0xFF, 0xFF, 0xFF),   /* white   */
};
#define N_IDLE_COLORS (sizeof idle_colors / sizeof idle_colors[0])

static int16_t  logo_w, logo_h;
static float    logo_x, logo_y, logo_vx, logo_vy;
static int16_t  logo_drawn_x, logo_drawn_y;   /* where it is on the glass */
static uint8_t  logo_color;
static uint32_t g_idle_last_ms;

static void paint_logo(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill(c, COL_BG);
    gfx_bitmap2(c, logo_drawn_x, logo_drawn_y, LOGO_IDLE_W, LOGO_IDLE_H,
                logo_idle, idle_colors[logo_color]);
}

static void logo_enter(uint32_t now)
{
    logo_w = LOGO_IDLE_W;
    logo_h = LOGO_IDLE_H;

    /* start in the middle, heading down-right */
    logo_x  = (LCD_WIDTH  - logo_w) / 2.0f;
    logo_y  = (LCD_HEIGHT - logo_h) / 2.0f;
    logo_vx = IDLE_SPEED_X;
    logo_vy = IDLE_SPEED_Y;
    logo_drawn_x = (int16_t)logo_x;
    logo_drawn_y = (int16_t)logo_y;
    g_idle_last_ms = now;

    lcd_fill_screen(COL_BG);
    gfx_render(logo_drawn_x, logo_drawn_y, logo_w, logo_h, paint_logo, NULL);
}

static void logo_animate(uint32_t now)
{
    float dt = (float)(now - g_idle_last_ms) / 1000.0f;
    g_idle_last_ms = now;
    if (dt > 0.2f) dt = 0.2f;                      /* after a long stall */

    logo_x += logo_vx * dt;
    logo_y += logo_vy * dt;

    float max_x = (float)(LCD_WIDTH - logo_w), max_y = (float)(LCD_HEIGHT - logo_h);
    bool bounced = false;
    if (logo_x < 0)     { logo_x = -logo_x;            logo_vx = -logo_vx; bounced = true; }
    if (logo_x > max_x) { logo_x = 2 * max_x - logo_x; logo_vx = -logo_vx; bounced = true; }
    if (logo_y < 0)     { logo_y = -logo_y;            logo_vy = -logo_vy; bounced = true; }
    if (logo_y > max_y) { logo_y = 2 * max_y - logo_y; logo_vy = -logo_vy; bounced = true; }
    if (bounced) logo_color = (uint8_t)((logo_color + 1) % N_IDLE_COLORS);

    int16_t nx = (int16_t)logo_x, ny = (int16_t)logo_y;
    if (nx == logo_drawn_x && ny == logo_drawn_y && !bounced) return;

    /* redraw the union of old and new position: erases the trail too */
    int16_t dx = nx < logo_drawn_x ? nx : logo_drawn_x;
    int16_t dy = ny < logo_drawn_y ? ny : logo_drawn_y;
    int16_t dw = (int16_t)((nx > logo_drawn_x ? nx : logo_drawn_x) + logo_w - dx);
    int16_t dh = (int16_t)((ny > logo_drawn_y ? ny : logo_drawn_y) + logo_h - dy);
    logo_drawn_x = nx;
    logo_drawn_y = ny;
    gfx_render(dx, dy, dw, dh, paint_logo, NULL);
}

/* ======================================================================== */
/*  idle screen while autoplay plays: NOW PLAYING + scrolling wave          */
/* ======================================================================== */
/*
 * The current waveform is sampled once on entry into a table of one period
 * and scrolls slowly across the screen; only the wave band is redrawn per
 * frame. (The waveform can't change while idle: that would be knob activity
 * and would wake the main screen.)
 */
#define NP_CY   (NP_WAVE_Y + NP_WAVE_H / 2)

static int16_t  np_tab[NP_PERIOD_W];        /* screen y per column, 1 period */
static uint32_t g_np_start_ms;
static int16_t  g_np_offset;

static void paint_np_wave(gfx_canvas_t *c, void *ctx)
{
    (void)ctx;
    gfx_fill_rect(c, 0, NP_WAVE_Y, LCD_WIDTH, NP_WAVE_H, COL_BG);
    gfx_hline_dotted(c, 0, NP_CY, LCD_WIDTH, 3, COL_GRID);
    for (int16_t x = 0; x < LCD_WIDTH; x++) {
        int16_t y = np_tab[(x + g_np_offset) % NP_PERIOD_W];
        if (y < NP_CY) gfx_vline(c, x, y, NP_CY - y, COL_WAVE_FILL);
        else           gfx_vline(c, x, NP_CY, y - NP_CY + 1, COL_WAVE_FILL);
    }
    for (int16_t x = 0; x < LCD_WIDTH; x++) {
        int16_t y0 = np_tab[(x + g_np_offset) % NP_PERIOD_W];
        int16_t y1 = np_tab[(x + 1 + g_np_offset) % NP_PERIOD_W];
        int16_t top = y0 < y1 ? y0 : y1;
        int16_t len = (y0 < y1 ? y1 - y0 : y0 - y1) + 1;
        gfx_vline(c, x, top - 1, len + 1, COL_WAVE_LINE);
        gfx_pixel(c, x, y0, COL_WAVE_CORE);
    }
}

static void paint_np(gfx_canvas_t *c, void *ctx)
{
    gfx_fill(c, COL_BG);

    /* "> NOW PLAYING" */
    const char *tag = "NOW PLAYING";
    int16_t tw = gfx_text_width(tag, FONT_SMALL);
    int16_t tx = (LCD_WIDTH - tw + 10) / 2, ty = 12;
    for (int k = 0; k < 4; k++)                      /* play triangle */
        gfx_vline(c, tx - 10 + k, ty + k, 7 - 2 * k, COL_ACCENT);
    gfx_text(c, tx, ty, tag, FONT_SMALL, COL_ACCENT);

    /* song name: biggest size that fits, else truncated with ".." */
    const char *song = g_data.autoplay_label ? g_data.autoplay_label : "";
    char buf[HDR_LABEL_MAX + 1];
    strncpy(buf, song, HDR_LABEL_MAX);
    buf[HDR_LABEL_MAX] = '\0';
    uint8_t sc = 3;
    while (sc > 2 && gfx_text_width(buf, sc) > LCD_WIDTH - 16) sc--;
    int maxch = (LCD_WIDTH - 16) / (6 * sc);
    if ((int)strlen(buf) > maxch && maxch > 2) {
        buf[maxch - 2] = '.'; buf[maxch - 1] = '.'; buf[maxch] = '\0';
    }
    int16_t sy = ty + 14 + (21 - gfx_text_height(sc)) / 2;
    gfx_text_aligned(c, 0, sy, LCD_WIDTH, ALIGN_CENTER, buf, sc, COL_TEXT);

    paint_np_wave(c, ctx);

    /* BPM under the wave */
    int16_t by = NP_WAVE_Y + NP_WAVE_H + 10;
    if (g_data.bpm_valid) {
        char b[24];
        char *e = put_uint(b, (uint32_t)clampf(g_data.bpm + 0.5f, 0, 999));
        strcpy(e, " BPM");
        if (g_data.lfo_division) { strcat(b, "  "); strcat(b, g_data.lfo_division); }
        gfx_text_aligned(c, 0, by, LCD_WIDTH, ALIGN_CENTER, b, FONT_MED, COL_ACCENT);
    }
    gfx_text_aligned(c, 0, LCD_HEIGHT - 12, LCD_WIDTH, ALIGN_CENTER,
                     "PLAY A KEY OR TURN A KNOB", FONT_SMALL, COL_TEXT_OFF);
}

static void np_enter(uint32_t now)
{
    ui_wave_fn fn = g_data.wave_sample;
    for (int i = 0; i < NP_PERIOD_W; i++) {
        float v = fn ? clampf(fn((float)i / (float)NP_PERIOD_W), -1.0f, 1.0f) : 0.0f;
        np_tab[i] = (int16_t)(NP_CY - v * (NP_WAVE_H / 2 - 4) + 0.5f);
    }
    g_np_start_ms = now;
    g_np_offset = 0;
    gfx_render(0, 0, LCD_WIDTH, LCD_HEIGHT, paint_np, NULL);
}

static void np_animate(uint32_t now)
{
    int16_t off = (int16_t)((now - g_np_start_ms) / 1000.0f * NP_SCROLL_PXS);
    off %= NP_PERIOD_W;
    if (off != g_np_offset) {
        g_np_offset = off;
        gfx_render(0, NP_WAVE_Y, LCD_WIDTH, NP_WAVE_H, paint_np_wave, NULL);
    }
}

/* ======================================================================== */
/*  idle dispatcher: which idle view to show                                */
/* ======================================================================== */
/*
 * Autoplay playing  -> NOW PLAYING view.   Nothing playing -> bouncing logo.
 * If autoplay starts/stops or the song changes while idle, the idle view
 * switches over without waking the main screen.
 */
typedef enum { IDLE_LOGO, IDLE_NOW_PLAYING } idle_mode_t;
static idle_mode_t g_idle_mode;
static const char *g_idle_label;

static void enter_idle(uint32_t now)
{
    g_idle_label = g_data.autoplay_label;
    if (autoplay_running()) { g_idle_mode = IDLE_NOW_PLAYING; np_enter(now); }
    else                    { g_idle_mode = IDLE_LOGO;        logo_enter(now); }
}

static void idle_animate(uint32_t now)
{
    if (g_data.autoplay_label != g_idle_label ||
        autoplay_running() != (g_idle_mode == IDLE_NOW_PLAYING)) {
        enter_idle(now);
        return;
    }
    if (g_idle_mode == IDLE_NOW_PLAYING) np_animate(now);
    else                                 logo_animate(now);
}

/* ======================================================================== */
/*  main screen                                                             */
/* ======================================================================== */

static void main_update(void)
{
    update_header();
    update_wave();
    update_info();
    update_env();
    update_lfo();
    update_led();
    update_filter();
    update_out();
    g_force = false;
}

static void enter_main(void)
{
    lcd_fill_screen(COL_BG);
    g_force = true;                      /* every panel repaints */
    main_update();
}

/* ======================================================================== */
/*  public API                                                              */
/* ======================================================================== */

typedef enum { SCR_BOOT, SCR_MAIN, SCR_IDLE } screen_t;
static screen_t g_screen;

void ui_init(void)
{
    lcd_init();
    g_screen = SCR_BOOT;
    g_boot_start_ms = ui_port_millis();
    gfx_render(0, 0, LCD_WIDTH, LCD_HEIGHT, paint_boot, NULL);
    g_last_frame_ms = g_boot_start_ms;
}

void ui_force_redraw(void)
{
    g_force = true;
}

void ui_task(void)
{
    uint32_t now = ui_port_millis();
    uint32_t frame = UI_FRAME_MS;
    if (g_screen == SCR_IDLE)
        frame = (g_idle_mode == IDLE_NOW_PLAYING) ? UI_NP_FRAME_MS : UI_IDLE_FRAME_MS;
    if (!g_force && (uint32_t)(now - g_last_frame_ms) < frame) return;
    g_last_frame_ms = now;

    g_data = ui_data;                    /* snapshot for this frame */

    switch (g_screen) {
    case SCR_BOOT:
        if ((uint32_t)(now - g_boot_start_ms) >= UI_BOOT_MS) {
            activity_rebase(now);        /* startup values aren't "activity" */
            g_activity_flag = false;
            g_screen = SCR_MAIN;
            enter_main();
        }
        break;

    case SCR_MAIN:
        activity_check(now, true);
        if ((uint32_t)(now - g_last_activity_ms) >= UI_IDLE_MS) {
            g_screen = SCR_IDLE;
            enter_idle(now);
        } else {
            main_update();
        }
        break;

    case SCR_IDLE:
        if (activity_check(now, false)) {
            g_screen = SCR_MAIN;
            enter_main();
        } else if (g_force) {
            g_force = false;
            enter_idle(now);
        } else {
            idle_animate(now);
        }
        break;
    }
}

#ifdef UI_HOST_SIM
int ui_debug_screen(void) { return (int)g_screen; }   /* sim tests only */
#endif
