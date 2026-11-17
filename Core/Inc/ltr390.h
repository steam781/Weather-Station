/**
  ******************************************************************************
  * @file    ltr390.h
  * @brief   LTR-390UV-01 ambient light / UV sensor driver for STM32 HAL (I2C).
  *          Provides ALS (lux) and UVS (UV index) readings.
  ******************************************************************************
  */
#ifndef LTR390_H
#define LTR390_H

#include "stm32g4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Chip Identification ─────────────────────────────────────────────────── */
#define LTR390_PART_ID_VAL      0xB2

/* ── Register Map ────────────────────────────────────────────────────────── */
#define LTR390_REG_MAIN_CTRL    0x00
#define LTR390_REG_MEAS_RATE    0x04
#define LTR390_REG_GAIN         0x05
#define LTR390_REG_PART_ID      0x06
#define LTR390_REG_MAIN_STATUS  0x07
#define LTR390_REG_ALS_DATA_0   0x0D   /* LSB */
#define LTR390_REG_ALS_DATA_1   0x0E
#define LTR390_REG_ALS_DATA_2   0x0F   /* MSB (bits [3:0]) */
#define LTR390_REG_UVS_DATA_0   0x10   /* LSB */
#define LTR390_REG_UVS_DATA_1   0x11
#define LTR390_REG_UVS_DATA_2   0x12   /* MSB (bits [3:0]) */

/* ── MAIN_CTRL bits ──────────────────────────────────────────────────────── */
#define LTR390_CTRL_EN          (1 << 1)  /* Sensor enable               */
#define LTR390_CTRL_UVS_MODE    (1 << 3)  /* 0 = ALS mode, 1 = UVS mode */

/* ── MAIN_STATUS bits ────────────────────────────────────────────────────── */
#define LTR390_STATUS_DATA_RDY  (1 << 3)

/* ── Gain values (register value → actual multiplier) ────────────────────── */
/*  Reg  Gain                                                                 */
/*   0    1×                                                                  */
/*   1    3×                                                                  */
/*   2    6×                                                                  */
/*   3    9×                                                                  */
/*   4   18×                                                                  */

/* ── Resolution / integration time (register value → bit depth / time) ───── */
/*  Reg  Bits  Integration                                                    */
/*   0   20    400 ms                                                         */
/*   1   19    200 ms                                                         */
/*   2   18    100 ms  ← default                                              */
/*   3   17     50 ms                                                         */
/*   4   16     25 ms                                                         */

/* ── Return Codes ────────────────────────────────────────────────────────── */
typedef enum {
    LTR390_OK = 0,
    LTR390_ERR_I2C,
    LTR390_ERR_PART_ID,
    LTR390_ERR_TIMEOUT,
} LTR390_Status;

/* ── Measurement Results ─────────────────────────────────────────────────── */
typedef struct {
    uint32_t als_raw;      /* Raw ALS ADC count                               */
    uint32_t uvs_raw;      /* Raw UVS ADC count                               */
    float    lux;          /* Calculated lux                                  */
    float    uv_index;     /* Calculated UV index                             */
    bool     saturated;    /* True if ADC is clipped to full-scale            */
} LTR390_Data;

/* ── Device Handle ───────────────────────────────────────────────────────── */
typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t            addr;        /* 7-bit I2C address                      */
    uint8_t            als_gain;    /* Gain register value                    */
    uint8_t            uvs_gain;    /* Gain register value                    */
    uint8_t            resolution;  /* Resolution register value              */
    LTR390_Data        data;
} LTR390_Dev;

/* ── Public API ──────────────────────────────────────────────────────────── */

/**
 * @brief  Initialise the LTR390: verify part ID, set gain & resolution.
 * @param  dev   Pointer to an uninitialised device handle.
 * @param  hi2c  HAL I2C handle (must already be initialised).
 * @param  addr  7-bit I2C address (typically 0x53).
 * @return LTR390_OK on success.
 */
LTR390_Status LTR390_Init(LTR390_Dev *dev, I2C_HandleTypeDef *hi2c, uint8_t addr);

/**
 * @brief  Read both ALS (lux) and UVS (UV index).
 *         Internally switches modes and waits for each conversion.
 * @param  dev  Device handle (results stored in dev->data).
 * @return LTR390_OK on success.
 */
LTR390_Status LTR390_Read(LTR390_Dev *dev);

/**
 * @brief  Put LTR390 into standby mode (< 1 µA).
 *         Used during Stop Mode in v2 Outdoor Mode.
 * @param  dev  Device handle.
 * @return LTR390_OK on success.
 */
LTR390_Status LTR390_Standby(LTR390_Dev *dev);

/**
 * @brief  Wake LTR390 from standby mode into active ALS operation.
 * @param  dev  Device handle.
 * @return LTR390_OK on success.
 */
LTR390_Status LTR390_Wakeup(LTR390_Dev *dev);

/**
 * @brief  Configure ALS and UVS gain settings dynamically.
 * @param  dev       Device handle.
 * @param  als_gain  ALS gain: 0=1×, 1=3×, 2=6×, 3=9×, 4=18×.
 * @param  uvs_gain  UVS gain: 0=1×, 1=3×, 2=6×, 3=9×, 4=18×.
 * @return LTR390_OK on success.
 */
LTR390_Status LTR390_SetGain(LTR390_Dev *dev, uint8_t als_gain, uint8_t uvs_gain);

/**
 * @brief  Access the most recent measurement results.
 * @param  dev  Device handle.
 * @return Pointer to the internal LTR390_Data struct.
 */
const LTR390_Data *LTR390_GetData(const LTR390_Dev *dev);

#endif /* LTR390_H */
