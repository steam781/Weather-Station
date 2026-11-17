/**
  ******************************************************************************
  * @file    ili9341.h
  * @brief   ILI9341 2.8" TFT display driver for STM32 HAL (SPI1).
  *          Supports portrait/landscape rotations, hardware reset,
  *          backlight PWM, primitives, and fast block writes (LVGL-ready).
  *
  *          Bus Sharing: Complies strictly with shared SPI1 bus rules
  *          (TFT_CS returned HIGH after every transaction).
  ******************************************************************************
  */
#ifndef ILI9341_H
#define ILI9341_H

#include "stm32g4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Display Dimensions ─────────────────────────────────────────────────── */
#define ILI9341_WIDTH               240
#define ILI9341_HEIGHT              320

/* ── Commands ───────────────────────────────────────────────────────────── */
#define ILI9341_CMD_NOP             0x00
#define ILI9341_CMD_SWRESET         0x01
#define ILI9341_CMD_SLPIN           0x10
#define ILI9341_CMD_SLPOUT          0x11
#define ILI9341_CMD_INVOFF          0x20
#define ILI9341_CMD_INVON           0x21
#define ILI9341_CMD_DISPOFF         0x28
#define ILI9341_CMD_DISPON          0x29
#define ILI9341_CMD_CASET           0x2A
#define ILI9341_CMD_PASET           0x2B
#define ILI9341_CMD_RAMWR           0x2C
#define ILI9341_CMD_MADCTL          0x36
#define ILI9341_CMD_PIXFMT          0x3A

/* ── MADCTL Flags ───────────────────────────────────────────────────────── */
#define ILI9341_MADCTL_MY           0x80  /* Row address order               */
#define ILI9341_MADCTL_MX           0x40  /* Column address order            */
#define ILI9341_MADCTL_MV           0x20  /* Row / column exchange           */
#define ILI9341_MADCTL_ML           0x10  /* Vertical refresh direction      */
#define ILI9341_MADCTL_BGR          0x08  /* RGB-BGR Order                   */
#define ILI9341_MADCTL_MH           0x04  /* Horizontal refresh direction    */

/* ── Orientations ───────────────────────────────────────────────────────── */
typedef enum {
    ILI9341_ROTATION_PORTRAIT = 0,        /* 240 × 320                       */
    ILI9341_ROTATION_LANDSCAPE,           /* 320 × 240                       */
    ILI9341_ROTATION_PORTRAIT_REV,        /* 240 × 320 inverted              */
    ILI9341_ROTATION_LANDSCAPE_REV        /* 320 × 240 inverted              */
} ILI9341_Rotation;

/* ── Common RGB565 Colors ───────────────────────────────────────────────── */
#define ILI9341_BLACK               0x0000
#define ILI9341_NAVY                0x000F
#define ILI9341_DARKGREEN           0x03E0
#define ILI9341_DARKCYAN            0x03EF
#define ILI9341_MAROON              0x7800
#define ILI9341_PURPLE              0x780F
#define ILI9341_OLIVE               0x7BE0
#define ILI9341_LIGHTGREY           0xC618
#define ILI9341_DARKGREY            0x7BEF
#define ILI9341_BLUE                0x001F
#define ILI9341_GREEN               0x07E0
#define ILI9341_CYAN                0x07FF
#define ILI9341_RED                 0xF800
#define ILI9341_MAGENTA             0xF81F
#define ILI9341_YELLOW              0xFFE0
#define ILI9341_WHITE               0xFFFF
#define ILI9341_ORANGE              0xFD20
#define ILI9341_GREENYELLOW         0xAFE5

/* Convert 8-bit RGB to 16-bit RGB565 */
#define ILI9341_RGB565(r, g, b) \
    ((((uint16_t)(r) & 0xF8) << 8) | (((uint16_t)(g) & 0xFC) << 3) | ((uint16_t)(b) >> 3))

/* ── Device Handle ───────────────────────────────────────────────────────── */
typedef struct {
    SPI_HandleTypeDef *hspi;
    TIM_HandleTypeDef *htim_pwm;
    uint32_t           pwm_channel;
    uint16_t           width;
    uint16_t           height;
    ILI9341_Rotation   rotation;
} ILI9341_Dev;

/* ── Public API ──────────────────────────────────────────────────────────── */

