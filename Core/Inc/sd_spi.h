/**
  ******************************************************************************
  * @file    sd_spi.h
  * @brief   Low-level SD Card SPI driver for STM32 HAL (SPI1).
  *          Supports SDv1, SDv2 (SDSC / SDHC / SDXC) and MMC.
  *
  *          Bus Sharing: Complies strictly with shared SPI1 bus rules
  *          (SD_CS returned HIGH after every transaction + 8 dummy clocks
  *          to release MISO into high-impedance).
  ******************************************************************************
  */
#ifndef SD_SPI_H
#define SD_SPI_H

#include "stm32g4xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* ── Sector Size ─────────────────────────────────────────────────────────── */
#define SD_BLOCK_SIZE           512

/* ── Card Types ─────────────────────────────────────────────────────────── */
#define SD_TYPE_UNKNOWN         0x00
#define SD_TYPE_MMC             0x01
#define SD_TYPE_V1              0x02
#define SD_TYPE_V2SC            0x04  /* Standard Capacity (byte-addressed) */
#define SD_TYPE_V2HC            0x08  /* High / Extended Capacity (block-addressed) */

/* ── Status / Return Codes ───────────────────────────────────────────────── */
typedef enum {
    SD_OK = 0,
    SD_ERR_NOT_READY,
    SD_ERR_TIMEOUT,
    SD_ERR_CMD_FAILED,
    SD_ERR_DATA_REJECTED,
    SD_ERR_CRC,
    SD_ERR_PARAM
} SD_Status;

/* ── Device Handle ───────────────────────────────────────────────────────── */
typedef struct {
    SPI_HandleTypeDef *hspi;
    uint8_t            card_type;
    uint32_t           sector_count;
    bool               initialized;
} SD_Dev;

/* ── Public API ──────────────────────────────────────────────────────────── */

/**
 * @brief  Initialise the SD card over SPI:
 *         1. Send 80+ clock pulses with CS high at low speed (< 400 kHz).
 *         2. Send CMD0 (reset to idle).
 *         3. Send CMD8 (check operating voltage & card type).
 *         4. Poll ACMD41 until the card is ready.
 *         5. Read OCR via CMD58 (check SDHC block addressing).
 *         6. Switch SPI prescaler to high speed (~10 MHz).
 * @param  dev   Device handle pointer.
 * @param  hspi  SPI peripheral handle (e.g. &hspi1).
 * @return SD_OK on success.
 */
SD_Status SD_Init(SD_Dev *dev, SPI_HandleTypeDef *hspi);

/**
 * @brief  Read one or more 512-byte sectors from the SD card.
 * @param  dev     Device handle.
 * @param  buffer  Destination buffer (must be at least count * 512 bytes).
 * @param  sector  Starting sector (LBA).
 * @param  count   Number of sectors to read.
 * @return SD_OK on success.
 */
SD_Status SD_ReadSectors(SD_Dev *dev, uint8_t *buffer, uint32_t sector, uint32_t count);

/**
 * @brief  Write one or more 512-byte sectors to the SD card.
 * @param  dev     Device handle.
 * @param  buffer  Source buffer containing count * 512 bytes.
 * @param  sector  Starting sector (LBA).
 * @param  count   Number of sectors to write.
 * @return SD_OK on success.
 */
SD_Status SD_WriteSectors(SD_Dev *dev, const uint8_t *buffer, uint32_t sector, uint32_t count);

/**
 * @brief  Wait until the SD card finishes any ongoing internal write operation.
 * @param  dev     Device handle.
 * @param  timeout_ms Timeout in milliseconds.
 * @return SD_OK when ready.
 */
SD_Status SD_WaitReady(SD_Dev *dev, uint32_t timeout_ms);

/**
 * @brief  Retrieve total sector count of the card.
 * @param  dev  Device handle.
 * @return Number of 512-byte sectors.
 */
uint32_t SD_GetSectorCount(const SD_Dev *dev);

/**
 * @brief  Check if the SD card is present and initialised.
 */
bool SD_IsInitialized(const SD_Dev *dev);

#endif /* SD_SPI_H */
