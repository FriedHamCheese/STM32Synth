/*
 * lcd.c  —  ILI9341 / ST7789V driver for STM32 HAL (SPI, 8-bit, 4-wire).
 *
 * Protocol recap: CS low selects the chip. DC low = the byte is a command,
 * DC high = the bytes are parameters / pixel data. To draw:
 *   CASET (0x2A) x0,x1  ->  RASET (0x2B) y0,y1  ->  RAMWR (0x2C) pixels...
 * The controller auto-increments through the window, so a rectangle of pixels
 * is just one long byte stream.
 */
#include "lcd.h"
#include "main.h"            /* CubeMX: HAL + pin defines (LCD_CS_Pin etc.) */

extern SPI_HandleTypeDef hspi2;
#define LCD_SPI (&hspi2)

/* ---- pin helpers ------------------------------------------------------- */
#define CS_LOW()    HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_RESET)
#define CS_HIGH()   HAL_GPIO_WritePin(LCD_CS_PORT, LCD_CS_PIN, GPIO_PIN_SET)
#define DC_CMD()    HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define DC_DATA()   HAL_GPIO_WritePin(LCD_DC_PORT, LCD_DC_PIN, GPIO_PIN_SET)

#define SPI_TIMEOUT_MS 100

/* Wait until the SPI peripheral has finished shifting out the last byte.
 * Needed before toggling DC/CS, otherwise the tail of a transfer gets
 * interpreted with the wrong DC level. */
static void spi_wait_idle(void)
{
    while (HAL_SPI_GetState(LCD_SPI) != HAL_SPI_STATE_READY) { }
    while (__HAL_SPI_GET_FLAG(LCD_SPI, SPI_FLAG_BSY)) { }
}

static void spi_tx(const uint8_t *data, uint16_t len)
{
    HAL_SPI_Transmit(LCD_SPI, (uint8_t *)data, len, SPI_TIMEOUT_MS);
}

/* Send one command byte followed by `n` parameter bytes. */
static void lcd_cmd(uint8_t cmd, const uint8_t *params, uint16_t n)
{
    spi_wait_idle();
    CS_LOW();
    DC_CMD();
    spi_tx(&cmd, 1);
    if (n) {
        DC_DATA();
        spi_tx(params, n);
    }
    spi_wait_idle();
    CS_HIGH();
}

/* ---- init sequences ----------------------------------------------------
 * Table format: cmd, nparams | DELAY_FLAG, params..., [delay_ms]
 * Terminated by cmd 0x00 with count 0.
 */
#define DELAY_FLAG 0x80

#if LCD_CONTROLLER == LCD_CTRL_ILI9341
static const uint8_t init_seq[] = {
    0x01, 0 | DELAY_FLAG, 150,                       /* SWRESET              */
    0xEF, 3, 0x03, 0x80, 0x02,                       /* (undocumented, std)  */
    0xCF, 3, 0x00, 0xC1, 0x30,                       /* Power control B      */
    0xED, 4, 0x64, 0x03, 0x12, 0x81,                 /* Power on seq ctrl    */
    0xE8, 3, 0x85, 0x00, 0x78,                       /* Driver timing ctrl A */
    0xCB, 5, 0x39, 0x2C, 0x00, 0x34, 0x02,           /* Power control A      */
    0xF7, 1, 0x20,                                   /* Pump ratio control   */
    0xEA, 2, 0x00, 0x00,                             /* Driver timing ctrl B */
    0xC0, 1, 0x23,                                   /* PWCTR1 VRH=4.60V     */
    0xC1, 1, 0x10,                                   /* PWCTR2               */
    0xC5, 2, 0x3E, 0x28,                             /* VMCTR1               */
    0xC7, 1, 0x86,                                   /* VMCTR2               */
    0x36, 1, LCD_MADCTL,                             /* MADCTL rotation      */
    0x37, 1, 0x00,                                   /* VSCRSADD             */
    0x3A, 1, 0x55,                                   /* COLMOD 16-bit RGB565 */
    0xB1, 2, 0x00, 0x18,                             /* FRMCTR1 79 Hz        */
    0xB6, 3, 0x08, 0x82, 0x27,                       /* DFUNCTR              */
    0xF2, 1, 0x00,                                   /* 3-gamma off          */
    0x26, 1, 0x01,                                   /* Gamma curve 1        */
    0xE0, 15, 0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
              0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00,  /* +gamma          */
    0xE1, 15, 0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
              0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F,  /* -gamma          */
    LCD_INVERT ? 0x21 : 0x20, 0,                     /* INVON / INVOFF       */
    0x11, 0 | DELAY_FLAG, 120,                       /* SLPOUT               */
    0x29, 0 | DELAY_FLAG, 20,                        /* DISPON               */
    0x00, 0
};
#elif LCD_CONTROLLER == LCD_CTRL_ST7789V
static const uint8_t init_seq[] = {
    0x01, 0 | DELAY_FLAG, 150,                       /* SWRESET              */
    0x11, 0 | DELAY_FLAG, 120,                       /* SLPOUT               */
    0x3A, 1 | DELAY_FLAG, 0x55, 10,                  /* COLMOD 16-bit RGB565 */
    0x36, 1, LCD_MADCTL,                             /* MADCTL rotation      */
    0xB2, 5, 0x0C, 0x0C, 0x00, 0x33, 0x33,           /* PORCTRL              */
    0xB7, 1, 0x35,                                   /* GCTRL                */
    0xBB, 1, 0x19,                                   /* VCOMS                */
    0xC0, 1, 0x2C,                                   /* LCMCTRL              */
    0xC2, 1, 0x01,                                   /* VDVVRHEN             */
    0xC3, 1, 0x12,                                   /* VRHS                 */
    0xC4, 1, 0x20,                                   /* VDVS                 */
    0xC6, 1, 0x0F,                                   /* FRCTRL2 60 Hz        */
    0xD0, 2, 0xA4, 0xA1,                             /* PWCTRL1              */
    0xE0, 14, 0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
              0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23,  /* +gamma          */
    0xE1, 14, 0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
              0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23,  /* -gamma          */
    LCD_INVERT ? 0x21 : 0x20, 0,                     /* INVON / INVOFF       */
    0x13, 0 | DELAY_FLAG, 10,                        /* NORON                */
    0x29, 0 | DELAY_FLAG, 20,                        /* DISPON               */
    0x00, 0
};
#else
#error "LCD_CONTROLLER must be LCD_CTRL_ILI9341 or LCD_CTRL_ST7789V"
#endif

