/*
 * lcd_config.h  —  everything hardware-specific about the TFT lives here.
 *
 * Panel: 2.4"/2.8" 240x320 SPI TFT, sold as "ILI9341 / ST7789V".
 * The two controllers speak almost the same command set (CASET/RASET/RAMWR
 * are identical); only the power-up sequence and a few defaults differ.
 * Pick one with LCD_CONTROLLER below. If the screen stays white/black or the
 * colours look inverted, try the other one / flip LCD_INVERT / LCD_BGR.
 *
 * Wiring (fill in to match CubeMX — names are the "User Label" you give the
 * pins in CubeMX, which generates FOO_Pin / FOO_GPIO_Port in main.h):
 *
 *   TFT pin   STM32
 *   -------   -----------------------------------------------
 *   SCK       SPIx_SCK   (CubeMX: SPI "Transmit Only Master", 8-bit,
 *   MOSI/SDA  SPIx_MOSI   CPOL low, CPHA 1 edge, MSB first, NSS software)
 *   MISO      optional, not used
 *   CS        GPIO out   -> LCD_CS
 *   DC / RS   GPIO out   -> LCD_DC
 *   RESET     GPIO out   -> LCD_RST   (or tie to 3V3 and set LCD_HAS_RST 0)
 *   LED / BL  GPIO out   -> LCD_BL    (or tie to 3V3 and set LCD_HAS_BL 0)
 *   T_*       touch (XPT2046) — not used by this code yet
 *
 * Speed: ILI9341 is rated 10 MHz write but almost all modules run fine at
 * 21-42 MHz. On F4 SPI1 sits on APB2 (84 MHz) -> prescaler 2 = 42 MHz,
 * prescaler 4 = 21 MHz. Start at 21 MHz, go up if the picture is clean.
 */
#ifndef LCD_CONFIG_H
#define LCD_CONFIG_H

#include "main.h"

/* ---- controller -------------------------------------------------------- */
#define LCD_CTRL_ILI9341   1
#define LCD_CTRL_ST7789V   2

#ifndef LCD_CONTROLLER
#define LCD_CONTROLLER     LCD_CTRL_ILI9341
#endif

/* ---- geometry (after rotation) — the UI is designed for landscape ------ */
#define LCD_WIDTH          320
#define LCD_HEIGHT         240

/*
 * MADCTL (0x36) value = rotation + colour order. Common landscape values:
 *   ILI9341 : 0x28 (MV|BGR)        upside down: 0xE8 (MY|MX|MV|BGR)
 *   ST7789V : 0x60 (MX|MV, RGB)    upside down: 0xA0 (MY|MV, RGB)
 * Bits: 0x80 MY, 0x40 MX, 0x20 MV(swap XY), 0x08 BGR.
 * If the image is mirrored, toggle MX (0x40) or MY (0x80).
 */
#ifndef LCD_MADCTL
#  if LCD_CONTROLLER == LCD_CTRL_ILI9341
#    define LCD_MADCTL     0x28
#  else
#    define LCD_MADCTL     0x60
#  endif
#endif

/* Colour inversion: most ST7789V IPS panels need it ON, ILI9341 needs OFF. */
#ifndef LCD_INVERT
#  if LCD_CONTROLLER == LCD_CTRL_ILI9341
#    define LCD_INVERT     0
#  else
#    define LCD_INVERT     1
#  endif
#endif

/* Pixel offset of the visible area inside controller RAM (0 for 240x320). */
#define LCD_X_OFFSET       0
#define LCD_Y_OFFSET       0

/* ---- STM32 plumbing (only used by lcd.c, not by the host simulator) ---- */
#define LCD_SPI_HANDLE     hspi1          /* SPI_HandleTypeDef from CubeMX */

#define LCD_HAS_RST        1
#define LCD_HAS_BL         0

/* Use HAL_SPI_Transmit_DMA for pixel data (enable SPIx_TX DMA in CubeMX,
 * Normal mode, byte width, memory increment). 0 = plain blocking transmit. */
#define LCD_USE_DMA        0

/* Pin names. CubeMX generates these in main.h from the pin User Labels.
 * If you label the pins differently, change the right-hand side here. */
#define LCD_CS_PORT        LCD_CS_GPIO_Port
#define LCD_CS_PIN         LCD_CS_Pin
#define LCD_DC_PORT        LCD_DC_GPIO_Port
#define LCD_DC_PIN         LCD_DC_Pin
#define LCD_RST_PORT       LCD_RST_GPIO_Port
#define LCD_RST_PIN        LCD_RST_Pin
/*
Nah we just give 3.3v to LED pin
#define LCD_BL_PORT        LCD_BL_GPIO_Port
#define LCD_BL_PIN         LCD_BL_Pin
*/
#endif /* LCD_CONFIG_H */
