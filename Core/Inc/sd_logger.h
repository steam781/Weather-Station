/**
  ******************************************************************************
  * @file    sd_logger.h
  * @brief   MicroSD FATFS CSV data logger for weather telemetry.
  *          Provides mounting, CSV header creation, and periodic appending.
  ******************************************************************************
  */
#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include "stm32g4xx_hal.h"
#include "bme680.h"
#include "ltr390.h"
#include <stdbool.h>

/* ── Public API ──────────────────────────────────────────────────────────── */

/**
 * @brief  Mount the SD card FAT volume and ensure the CSV file exists with headers.
 * @return true if mounted and ready for logging.
 */
bool SD_Logger_Init(void);

/**
 * @brief  Append a single weather telemetry row to the CSV file.
 * @param  timestamp_sec  Elapsed uptime or RTC epoch timestamp in seconds.
 * @param  bd             Pointer to latest BME680 measurements.
 * @param  ld             Pointer to latest LTR390 measurements.
 * @return true if write and sync succeeded.
 */
bool SD_Logger_LogWeather(uint32_t timestamp_sec, const BME680_Data *bd, const LTR390_Data *ld);

/**
 * @brief  Check if the SD logger is mounted and active.
 */
bool SD_Logger_IsActive(void);

#endif /* SD_LOGGER_H */