static void run_init_seq(const uint8_t *p)
{
    for (;;) {
        uint8_t cmd   = *p++;
        uint8_t count = *p++;
        if (cmd == 0x00 && count == 0) break;
        uint8_t n = count & ~DELAY_FLAG;
        lcd_cmd(cmd, p, n);
        p += n;
        if (count & DELAY_FLAG) HAL_Delay(*p++);
    }
}

/* ---- public API -------------------------------------------------------- */

void lcd_backlight(bool on)
{
#if LCD_HAS_BL
    HAL_GPIO_WritePin(LCD_BL_PORT, LCD_BL_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    (void)on;
#endif
}

void lcd_init(void)
{
    CS_HIGH();
    DC_DATA();
    lcd_backlight(false);

#if LCD_HAS_RST
    /* Hardware reset pulse */
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(LCD_RST_PORT, LCD_RST_PIN, GPIO_PIN_SET);
    HAL_Delay(150);
#endif

    run_init_seq(init_seq);
    lcd_fill_screen(0x0000);
    lcd_backlight(true);
}

void lcd_write_begin(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
    uint16_t x0 = x + LCD_X_OFFSET, x1 = x0 + w - 1;
    uint16_t y0 = y + LCD_Y_OFFSET, y1 = y0 + h - 1;
    uint8_t ca[4] = { x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t ra[4] = { y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF };

    lcd_cmd(0x2A, ca, 4);              /* CASET */
    lcd_cmd(0x2B, ra, 4);              /* RASET */

    /* RAMWR, then leave CS low with DC=data for the pixel stream */
    uint8_t ramwr = 0x2C;
    spi_wait_idle();
    CS_LOW();
    DC_CMD();
    spi_tx(&ramwr, 1);
    spi_wait_idle();
    DC_DATA();
}

void lcd_write_pixels(const uint8_t *buf, uint32_t nbytes)
{
    while (nbytes) {
        /* HAL transfer length is 16-bit; split big writes */
        uint16_t chunk = nbytes > 0xFFF0u ? 0xFFF0u : (uint16_t)nbytes;
#if LCD_USE_DMA
        spi_wait_idle();               /* previous DMA must be finished */
        HAL_SPI_Transmit_DMA(LCD_SPI, (uint8_t *)buf, chunk);
#else
        spi_tx(buf, chunk);
#endif
        buf    += chunk;
        nbytes -= chunk;
    }
}

void lcd_write_end(void)
{
    spi_wait_idle();
    CS_HIGH();
}

void lcd_fill_screen(uint16_t rgb565)
{
    /* Small static strip of one colour, streamed repeatedly. */
    static uint8_t strip[LCD_WIDTH * 2];
    for (int i = 0; i < LCD_WIDTH; i++) {
        strip[2 * i]     = rgb565 >> 8;
        strip[2 * i + 1] = rgb565 & 0xFF;
    }
    lcd_write_begin(0, 0, LCD_WIDTH, LCD_HEIGHT);
    for (int y = 0; y < LCD_HEIGHT; y++) {
        lcd_write_pixels(strip, sizeof strip);
    }
    lcd_write_end();
}
