/**
  ******************************************************************************
  * @file    sd_logger.c
  * @brief   MicroSD FATFS CSV data logger implementation.
  *          Automatically creates "WEATHER.CSV" on the root of the SD card
  *          with column headers and appends formatted rows.
  ******************************************************************************
  */

#include "sd_logger.h"
#include "app_config.h"
#include <stdio.h>
#include <string.h>

#if __has_include("ff.h")
#include "ff.h"
#define HAS_FATFS 1
#else
#define HAS_FATFS 0
#endif

#if HAS_FATFS

static FATFS   sd_fs;
static FIL     log_file;
static bool    logger_ready = false;

static const char csv_header[] =
    "Time_s,Temp_C,Pres_hPa,Hum_pct,DewPoint_C,Gas_kOhm,IAQ_Score,IAQ_Status,Lux,UV_Index\r\n";

bool SD_Logger_Init(void)
{
    FRESULT fr;

    /* 1. Mount SD volume (0 = drive 0, 1 = force immediate mount) */
    fr = f_mount(&sd_fs, "", 1);
    if (fr != FR_OK) {
        printf("SD_Logger: f_mount failed (err %d)\r\n", fr);
        logger_ready = false;
        return false;
    }

    /* 2. Check if WEATHER.CSV exists; if not, create it and write headers */
    FILINFO fno;
    fr = f_stat(SD_LOG_FILENAME, &fno);
    if (fr == FR_NO_FILE) {
        /* Create file and write header */
        fr = f_open(&log_file, SD_LOG_FILENAME, FA_CREATE_ALWAYS | FA_WRITE);
        if (fr == FR_OK) {
            UINT bw;
            f_write(&log_file, csv_header, strlen(csv_header), &bw);
            f_close(&log_file);
            printf("SD_Logger: Created new %s with headers\r\n", SD_LOG_FILENAME);
        } else {
            printf("SD_Logger: Failed to create %s (err %d)\r\n", SD_LOG_FILENAME, fr);
            logger_ready = false;
            return false;
        }
    } else if (fr == FR_OK) {
        printf("SD_Logger: Found existing %s (%lu bytes)\r\n", SD_LOG_FILENAME, (unsigned long)fno.fsize);
    }

    logger_ready = true;
    return true;
}

bool SD_Logger_LogWeather(uint32_t timestamp_sec, const BME680_Data *bd, const LTR390_Data *ld)
{
    if (!logger_ready) return false;

    FRESULT fr = f_open(&log_file, SD_LOG_FILENAME, FA_OPEN_APPEND | FA_WRITE);
    if (fr != FR_OK) {
        printf("SD_Logger: f_open append failed (err %d)\r\n", fr);
        return false;
    }

    char row[128];
    float gas_k = bd->gas_valid ? (bd->gas_resistance / 1000.0f) : 0.0f;
    float iaq   = bd->gas_valid ? bd->iaq_score : 0.0f;
    const char *iaq_str = bd->gas_valid ? bd->iaq_text : "N/A";

    int len = snprintf(row, sizeof(row),
        "%lu,%.2f,%.2f,%.1f,%.2f,%.1f,%.0f,%s,%.1f,%.2f\r\n",
        (unsigned long)timestamp_sec,
        bd->temperature,
        bd->pressure,
        bd->humidity,
        bd->dew_point,
        gas_k,
        iaq,
        iaq_str,
        ld->lux,
        ld->uv_index
    );

    UINT bw;
    fr = f_write(&log_file, row, (UINT)len, &bw);
    f_close(&log_file);

    return (fr == FR_OK && bw == (UINT)len);
}

bool SD_Logger_IsActive(void)
{
    return logger_ready;
}

#else

/* Stub implementations when FatFS middleware is not yet generated in the project */
bool SD_Logger_Init(void)
{
    return false;
}

bool SD_Logger_LogWeather(uint32_t timestamp_sec, const BME680_Data *bd, const LTR390_Data *ld)
{
    (void)timestamp_sec;
    (void)bd;
    (void)ld;
    return false;
}

bool SD_Logger_IsActive(void)
{
    return false;
}

#endif /* HAS_FATFS */