/**
 * @brief  Initialise the ILI9341: perform hardware reset, send configuration
 *         sequence, clear screen to black, and start backlight PWM.
 * @param  dev          Device handle pointer.
 * @param  hspi         SPI handle (e.g. &hspi1).
 * @param  htim_pwm     TIM handle for backlight PWM (e.g. &htim2) or NULL.
 * @param  pwm_channel  TIM channel (e.g. TIM_CHANNEL_1).
 */
void ILI9341_Init(ILI9341_Dev *dev, SPI_HandleTypeDef *hspi,
                  TIM_HandleTypeDef *htim_pwm, uint32_t pwm_channel);

/**
 * @brief  Set display orientation.
 * @param  dev       Device handle.
 * @param  rotation  ILI9341_ROTATION_PORTRAIT / LANDSCAPE / ...
 */
void ILI9341_SetRotation(ILI9341_Dev *dev, ILI9341_Rotation rotation);

/**
 * @brief  Adjust display backlight brightness via PWM duty cycle.
 * @param  dev   Device handle.
 * @param  duty  0 (off) to 999 (full brightness).
 */
void ILI9341_SetBacklight(ILI9341_Dev *dev, uint16_t duty);

/**
 * @brief  Enter display sleep mode (turns off display and backlight).
 *         Used during Stop Mode in v2 Outdoor Mode.
 * @param  dev  Device handle.
 */
void ILI9341_Sleep(ILI9341_Dev *dev);

/**
 * @brief  Wake display from sleep mode.
 * @param  dev  Device handle.
 */
void ILI9341_Wakeup(ILI9341_Dev *dev);

/**
 * @brief  Define drawing window coordinates.
 * @param  dev   Device handle.
 * @param  x0,y0 Start coordinates.
 * @param  x1,y1 End coordinates.
 */
void ILI9341_SetAddressWindow(ILI9341_Dev *dev, uint16_t x0, uint16_t y0,
                              uint16_t x1, uint16_t y1);

/**
 * @brief  Draw a single pixel in RGB565 color.
 */
void ILI9341_DrawPixel(ILI9341_Dev *dev, uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief  Fill a rectangle with a solid RGB565 color using block transfers.
 */
void ILI9341_FillRect(ILI9341_Dev *dev, uint16_t x, uint16_t y,
                      uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief  Fill entire screen with a solid color.
 */
void ILI9341_FillScreen(ILI9341_Dev *dev, uint16_t color);

/**
 * @brief  Draw a horizontal line.
 */
void ILI9341_DrawHLine(ILI9341_Dev *dev, uint16_t x, uint16_t y,
                       uint16_t len, uint16_t color);

/**
 * @brief  Draw a vertical line.
 */
void ILI9341_DrawVLine(ILI9341_Dev *dev, uint16_t x, uint16_t y,
                       uint16_t len, uint16_t color);

/**
 * @brief  Stream an RGB565 pixel buffer directly to a display area.
 *         Directly compatible with LVGL `flush_cb`.
 * @param  dev   Device handle.
 * @param  x,y   Top-left corner.
 * @param  w,h   Dimensions.
 * @param  data  Array of RGB565 pixel values (w * h elements).
 */
void ILI9341_DrawBitmap(ILI9341_Dev *dev, uint16_t x, uint16_t y,
                        uint16_t w, uint16_t h, const uint16_t *data);

/**
 * @brief  Draw a single ASCII character using built-in 5x7 font.
 * @param  dev      Device handle.
 * @param  x,y      Top-left coordinates.
 * @param  c        Character to draw.
 * @param  color    Text color (RGB565).
 * @param  bg_color Background color (RGB565).
 * @param  size     Scaling factor (1 = 6x8 px, 2 = 12x16 px, etc.).
 */
void ILI9341_DrawChar(ILI9341_Dev *dev, uint16_t x, uint16_t y, char c,
                      uint16_t color, uint16_t bg_color, uint8_t size);

/**
 * @brief  Draw an ASCII string with text wrapping.
 * @param  dev      Device handle.
 * @param  x,y      Top-left coordinates.
 * @param  str      Null-terminated string.
 * @param  color    Text color (RGB565).
 * @param  bg_color Background color (RGB565).
 * @param  size     Scaling factor (1, 2, ...).
 */
void ILI9341_DrawString(ILI9341_Dev *dev, uint16_t x, uint16_t y, const char *str,
                        uint16_t color, uint16_t bg_color, uint8_t size);

#endif /* ILI9341_H */
