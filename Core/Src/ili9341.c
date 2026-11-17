/**
  ******************************************************************************
  * @file    ili9341.c
  * @brief   ILI9341 2.8" TFT display driver for STM32 HAL (SPI1).
  *          Complies with shared SPI1 bus rules (TFT_CS asserted only during
  *          active transmission, deasserted immediately afterward).
  ******************************************************************************
  */

#include "ili9341.h"
#include "main.h"
#include "app_config.h"
#include <string.h>

/* ════════════════════════════════════════════════════════════════════════════
 *  GPIO Pin Macros (using CubeMX User Labels)
 * ════════════════════════════════════════════════════════════════════════════ */

#define TFT_CS_LOW()    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET)
#define TFT_CS_HIGH()   HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET)

#define TFT_DC_CMD()    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET)
#define TFT_DC_DATA()   HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET)

#define TFT_RST_LOW()   HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_RESET)
#define TFT_RST_HIGH()  HAL_GPIO_WritePin(TFT_RST_GPIO_Port, TFT_RST_Pin, GPIO_PIN_SET)

/* ════════════════════════════════════════════════════════════════════════════
 *  Standard 5x7 ASCII Font Table (characters 32 to 126)
 * ════════════════════════════════════════════════════════════════════════════ */

static const uint8_t font5x7[95][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, /*   32 Space */
    {0x00, 0x00, 0x5F, 0x00, 0x00}, /* ! 33 */
    {0x00, 0x07, 0x00, 0x07, 0x00}, /* " 34 */
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, /* # 35 */
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, /* $ 36 */
    {0x23, 0x13, 0x08, 0x64, 0x62}, /* % 37 */
    {0x36, 0x49, 0x55, 0x22, 0x50}, /* & 38 */
    {0x00, 0x05, 0x03, 0x00, 0x00}, /* ' 39 */
    {0x00, 0x1C, 0x22, 0x41, 0x00}, /* ( 40 */
    {0x00, 0x41, 0x22, 0x1C, 0x00}, /* ) 41 */
    {0x08, 0x2A, 0x1C, 0x2A, 0x08}, /* * 42 */
    {0x08, 0x08, 0x3E, 0x08, 0x08}, /* + 43 */
    {0x00, 0x50, 0x30, 0x00, 0x00}, /* , 44 */
    {0x08, 0x08, 0x08, 0x08, 0x08}, /* - 45 */
    {0x00, 0x60, 0x60, 0x00, 0x00}, /* . 46 */
    {0x20, 0x10, 0x08, 0x04, 0x02}, /* / 47 */
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, /* 0 48 */
    {0x00, 0x42, 0x7F, 0x40, 0x00}, /* 1 49 */
    {0x42, 0x61, 0x51, 0x49, 0x46}, /* 2 50 */
    {0x21, 0x41, 0x45, 0x4B, 0x31}, /* 3 51 */
    {0x18, 0x14, 0x12, 0x7F, 0x10}, /* 4 52 */
    {0x27, 0x45, 0x45, 0x45, 0x39}, /* 5 53 */
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, /* 6 54 */
    {0x01, 0x71, 0x09, 0x05, 0x03}, /* 7 55 */
    {0x36, 0x49, 0x49, 0x49, 0x36}, /* 8 56 */
    {0x06, 0x49, 0x49, 0x29, 0x1E}, /* 9 57 */
    {0x00, 0x36, 0x36, 0x00, 0x00}, /* : 58 */
    {0x00, 0x56, 0x36, 0x00, 0x00}, /* ; 59 */
    {0x00, 0x08, 0x14, 0x22, 0x41}, /* < 60 */
    {0x14, 0x14, 0x14, 0x14, 0x14}, /* = 61 */
    {0x41, 0x22, 0x14, 0x08, 0x00}, /* > 62 */
    {0x02, 0x01, 0x51, 0x09, 0x06}, /* ? 63 */
    {0x32, 0x49, 0x79, 0x41, 0x3E}, /* @ 64 */
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, /* A 65 */
    {0x7F, 0x49, 0x49, 0x49, 0x36}, /* B 66 */
    {0x3E, 0x41, 0x41, 0x41, 0x22}, /* C 67 */
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, /* D 68 */
    {0x7F, 0x49, 0x49, 0x49, 0x41}, /* E 69 */
    {0x7F, 0x09, 0x09, 0x01, 0x01}, /* F 70 */
    {0x3E, 0x41, 0x41, 0x51, 0x32}, /* G 71 */
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, /* H 72 */
    {0x00, 0x41, 0x7F, 0x41, 0x00}, /* I 73 */
    {0x20, 0x40, 0x41, 0x3F, 0x01}, /* J 74 */
    {0x7F, 0x08, 0x14, 0x22, 0x41}, /* K 75 */
    {0x7F, 0x40, 0x40, 0x40, 0x40}, /* L 76 */
    {0x7F, 0x02, 0x04, 0x02, 0x7F}, /* M 77 */
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, /* N 78 */
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, /* O 79 */
    {0x7F, 0x09, 0x09, 0x09, 0x06}, /* P 80 */
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, /* Q 81 */
    {0x7F, 0x09, 0x19, 0x29, 0x46}, /* R 82 */
    {0x46, 0x49, 0x49, 0x49, 0x31}, /* S 83 */
    {0x01, 0x01, 0x7F, 0x01, 0x01}, /* T 84 */
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, /* U 85 */
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, /* V 86 */
    {0x7F, 0x20, 0x18, 0x20, 0x7F}, /* W 87 */
    {0x63, 0x14, 0x08, 0x14, 0x63}, /* X 88 */
    {0x03, 0x04, 0x78, 0x04, 0x03}, /* Y 89 */
    {0x61, 0x51, 0x49, 0x45, 0x43}, /* Z 90 */
    {0x00, 0x00, 0x7F, 0x41, 0x41}, /* [ 91 */
    {0x02, 0x04, 0x08, 0x10, 0x20}, /* \ 92 */
    {0x41, 0x41, 0x7F, 0x00, 0x00}, /* ] 93 */
    {0x04, 0x02, 0x01, 0x02, 0x04}, /* ^ 94 */
    {0x40, 0x40, 0x40, 0x40, 0x40}, /* _ 95 */
    {0x00, 0x01, 0x02, 0x04, 0x00}, /* ` 96 */
    {0x20, 0x54, 0x54, 0x54, 0x78}, /* a 97 */
    {0x7F, 0x48, 0x44, 0x44, 0x38}, /* b 98 */
    {0x38, 0x44, 0x44, 0x44, 0x20}, /* c 99 */
    {0x38, 0x44, 0x44, 0x48, 0x7F}, /* d 100 */
    {0x38, 0x54, 0x54, 0x54, 0x18}, /* e 101 */
    {0x08, 0x7E, 0x09, 0x01, 0x02}, /* f 102 */
    {0x08, 0x14, 0x54, 0x54, 0x3C}, /* g 103 */
    {0x7F, 0x08, 0x04, 0x04, 0x78}, /* h 104 */
    {0x00, 0x44, 0x7D, 0x40, 0x00}, /* i 105 */
    {0x20, 0x40, 0x44, 0x3D, 0x00}, /* j 106 */
    {0x00, 0x7F, 0x10, 0x28, 0x44}, /* k 107 */
    {0x00, 0x41, 0x7F, 0x40, 0x00}, /* l 108 */
    {0x7C, 0x04, 0x18, 0x04, 0x78}, /* m 109 */
    {0x7C, 0x08, 0x04, 0x04, 0x78}, /* n 110 */
    {0x38, 0x44, 0x44, 0x44, 0x38}, /* o 111 */
    {0x7C, 0x14, 0x14, 0x14, 0x08}, /* p 112 */
    {0x08, 0x14, 0x14, 0x18, 0x7C}, /* q 113 */
    {0x7C, 0x08, 0x04, 0x04, 0x08}, /* r 114 */
    {0x48, 0x54, 0x54, 0x54, 0x20}, /* s 115 */
    {0x04, 0x3F, 0x44, 0x40, 0x20}, /* t 116 */
    {0x3C, 0x40, 0x40, 0x20, 0x7C}, /* u 117 */
    {0x1C, 0x20, 0x40, 0x20, 0x1C}, /* v 118 */
    {0x3C, 0x40, 0x30, 0x40, 0x3C}, /* w 119 */
    {0x44, 0x28, 0x10, 0x28, 0x44}, /* x 120 */
    {0x0C, 0x50, 0x50, 0x50, 0x3C}, /* y 121 */
    {0x44, 0x64, 0x54, 0x4C, 0x44}, /* z 122 */
    {0x00, 0x08, 0x36, 0x41, 0x00}, /* { 123 */
    {0x00, 0x00, 0x7F, 0x00, 0x00}, /* | 124 */
    {0x00, 0x41, 0x36, 0x08, 0x00}, /* } 125 */
    {0x08, 0x08, 0x2A, 0x1C, 0x08}  /* ~ 126 */
};

