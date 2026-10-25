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

#endif /* UI_H */
