/*
 * lcd.h  —  minimal ILI9341 / ST7789V SPI driver.
 *
 * The driver only knows how to: initialise the panel, open a rectangular
 * "window" in display RAM, and stream RGB565 pixels into it. All drawing
 * (text, lines, waveform...) happens in gfx.c into a small RAM strip which is
 * then streamed here. This keeps RAM use tiny (no 150 KB framebuffer, which an
 * F4 doesn't have to spare).
 *
 * Pixel data format: RGB565, big-endian byte order (high byte first), which is
 * what the controller expects on an 8-bit SPI bus. gfx.c already stores pixels
 * that way, so buffers can be sent as-is (and by DMA).
 */
#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include <stdbool.h>
#include "lcd_config.h"

void lcd_init(void);
void lcd_backlight(bool on);

/* Fill the whole screen with one colour (normal RGB565, not byte-swapped). */
void lcd_fill_screen(uint16_t rgb565);

/*
 * Streaming write:
 *   lcd_write_begin(x, y, w, h);         // opens window, CS low
 *   lcd_write_pixels(buf, nbytes);       // any number of times, w*h*2 total
 *   lcd_write_end();                     // waits for DMA, CS high
 *
 * With LCD_USE_DMA=1, lcd_write_pixels() returns as soon as the transfer has
 * started, so the caller must not touch `buf` until the NEXT
 * lcd_write_pixels()/lcd_write_end() call returns (use two buffers and
 * alternate — ui.c does this).
 */
void lcd_write_begin(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void lcd_write_pixels(const uint8_t *buf, uint32_t nbytes);
void lcd_write_end(void);

#endif /* LCD_H */