/* ════════════════════════════════════════════════════════════════════════════
 *  Private SPI Helper Functions
 * ════════════════════════════════════════════════════════════════════════════ */

static void ili_write_cmd(const ILI9341_Dev *dev, uint8_t cmd)
{
    TFT_DC_CMD();
    TFT_CS_LOW();
    HAL_SPI_Transmit(dev->hspi, &cmd, 1, HAL_MAX_DELAY);
    TFT_CS_HIGH();
}

static void ili_write_data(const ILI9341_Dev *dev, const uint8_t *data, uint16_t len)
{
    TFT_DC_DATA();
    TFT_CS_LOW();
    HAL_SPI_Transmit(dev->hspi, (uint8_t *)data, len, HAL_MAX_DELAY);
    TFT_CS_HIGH();
}

static void ili_write_data8(const ILI9341_Dev *dev, uint8_t data)
{
    ili_write_data(dev, &data, 1);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Public API Implementation
 * ════════════════════════════════════════════════════════════════════════════ */

void ILI9341_Init(ILI9341_Dev *dev, SPI_HandleTypeDef *hspi,
                  TIM_HandleTypeDef *htim_pwm, uint32_t pwm_channel)
{
    dev->hspi        = hspi;
    dev->htim_pwm    = htim_pwm;
    dev->pwm_channel = pwm_channel;
    dev->width       = ILI9341_WIDTH;
    dev->height      = ILI9341_HEIGHT;
    dev->rotation    = ILI9341_ROTATION_PORTRAIT;

    /* 1. Ensure Chip Selects are HIGH initially */
    TFT_CS_HIGH();

    /* 2. Hardware Reset */
    TFT_RST_HIGH();
    HAL_Delay(10);
    TFT_RST_LOW();
    HAL_Delay(20);
    TFT_RST_HIGH();
    HAL_Delay(120);

    /* 3. Software Reset */
    ili_write_cmd(dev, ILI9341_CMD_SWRESET);
    HAL_Delay(120);

    /* 4. Power Control A */
    ili_write_cmd(dev, 0xCB);
    uint8_t pctrl_a[] = {0x39, 0x2C, 0x00, 0x34, 0x02};
    ili_write_data(dev, pctrl_a, sizeof(pctrl_a));

    /* 5. Power Control B */
    ili_write_cmd(dev, 0xCF);
    uint8_t pctrl_b[] = {0x00, 0xC1, 0x30};
    ili_write_data(dev, pctrl_b, sizeof(pctrl_b));

    /* 6. Driver Timing Control A */
    ili_write_cmd(dev, 0xE8);
    uint8_t dtctrl_a[] = {0x85, 0x00, 0x78};
    ili_write_data(dev, dtctrl_a, sizeof(dtctrl_a));

    /* 7. Driver Timing Control B */
    ili_write_cmd(dev, 0xEA);
    uint8_t dtctrl_b[] = {0x00, 0x00};
    ili_write_data(dev, dtctrl_b, sizeof(dtctrl_b));

    /* 8. Power On Sequence Control */
    ili_write_cmd(dev, 0xED);
    uint8_t posctrl[] = {0x64, 0x03, 0x12, 0x81};
    ili_write_data(dev, posctrl, sizeof(posctrl));

    /* 9. Pump Ratio Control */
    ili_write_cmd(dev, 0xF7);
    ili_write_data8(dev, 0x20);

    /* 10. Power Control 1 */
    ili_write_cmd(dev, 0xC0);
    ili_write_data8(dev, 0x23);

    /* 11. Power Control 2 */
    ili_write_cmd(dev, 0xC1);
    ili_write_data8(dev, 0x10);

    /* 12. VCOM Control 1 */
    ili_write_cmd(dev, 0xC5);
    uint8_t vcom1[] = {0x3E, 0x28};
    ili_write_data(dev, vcom1, sizeof(vcom1));

    /* 13. VCOM Control 2 */
    ili_write_cmd(dev, 0xC7);
    ili_write_data8(dev, 0x86);

    /* 14. Pixel Format Set: 16-bit RGB565 */
    ili_write_cmd(dev, ILI9341_CMD_PIXFMT);
    ili_write_data8(dev, 0x55);

    /* 15. Frame Rate Control (70 Hz) */
    ili_write_cmd(dev, 0xB1);
    uint8_t frate[] = {0x00, 0x18};
    ili_write_data(dev, frate, sizeof(frate));

    /* 16. Display Function Control */
    ili_write_cmd(dev, 0xB6);
    uint8_t dfctrl[] = {0x08, 0x82, 0x27};
    ili_write_data(dev, dfctrl, sizeof(dfctrl));

    /* 17. 3Gamma Enable: Disabled */
    ili_write_cmd(dev, 0xF2);
    ili_write_data8(dev, 0x00);

    /* 18. Gamma Set */
    ili_write_cmd(dev, 0x26);
    ili_write_data8(dev, 0x01);

    /* 19. Positive Gamma Correction */
    ili_write_cmd(dev, 0xE0);
    uint8_t pos_gamma[] = {
        0x0F, 0x31, 0x2B, 0x0C, 0x0E, 0x08, 0x4E, 0xF1,
        0x37, 0x07, 0x10, 0x03, 0x0E, 0x09, 0x00
    };
    ili_write_data(dev, pos_gamma, sizeof(pos_gamma));

    /* 20. Negative Gamma Correction */
    ili_write_cmd(dev, 0xE1);
    uint8_t neg_gamma[] = {
        0x00, 0x0E, 0x14, 0x03, 0x11, 0x07, 0x31, 0xC1,
        0x48, 0x08, 0x0F, 0x0C, 0x31, 0x36, 0x0F
    };
    ili_write_data(dev, neg_gamma, sizeof(neg_gamma));

    /* 21. Set Initial Orientation */
    ILI9341_SetRotation(dev, ILI9341_ROTATION_PORTRAIT);

    /* 22. Exit Sleep */
    ili_write_cmd(dev, ILI9341_CMD_SLPOUT);
    HAL_Delay(120);

    /* 23. Display ON */
    ili_write_cmd(dev, ILI9341_CMD_DISPON);
    HAL_Delay(20);

    /* 24. Clear screen to black */
    ILI9341_FillScreen(dev, ILI9341_BLACK);

    /* 25. Start Backlight PWM */
    if (dev->htim_pwm != NULL) {
        HAL_TIM_PWM_Start(dev->htim_pwm, dev->pwm_channel);
        ILI9341_SetBacklight(dev, TFT_BLK_DEFAULT);
    }
}

void ILI9341_SetRotation(ILI9341_Dev *dev, ILI9341_Rotation rotation)
{
    dev->rotation = rotation;
    uint8_t madctl = 0;

    switch (rotation) {
        case ILI9341_ROTATION_PORTRAIT:
            madctl = ILI9341_MADCTL_MX | ILI9341_MADCTL_BGR;
            dev->width  = ILI9341_WIDTH;
            dev->height = ILI9341_HEIGHT;
            break;

        case ILI9341_ROTATION_LANDSCAPE:
            madctl = ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR;
            dev->width  = ILI9341_HEIGHT;
            dev->height = ILI9341_WIDTH;
            break;

        case ILI9341_ROTATION_PORTRAIT_REV:
            madctl = ILI9341_MADCTL_MY | ILI9341_MADCTL_BGR;
            dev->width  = ILI9341_WIDTH;
            dev->height = ILI9341_HEIGHT;
            break;

        case ILI9341_ROTATION_LANDSCAPE_REV:
            madctl = ILI9341_MADCTL_MX | ILI9341_MADCTL_MY | ILI9341_MADCTL_MV | ILI9341_MADCTL_BGR;
            dev->width  = ILI9341_HEIGHT;
            dev->height = ILI9341_WIDTH;
            break;
    }

    ili_write_cmd(dev, ILI9341_CMD_MADCTL);
    ili_write_data8(dev, madctl);
}

void ILI9341_SetBacklight(ILI9341_Dev *dev, uint16_t duty)
{
    if (dev->htim_pwm != NULL) {
        if (duty > 999) duty = 999;
        __HAL_TIM_SET_COMPARE(dev->htim_pwm, dev->pwm_channel, duty);
    }
}

void ILI9341_Sleep(ILI9341_Dev *dev)
{
    /* Turn off backlight */
    ILI9341_SetBacklight(dev, 0);

    /* Turn display off and enter low-power sleep */
    ili_write_cmd(dev, ILI9341_CMD_DISPOFF);
    HAL_Delay(20);
    ili_write_cmd(dev, ILI9341_CMD_SLPIN);
    HAL_Delay(120);
}

void ILI9341_Wakeup(ILI9341_Dev *dev)
{
    /* Exit sleep and turn display on */
    ili_write_cmd(dev, ILI9341_CMD_SLPOUT);
    HAL_Delay(120);
    ili_write_cmd(dev, ILI9341_CMD_DISPON);
    HAL_Delay(20);

    /* Restore default backlight */
    ILI9341_SetBacklight(dev, TFT_BLK_DEFAULT);
}

void ILI9341_SetAddressWindow(ILI9341_Dev *dev, uint16_t x0, uint16_t y0,
                              uint16_t x1, uint16_t y1)
{
    uint8_t col_data[4] = {
        (uint8_t)(x0 >> 8), (uint8_t)(x0 & 0xFF),
        (uint8_t)(x1 >> 8), (uint8_t)(x1 & 0xFF)
    };
    uint8_t row_data[4] = {
        (uint8_t)(y0 >> 8), (uint8_t)(y0 & 0xFF),
        (uint8_t)(y1 >> 8), (uint8_t)(y1 & 0xFF)
    };

    ili_write_cmd(dev, ILI9341_CMD_CASET);
    ili_write_data(dev, col_data, 4);

    ili_write_cmd(dev, ILI9341_CMD_PASET);
    ili_write_data(dev, row_data, 4);

    ili_write_cmd(dev, ILI9341_CMD_RAMWR);
}

void ILI9341_DrawPixel(ILI9341_Dev *dev, uint16_t x, uint16_t y, uint16_t color)
{
    if (x >= dev->width || y >= dev->height) return;

    ILI9341_SetAddressWindow(dev, x, y, x, y);

    uint8_t pixel[2] = {(uint8_t)(color >> 8), (uint8_t)(color & 0xFF)};
    ili_write_data(dev, pixel, 2);
}

void ILI9341_FillRect(ILI9341_Dev *dev, uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h, uint16_t color)
{
    if (x >= dev->width || y >= dev->height || w == 0 || h == 0) return;
    if ((x + w - 1) >= dev->width)  w = dev->width - x;
    if ((y + h - 1) >= dev->height) h = dev->height - y;

    ILI9341_SetAddressWindow(dev, x, y, x + w - 1, y + h - 1);

    /* Use a compact 64-pixel (128-byte) stack buffer for fast chunked SPI transfers */
    #define CHUNK_PIXELS 64
    uint8_t chunk[CHUNK_PIXELS * 2];
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);

    for (int i = 0; i < CHUNK_PIXELS; i++) {
        chunk[2 * i]     = hi;
        chunk[2 * i + 1] = lo;
    }

    uint32_t total_pixels = (uint32_t)w * h;

    TFT_DC_DATA();
    TFT_CS_LOW();

    while (total_pixels > 0) {
        uint16_t count = (total_pixels > CHUNK_PIXELS) ? CHUNK_PIXELS : (uint16_t)total_pixels;
        HAL_SPI_Transmit(dev->hspi, chunk, count * 2, HAL_MAX_DELAY);
        total_pixels -= count;
    }

    TFT_CS_HIGH();
}

