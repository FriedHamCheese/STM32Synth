/*
 * ui.h  —  synth status screen.
 *
 * Usage (main.c, after MX_SPIx_Init()):
 *     ui_init();
 *     while (1) {
 *         ...
 *         ui_task();      // cheap when nothing changed; call as often as you like
 *     }
 */
#ifndef UI_H
#define UI_H

void ui_init(void);          /* init LCD, clear, draw everything once        */
void ui_task(void);          /* read values, redraw only panels that changed */
void ui_force_redraw(void);  /* repaint everything on the next ui_task()     */

/*
 * Screens: BOOT (from ui_init, UI_BOOT_MS) -> MAIN -> IDLE after UI_IDLE_MS
 * without activity -> back to MAIN on any activity.
 *
 * Two idle views: autoplay playing -> NOW PLAYING + song + scrolling wave;
 * nothing playing -> DVD-style bouncing logo. Autoplay alone does not keep
 * the screen awake.
 *
 * Activity is detected automatically when a knob value shown on screen moves
 * (with a dead-band so ADC noise doesn't count) and when the autoplay song
 * changes on the main screen. Things the screen can't see -
 * key presses, button presses - are reported with ui_notify_activity().
 * It only sets a flag, so it's safe to call from an interrupt (note_on etc.).
 */
void ui_notify_activity(void);

#endif /* UI_H */
