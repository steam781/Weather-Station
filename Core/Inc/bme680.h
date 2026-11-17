/**
  ******************************************************************************
  * @file    bme680.h
  * @brief   BME680 environmental sensor driver for STM32 HAL (I2C, forced mode).
  *          Provides temperature, pressure, humidity, and gas resistance.
  ******************************************************************************
  */
#ifndef BME680_H
#define BME680_H

#include "stm32g4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Chip Identification ─────────────────────────────────────────────────── */
#define BME680_CHIP_ID          0x61

/* ── Register Map ────────────────────────────────────────────────────────── */
#define BME680_REG_STATUS       0x73
#define BME680_REG_CTRL_MEAS    0x74
#define BME680_REG_CTRL_HUM     0x72
#define BME680_REG_CTRL_GAS1    0x71
#define BME680_REG_CTRL_GAS0    0x70
#define BME680_REG_CONFIG       0x75
#define BME680_REG_CHIP_ID      0xD0
#define BME680_REG_SOFT_RESET   0xE0

/* Raw data field 0 */
#define BME680_REG_MEAS_STATUS  0x1D
#define BME680_REG_PRES_MSB     0x1F
#define BME680_REG_PRES_LSB     0x20
#define BME680_REG_PRES_XLSB    0x21
#define BME680_REG_TEMP_MSB     0x22
#define BME680_REG_TEMP_LSB     0x23
#define BME680_REG_TEMP_XLSB    0x24
#define BME680_REG_HUM_MSB      0x25
#define BME680_REG_HUM_LSB      0x26
#define BME680_REG_GAS_MSB      0x2A
#define BME680_REG_GAS_LSB      0x2B

/* Heater set-point 0 */
#define BME680_REG_RES_HEAT0    0x5A
#define BME680_REG_GAS_WAIT0    0x64

/* Calibration data regions */
#define BME680_REG_COEFF1       0x89   /* 25 bytes: 0x89 – 0xA1 */
#define BME680_REG_COEFF1_LEN   25
#define BME680_REG_COEFF2       0xE1   /* 16 bytes: 0xE1 – 0xF0 */
#define BME680_REG_COEFF2_LEN   16

/* Additional calibration registers */
#define BME680_REG_RES_HEAT_RANGE 0x02 /* bits [5:4] */
#define BME680_REG_RES_HEAT_VAL   0x00
#define BME680_REG_RANGE_SW_ERR   0x04 /* bits [7:4] (signed nibble) */

/* ── Mode Constants ──────────────────────────────────────────────────────── */
#define BME680_MODE_SLEEP       0x00
#define BME680_MODE_FORCED      0x01
#define BME680_SOFT_RESET_CMD   0xB6

/* ── Return Codes ────────────────────────────────────────────────────────── */
typedef enum {
    BME680_OK = 0,
    BME680_ERR_I2C,
    BME680_ERR_CHIP_ID,
    BME680_ERR_TIMEOUT,
} BME680_Status;

/* ── Calibration Data (read once at init) ────────────────────────────────── */
typedef struct {
    /* Temperature */
    uint16_t par_t1;
    int16_t  par_t2;
    int8_t   par_t3;

    /* Pressure */
    uint16_t par_p1;
    int16_t  par_p2;
    int8_t   par_p3;
    int16_t  par_p4;
    int16_t  par_p5;
    int8_t   par_p6;
    int8_t   par_p7;
    int16_t  par_p8;
    int16_t  par_p9;
    uint8_t  par_p10;

    /* Humidity */
    uint16_t par_h1;
    uint16_t par_h2;
    int8_t   par_h3;
    int8_t   par_h4;
    int8_t   par_h5;
    uint8_t  par_h6;
    int8_t   par_h7;

    /* Gas heater */
    int8_t   par_gh1;
    int16_t  par_gh2;
    int8_t   par_gh3;
    uint8_t  res_heat_range;
    int8_t   res_heat_val;
    int8_t   range_sw_err;

    /* Shared temperature fine value for cascaded compensation */
    float    t_fine;
} BME680_CalibData;

/* ── Measurement Results ─────────────────────────────────────────────────── */
typedef struct {
    float temperature;     /* °C                              */
    float pressure;        /* hPa (mbar)                      */
    float humidity;        /* %RH                             */
    float gas_resistance;  /* Ω  (0 if gas reading invalid)   */
    bool  gas_valid;       /* true if heater was stable        */
} BME680_Data;

/* ── Device Handle ───────────────────────────────────────────────────────── */
typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t            addr;       /* 7-bit I2C address              */
    BME680_CalibData   calib;
    BME680_Data        data;
} BME680_Dev;

/* ── Public API ──────────────────────────────────────────────────────────── */

/**
 * @brief  Initialise the BME680: soft-reset, verify chip ID, read calibration,
 *         configure oversampling & gas heater.
 * @param  dev   Pointer to an uninitialised device handle.
 * @param  hi2c  HAL I2C handle (must already be initialised).
 * @param  addr  7-bit I2C address (typically 0x77).
 * @return BME680_OK on success.
 */
BME680_Status BME680_Init(BME680_Dev *dev, I2C_HandleTypeDef *hi2c, uint8_t addr);

/**
 * @brief  Trigger a forced-mode measurement (T + P + H + Gas) and block
 *         until the data is ready or timeout.
 * @param  dev          Device handle.
 * @param  enable_gas   true to run the gas heater (ignored when OUTDOOR_MODE).
 * @return BME680_OK on success.
 */
BME680_Status BME680_ReadForced(BME680_Dev *dev, bool enable_gas);

/**
 * @brief  Access the most recent measurement results.
 * @param  dev  Device handle (results are in dev->data after BME680_ReadForced).
 * @return Pointer to the internal BME680_Data struct.
 */
const BME680_Data *BME680_GetData(const BME680_Dev *dev);

#endif /* BME680_H */
