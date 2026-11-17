/**
  ******************************************************************************
  * @file    app_config.h
  * @brief   Top-level application configuration for the Weather Station.
  *          Contains build-mode flags, sensor addresses, and tuning constants.
  ******************************************************************************
  */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ── Build-Mode Flag ─────────────────────────────────────────────────────── */
/* 0 = v1 Indoor (USB/bench-powered, display always on, gas/IAQ enabled)     */
/* 1 = v2 Outdoor (battery/solar, Stop Mode, gas/IAQ disabled)               */
#define OUTDOOR_MODE         0

/* ── I2C Sensor Addresses (7-bit, unshifted) ─────────────────────────────── */
#define BME680_I2C_ADDR      0x77   /* Bosch BME680 (SDO → VDD)              */
#define LTR390_I2C_ADDR      0x53   /* Lite-On LTR-390UV-01                  */

/* ── Sensor Timing ───────────────────────────────────────────────────────── */
#define SENSOR_POLL_MS       2000   /* Main-loop sensor read interval (ms)   */
#define BME680_MEAS_WAIT_MS   300   /* Max wait for BME680 forced conversion */
#define LTR390_CONV_WAIT_MS   150   /* Max wait for LTR390 conversion        */

/* ── BME680 Oversampling & Gas Heater ─────────────────────────────────────── */
#define BME680_OS_TEMP          4   /* Temperature oversampling: x8          */
#define BME680_OS_PRES          3   /* Pressure oversampling:   x4          */
#define BME680_OS_HUM           2   /* Humidity oversampling:   x2          */
#define BME680_GAS_TEMP       320   /* Gas heater target (°C)                */
#define BME680_GAS_DUR        150   /* Gas heater duration (ms)              */

/* ── LTR390 Defaults ─────────────────────────────────────────────────────── */
#define LTR390_ALS_GAIN         1   /* ALS gain: 3×                          */
#define LTR390_UVS_GAIN         4   /* UVS gain: 18×                         */
#define LTR390_RESOLUTION       2   /* 18-bit (100 ms integration)          */
#define LTR390_MEAS_RATE        2   /* 100 ms measurement rate               */

/* ── Display & LED ───────────────────────────────────────────────────────── */
#define TFT_BLK_DEFAULT       500   /* Default backlight PWM duty (0–999)    */
#define BTN_LED_LUX_THRESH   20.0f  /* Lux threshold for button LED ring     */

/* ── MicroSD Logging ─────────────────────────────────────────────────────── */
#define SD_LOG_FILENAME      "WEATHER.CSV"
#define SD_LOG_INTERVAL_MS   10000  /* CSV logging interval (10 seconds)     */

/* ── I2C HAL Timeout ─────────────────────────────────────────────────────── */
#define I2C_TIMEOUT_MS        100

#endif /* APP_CONFIG_H */
