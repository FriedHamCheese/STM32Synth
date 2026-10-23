/*
 * ui_layout.h  —  every position, size and colour of the screen.
 *
 * Tuning the look should only mean editing the numbers in this file.
 * Pixels, landscape 320 x 240, (0,0) = top-left. A rect is X, Y, W, H.
 *
 *  +--------------------------------------------------------------+ y=0
 *  |[AUTO] > Cryin' Through The Rain                              | header
 *  +--------------------------------------------------------------+ y=20
 *  |         ~~~ global waveform, one period, CDJ style ~~~       | wave
 *  +--------------------------------------------------------------+ y=98
 *  | RISE 30%     FALL 70%           CENTER OSC 4.20Hz            | info
 *  +-----------------------------+--------------------------------+ y=122
 *  | ENVELOPE                    | LFO DEPTH   LFO RATE        (o)| lfo
 *  |   /\___                     +--------------------------------+ y=158
 *  |  /     \                    | LOW PASS    ENV AMT            | filter
 *  | ATTACK   DECAY              +--------------------------------+ y=194
 *  | SUSTAIN  RELEASE            | OUTPUT [ 1.29x ] +2.2dB [==|=] | out
 *  +-----------------------------+--------------------------------+ y=240
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
#define COL_TEXT_OFF    RGB565(0x50, 0x56, 0x60)   /* "OFF" / "--"          */
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

/* ---- header: autoplay song ---------------------------------------------- */
#define HDR_X           0
#define HDR_Y           0
#define HDR_W           320
#define HDR_H           18
#define HDR_LABEL_MAX   48     /* chars of the song name considered         */

/* ---- waveform ----------------------------------------------------------- */
#define WAVE_X          0
#define WAVE_Y          20
#define WAVE_W          320
#define WAVE_H          76
#define WAVE_PAD_X      6      /* inner margin left/right                  */
#define WAVE_PAD_Y      5      /* inner margin top/bottom                  */
#define WAVE_PERIODS    1      /* how many periods to draw across          */

/* ---- waveform info strip: rise / fall / centre osc ---------------------- */
#define INFO_X          0
#define INFO_Y          98
#define INFO_W          320
#define INFO_H          22
#define INFO_FALL_X     96     /* x offsets of the three fields             */
#define INFO_OSC_X      192

/* ---- envelope (ADSR), left column --------------------------------------- */
#define ENV_X           0
#define ENV_Y           122
#define ENV_W           160
#define ENV_H           118
#define ENV_GRAPH_H     34     /* height of the envelope drawing            */

/* ---- right column: three stacked rows ----------------------------------- */
#define COL2_X          162
#define COL2_W          158
#define COL2_C2         86     /* x offset of the 2nd value column          */

#define LFO_X           COL2_X
#define LFO_Y           122
#define LFO_W           COL2_W
#define LFO_H           34
#define LFO_LED_SIZE    7      /* blinking dot at the LFO rate              */

#define FLT_X           COL2_X
#define FLT_Y           158
#define FLT_W           COL2_W
#define FLT_H           34

#define OUT_X           COL2_X
#define OUT_Y           194
#define OUT_W           COL2_W
#define OUT_H           46
#define OUT_MAX         10.0f  /* full-scale of the bar                     */

/* ---- shared ------------------------------------------------------------- */
#define TITLE_H         12     /* panel title strip height                  */
#define FONT_SMALL      1      /* 5x7  -> labels                            */
#define FONT_MED        2      /* 10x14 -> values                           */
#define FONT_BIG        3      /* 15x21 -> output volume                    */

/* ---- timing ------------------------------------------------------------- */
#define UI_FRAME_MS     33     /* ~30 fps max; only changed panels redraw   */

#endif /* UI_LAYOUT_H */
