/*
 * ui_layout.h  —  every position, size and colour of the screen.
 *
 * When the real panel arrives, tuning the look should only mean editing the
 * numbers in this file. Coordinates are in pixels, landscape 320 x 240,
 * (0,0) = top-left. A rect is X, Y, W, H.
 *
 *  +--------------------------------------------------------------+  y=0
 *  | [WAVEFORM]                         RISE 35%    FALL 65%      |  header
 *  +--------------------------------------------------------------+  y=20
 *  |                                                              |
 *  |         ~~~ global waveform, one period, CDJ style ~~~       |  wave
 *  |                                                              |
 *  +------------------------------+-------------------------------+  y=118
 *  | ENVELOPE                     | VOL LFO                    (o)|
 *  |   /\___                      |  DEPTH 0.50   RATE 12.5Hz     |  lfo
 *  |  /     \                     +-------------------------------+  y=180
 *  | ATTACK  DECAY                | OUTPUT                        |
 *  | SUSTAIN RELEASE              |   1.00x      [====|.......]   |  out
 *  +------------------------------+-------------------------------+  y=240
 */
#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include "gfx.h"

/* ---- colours (CDJ-ish: black, waveform blue, orange accents) ------------ */
#define COL_BG          RGB565(0x00, 0x00, 0x00)
#define COL_PANEL       RGB565(0x0C, 0x0E, 0x12)   /* panel background      */
#define COL_PANEL_EDGE  RGB565(0x30, 0x34, 0x3C)   /* 1px panel outline     */
#define COL_TITLE_BG    RGB565(0x1C, 0x20, 0x28)   /* panel title strip     */
#define COL_TEXT        RGB565(0xFF, 0xFF, 0xFF)
#define COL_TEXT_DIM    RGB565(0x8C, 0x94, 0xA0)   /* labels                */
#define COL_ACCENT      RGB565(0xFF, 0x8C, 0x00)   /* orange (CDJ cue)      */
#define COL_WAVE_FILL   RGB565(0x10, 0x3C, 0x8C)   /* dark blue body        */
#define COL_WAVE_LINE   RGB565(0x40, 0xA8, 0xFF)   /* bright blue outline   */
#define COL_WAVE_CORE   RGB565(0xD8, 0xEC, 0xFF)   /* near-white centre     */
#define COL_GRID        RGB565(0x20, 0x26, 0x30)
#define COL_ENV_LINE    RGB565(0x40, 0xA8, 0xFF)
#define COL_ENV_FILL    RGB565(0x0E, 0x28, 0x50)
#define COL_BAR_BG      RGB565(0x20, 0x24, 0x2C)
#define COL_BAR         RGB565(0x40, 0xA8, 0xFF)
#define COL_OVERDRIVE   RGB565(0xFF, 0x20, 0x10)   /* red box               */
#define COL_OD_TEXT     RGB565(0x00, 0x00, 0x00)   /* black text on red     */
#define COL_LED_OFF     RGB565(0x30, 0x18, 0x00)

/* ---- header ------------------------------------------------------------- */
#define HDR_X           0
#define HDR_Y           0
#define HDR_W           320
#define HDR_H           18

/* ---- waveform panel ----------------------------------------------------- */
#define WAVE_X          0
#define WAVE_Y          20
#define WAVE_W          320
#define WAVE_H          96
#define WAVE_PAD_X      6      /* inner margin left/right                  */
#define WAVE_PAD_Y      6      /* inner margin top/bottom                  */
#define WAVE_PERIODS    1      /* how many periods to draw across          */

/* ---- envelope (ADSR) panel ---------------------------------------------- */
#define ENV_X           0
#define ENV_Y           118
#define ENV_W           174
#define ENV_H           122
#define ENV_GRAPH_H     44     /* height of the envelope drawing            */

/* ---- volume LFO panel --------------------------------------------------- */
#define LFO_X           176
#define LFO_Y           118
#define LFO_W           144
#define LFO_H           60
#define LFO_LED_SIZE    8      /* blinking dot at the LFO rate              */

/* ---- output volume panel ------------------------------------------------ */
#define OUT_X           176
#define OUT_Y           180
#define OUT_W           144
#define OUT_H           60
#define OUT_MAX         10.0f  /* full-scale of the bar                     */

/* ---- shared ------------------------------------------------------------- */
#define TITLE_H         12     /* panel title strip height                  */
#define FONT_SMALL      1      /* 5x7  -> labels                            */
#define FONT_MED        2      /* 10x14 -> values                           */
#define FONT_BIG        3      /* 15x21 -> output volume                    */

/* ---- timing ------------------------------------------------------------- */
#define UI_FRAME_MS     33     /* ~30 fps max; only changed panels redraw   */

#endif /* UI_LAYOUT_H */