void ILI9341_FillScreen(ILI9341_Dev *dev, uint16_t color)
{
    ILI9341_FillRect(dev, 0, 0, dev->width, dev->height, color);
}

void ILI9341_DrawHLine(ILI9341_Dev *dev, uint16_t x, uint16_t y,
                       uint16_t len, uint16_t color)
{
    ILI9341_FillRect(dev, x, y, len, 1, color);
}

void ILI9341_DrawVLine(ILI9341_Dev *dev, uint16_t x, uint16_t y,
                       uint16_t len, uint16_t color)
{
    ILI9341_FillRect(dev, x, y, 1, len, color);
}

void ILI9341_DrawBitmap(ILI9341_Dev *dev, uint16_t x, uint16_t y,
                        uint16_t w, uint16_t h, const uint16_t *data)
{
    if (x >= dev->width || y >= dev->height || w == 0 || h == 0 || data == NULL) return;
    if ((x + w - 1) >= dev->width)  w = dev->width - x;
    if ((y + h - 1) >= dev->height) h = dev->height - y;

    ILI9341_SetAddressWindow(dev, x, y, x + w - 1, y + h - 1);

    /* Byte-swap in 64-pixel chunks because ARM Cortex-M is little-endian */
    uint8_t chunk[CHUNK_PIXELS * 2];
    uint32_t total_pixels = (uint32_t)w * h;
    const uint16_t *src   = data;

    TFT_DC_DATA();
    TFT_CS_LOW();

    while (total_pixels > 0) {
        uint16_t count = (total_pixels > CHUNK_PIXELS) ? CHUNK_PIXELS : (uint16_t)total_pixels;
        for (uint16_t i = 0; i < count; i++) {
            uint16_t val = *src++;
            chunk[2 * i]     = (uint8_t)(val >> 8);
            chunk[2 * i + 1] = (uint8_t)(val & 0xFF);
        }
        HAL_SPI_Transmit(dev->hspi, chunk, count * 2, HAL_MAX_DELAY);
        total_pixels -= count;
    }

    TFT_CS_HIGH();
}

