/*
 * ui.h  —  synth status screen.
 *
 * Usage (main.c, after MX_SPIx_Init()):
 *     ui_init(&ui_data);
 *     while (1) {
 *         ...
 *         ui_task(&ui_data); // cheap when nothing changed; call as often as you like
 *     }
 */
#ifndef UI_H
#define UI_H

#include "ui_data.h"

void ui_init(const ui_data_t *data);  /* init LCD and draw the initial snapshot */
void ui_task(const ui_data_t *data);  /* redraw using the caller's snapshot    */
void ui_force_redraw(void);  /* repaint everything on the next ui_task()     */

#endif /* UI_H */