void ILI9341_DrawChar(ILI9341_Dev *dev, uint16_t x, uint16_t y, char c,
                      uint16_t color, uint16_t bg_color, uint8_t size)
{
    if (c < 32 || c > 126) c = '?';
    if (size == 0) size = 1;

    uint8_t c_idx = (uint8_t)(c - 32);

    for (uint8_t col = 0; col < 5; col++) {
        uint8_t line = font5x7[c_idx][col];
        for (uint8_t row = 0; row < 7; row++) {
            uint16_t pixel_color = (line & 0x01) ? color : bg_color;
            if (size == 1) {
                ILI9341_DrawPixel(dev, x + col, y + row, pixel_color);
            } else {
                ILI9341_FillRect(dev, x + (col * size), y + (row * size), size, size, pixel_color);
            }
            line >>= 1;
        }
    }

    /* 1-pixel column spacing */
    if (size == 1) {
        ILI9341_DrawVLine(dev, x + 5, y, 7, bg_color);
    } else {
        ILI9341_FillRect(dev, x + (5 * size), y, size, 7 * size, bg_color);
    }
}

void ILI9341_DrawString(ILI9341_Dev *dev, uint16_t x, uint16_t y, const char *str,
                        uint16_t color, uint16_t bg_color, uint8_t size)
{
    if (str == NULL) return;
    if (size == 0) size = 1;

    uint16_t cur_x = x;
    uint16_t cur_y = y;
    uint16_t char_w = 6 * size;
    uint16_t char_h = 8 * size;

    while (*str) {
        if (*str == '\n') {
            cur_x = x;
            cur_y += char_h;
        } else if (*str == '\r') {
            /* ignore carriage return */
        } else {
            if ((cur_x + char_w) > dev->width) {
                cur_x = x;
                cur_y += char_h;
            }
            if ((cur_y + char_h) > dev->height) {
                break; /* Out of display bounds */
            }
            ILI9341_DrawChar(dev, cur_x, cur_y, *str, color, bg_color, size);
            cur_x += char_w;
        }
        str++;
    }
}
