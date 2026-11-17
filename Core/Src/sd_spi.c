/**
  ******************************************************************************
  * @file    sd_spi.c
  * @brief   Low-level SD Card SPI driver for STM32 HAL (SPI1).
  *          Complies with shared SPI1 bus rules (SD_CS deasserted + dummy clock
  *          after every transaction to ensure MISO tri-states).
  ******************************************************************************
  */

#include "sd_spi.h"
#include "main.h"
#include <string.h>

/* ── Command Definitions ─────────────────────────────────────────────────── */
#define CMD0    (0x40 | 0)   /* GO_IDLE_STATE */
#define CMD8    (0x40 | 8)   /* SEND_IF_COND */
#define CMD9    (0x40 | 9)   /* SEND_CSD */
#define CMD10   (0x40 | 10)  /* SEND_CID */
#define CMD12   (0x40 | 12)  /* STOP_TRANSMISSION */
#define CMD16   (0x40 | 16)  /* SET_BLOCKLEN */
#define CMD17   (0x40 | 17)  /* READ_SINGLE_BLOCK */
#define CMD18   (0x40 | 18)  /* READ_MULTIPLE_BLOCK */
#define CMD24   (0x40 | 24)  /* WRITE_BLOCK */
#define CMD25   (0x40 | 25)  /* WRITE_MULTIPLE_BLOCK */
#define CMD55   (0x40 | 55)  /* APP_CMD */
#define CMD58   (0x40 | 58)  /* READ_OCR */
#define ACMD41  (0x40 | 41)  /* SD_SEND_OP_COND */

/* ── Data Tokens ─────────────────────────────────────────────────────────── */
#define TOKEN_START_BLOCK       0xFE
#define TOKEN_START_MULTIBLOCK  0xFC
#define TOKEN_STOP_TRAN         0xFD

/* ── Timeouts ────────────────────────────────────────────────────────────── */
#define SD_CMD_TIMEOUT_MS       200
#define SD_WAIT_READY_TIMEOUT   500

/* ════════════════════════════════════════════════════════════════════════════
 *  Private SPI Helper Functions
 * ════════════════════════════════════════════════════════════════════════════ */

static void sd_cs_low(void)
{
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_RESET);
}

static void sd_cs_high(const SD_Dev *dev)
{
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);
    /* 8 dummy clock cycles so card tri-states MISO for other SPI devices */
    uint8_t dummy = 0xFF;
    HAL_SPI_Transmit(dev->hspi, &dummy, 1, 10);
}

static uint8_t sd_spi_xfer(const SD_Dev *dev, uint8_t byte)
{
    uint8_t rx = 0xFF;
    HAL_SPI_TransmitReceive(dev->hspi, &byte, &rx, 1, 100);
    return rx;
}

static void sd_set_speed(const SD_Dev *dev, uint32_t prescaler)
{
    __HAL_SPI_DISABLE(dev->hspi);
    MODIFY_REG(dev->hspi->Instance->CR1, SPI_CR1_BR_Msk, prescaler);
    __HAL_SPI_ENABLE(dev->hspi);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Command & Response Helpers
 * ════════════════════════════════════════════════════════════════════════════ */

SD_Status SD_WaitReady(SD_Dev *dev, uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    while ((HAL_GetTick() - t0) < timeout_ms) {
        if (sd_spi_xfer(dev, 0xFF) == 0xFF) {
            return SD_OK;
        }
    }
    return SD_ERR_TIMEOUT;
}

static uint8_t sd_send_cmd(const SD_Dev *dev, uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t frame[6];
    frame[0] = cmd;
    frame[1] = (uint8_t)(arg >> 24);
    frame[2] = (uint8_t)(arg >> 16);
    frame[3] = (uint8_t)(arg >> 8);
    frame[4] = (uint8_t)(arg & 0xFF);
    frame[5] = crc;

    /* If not CMD0 or CMD12, wait until card is ready before sending command */
    if (cmd != CMD0 && cmd != CMD12) {
        uint32_t t0 = HAL_GetTick();
        while ((HAL_GetTick() - t0) < SD_CMD_TIMEOUT_MS) {
            if (sd_spi_xfer(dev, 0xFF) == 0xFF) break;
        }
    }

    HAL_SPI_Transmit(dev->hspi, frame, 6, 100);

    /* Read response (up to 10 attempts for R1 byte with MSB = 0) */
    uint8_t res = 0xFF;
    for (int i = 0; i < 10; i++) {
        res = sd_spi_xfer(dev, 0xFF);
        if ((res & 0x80) == 0) break;
    }
    return res;
}

static uint8_t sd_send_acmd(const SD_Dev *dev, uint8_t cmd, uint32_t arg)
{
    sd_send_cmd(dev, CMD55, 0, 0x65);
    return sd_send_cmd(dev, cmd, arg, 0x77);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Public API Implementation
 * ════════════════════════════════════════════════════════════════════════════ */

SD_Status SD_Init(SD_Dev *dev, SPI_HandleTypeDef *hspi)
{
    memset(dev, 0, sizeof(*dev));
    dev->hspi = hspi;

    /* Ensure SD_CS is HIGH */
    HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);

    /* Step 1: Set SPI clock to slow speed (< 400 kHz) for card init */
    sd_set_speed(dev, SPI_BAUDRATEPRESCALER_256);

    /* Step 2: Send at least 74 clock cycles with CS HIGH */
    for (int i = 0; i < 12; i++) {
        sd_spi_xfer(dev, 0xFF);
    }

    /* Step 3: Send CMD0 (Reset to IDLE) */
    sd_cs_low();
    uint8_t r1 = sd_send_cmd(dev, CMD0, 0, 0x95);
    sd_cs_high(dev);

    if (r1 != 0x01) {
        return SD_ERR_NOT_READY;
    }

    /* Step 4: Send CMD8 (check operating voltage condition) */
    sd_cs_low();
    r1 = sd_send_cmd(dev, CMD8, 0x000001AA, 0x87);
    uint8_t ocr[4];
    for (int i = 0; i < 4; i++) {
        ocr[i] = sd_spi_xfer(dev, 0xFF);
    }
    sd_cs_high(dev);

    uint32_t t0 = HAL_GetTick();

    if (r1 == 0x01 && ocr[2] == 0x01 && ocr[3] == 0xAA) {
        /* SD v2.0 or later */
        dev->card_type = SD_TYPE_V2SC;

        /* Poll ACMD41 with HCS (bit 30 = 0x40000000) until card exits idle (r1 == 0x00) */
        do {
            sd_cs_low();
            r1 = sd_send_acmd(dev, ACMD41, 0x40000000);
            sd_cs_high(dev);
            if (r1 == 0x00) break;
            HAL_Delay(10);
        } while ((HAL_GetTick() - t0) < 1000);

        if (r1 != 0x00) return SD_ERR_TIMEOUT;

        /* Send CMD58 (READ_OCR) to check CCS (Card Capacity Status bit 30) */
        sd_cs_low();
        r1 = sd_send_cmd(dev, CMD58, 0, 0xFD);
        for (int i = 0; i < 4; i++) {
            ocr[i] = sd_spi_xfer(dev, 0xFF);
        }
        sd_cs_high(dev);

        if (r1 == 0x00 && (ocr[0] & 0x40)) {
            dev->card_type = SD_TYPE_V2HC;  /* SDHC or SDXC */
        }
    } else {
        /* SD v1.x or MMC */
        sd_cs_low();
        r1 = sd_send_acmd(dev, ACMD41, 0);
        sd_cs_high(dev);

        if (r1 <= 0x01) {
            dev->card_type = SD_TYPE_V1;
            do {
                sd_cs_low();
                r1 = sd_send_acmd(dev, ACMD41, 0);
                sd_cs_high(dev);
                if (r1 == 0x00) break;
                HAL_Delay(10);
            } while ((HAL_GetTick() - t0) < 1000);
        } else {
            dev->card_type = SD_TYPE_MMC;
            do {
                sd_cs_low();
                r1 = sd_send_cmd(dev, 0x40 | 1, 0, 0xFF); /* CMD1 for MMC */
                sd_cs_high(dev);
                if (r1 == 0x00) break;
                HAL_Delay(10);
            } while ((HAL_GetTick() - t0) < 1000);
        }

        if (r1 != 0x00) return SD_ERR_TIMEOUT;

        /* Force 512-byte block length for byte-addressed cards */
        sd_cs_low();
        sd_send_cmd(dev, CMD16, SD_BLOCK_SIZE, 0xFF);
        sd_cs_high(dev);
    }

    /* Step 5: Read CSD to calculate capacity / sector count */
    sd_cs_low();
    r1 = sd_send_cmd(dev, CMD9, 0, 0xFF);
    if (r1 == 0x00) {
        /* Wait for data token */
        uint8_t token = 0xFF;
        t0 = HAL_GetTick();
        while ((HAL_GetTick() - t0) < 200) {
            token = sd_spi_xfer(dev, 0xFF);
            if (token == TOKEN_START_BLOCK) break;
        }

        if (token == TOKEN_START_BLOCK) {
            uint8_t csd[16];
            for (int i = 0; i < 16; i++) csd[i] = sd_spi_xfer(dev, 0xFF);
            sd_spi_xfer(dev, 0xFF); /* discard CRC byte 1 */
            sd_spi_xfer(dev, 0xFF); /* discard CRC byte 2 */

            if ((csd[0] >> 6) == 1) {
                /* CSD v2.0 (SDHC/SDXC) */
                uint32_t c_size = ((uint32_t)(csd[7] & 0x3F) << 16) |
                                  ((uint32_t)csd[8] << 8) | csd[9];
                dev->sector_count = (c_size + 1) * 1024;
            } else {
                /* CSD v1.0 (SDSC/MMC) */
                uint32_t c_size = ((uint32_t)(csd[6] & 0x03) << 10) |
                                  ((uint32_t)csd[7] << 2) |
                                  ((uint32_t)(csd[8] & 0xC0) >> 6);
                uint8_t c_size_mult = ((csd[9] & 0x03) << 1) | ((csd[10] & 0x80) >> 7);
                uint8_t read_bl_len = csd[5] & 0x0F;
                dev->sector_count = (c_size + 1) * (1 << (c_size_mult + 2)) * (1 << (read_bl_len - 9));
            }
        }
    }
    sd_cs_high(dev);

    /* Step 6: Switch SPI clock back to high speed (~10 MHz) */
    sd_set_speed(dev, SPI_BAUDRATEPRESCALER_16);

    dev->initialized = true;
    return SD_OK;
}

SD_Status SD_ReadSectors(SD_Dev *dev, uint8_t *buffer, uint32_t sector, uint32_t count)
{
    if (!dev->initialized || buffer == NULL || count == 0) return SD_ERR_PARAM;

    /* For byte-addressed cards (SDSC), multiply sector by 512 */
    uint32_t addr = (dev->card_type == SD_TYPE_V2HC) ? sector : (sector * SD_BLOCK_SIZE);

    sd_cs_low();

    if (count == 1) {
        /* Single block read */
        uint8_t r1 = sd_send_cmd(dev, CMD17, addr, 0xFF);
        if (r1 != 0x00) {
            sd_cs_high(dev);
            return SD_ERR_CMD_FAILED;
        }

        /* Wait for data token */
        uint32_t t0 = HAL_GetTick();
        uint8_t token = 0xFF;
        while ((HAL_GetTick() - t0) < 200) {
            token = sd_spi_xfer(dev, 0xFF);
            if (token == TOKEN_START_BLOCK) break;
        }
        if (token != TOKEN_START_BLOCK) {
            sd_cs_high(dev);
            return SD_ERR_TIMEOUT;
        }

        /* Read 512 data bytes */
        HAL_SPI_Receive(dev->hspi, buffer, SD_BLOCK_SIZE, 500);

        /* Discard 2 CRC bytes */
        sd_spi_xfer(dev, 0xFF);
        sd_spi_xfer(dev, 0xFF);
    } else {
        /* Multiple block read */
        uint8_t r1 = sd_send_cmd(dev, CMD18, addr, 0xFF);
        if (r1 != 0x00) {
            sd_cs_high(dev);
            return SD_ERR_CMD_FAILED;
        }

        uint8_t *ptr = buffer;
        for (uint32_t n = 0; n < count; n++) {
            uint32_t t0 = HAL_GetTick();
            uint8_t token = 0xFF;
            while ((HAL_GetTick() - t0) < 200) {
                token = sd_spi_xfer(dev, 0xFF);
                if (token == TOKEN_START_BLOCK) break;
            }
            if (token != TOKEN_START_BLOCK) {
                sd_send_cmd(dev, CMD12, 0, 0xFF);
                sd_cs_high(dev);
                return SD_ERR_TIMEOUT;
            }

            HAL_SPI_Receive(dev->hspi, ptr, SD_BLOCK_SIZE, 500);
            sd_spi_xfer(dev, 0xFF);
            sd_spi_xfer(dev, 0xFF);
            ptr += SD_BLOCK_SIZE;
        }

        /* Send CMD12 (Stop Transmission) */
        sd_send_cmd(dev, CMD12, 0, 0xFF);
    }

    sd_cs_high(dev);
    return SD_OK;
}

SD_Status SD_WriteSectors(SD_Dev *dev, const uint8_t *buffer, uint32_t sector, uint32_t count)
{
    if (!dev->initialized || buffer == NULL || count == 0) return SD_ERR_PARAM;

    uint32_t addr = (dev->card_type == SD_TYPE_V2HC) ? sector : (sector * SD_BLOCK_SIZE);

    sd_cs_low();

    if (count == 1) {
        /* Single block write */
        uint8_t r1 = sd_send_cmd(dev, CMD24, addr, 0xFF);
        if (r1 != 0x00) {
            sd_cs_high(dev);
            return SD_ERR_CMD_FAILED;
        }

        /* Send dummy byte, then Start Block token */
        sd_spi_xfer(dev, 0xFF);
        sd_spi_xfer(dev, TOKEN_START_BLOCK);

        /* Write 512 bytes */
        HAL_SPI_Transmit(dev->hspi, (uint8_t *)buffer, SD_BLOCK_SIZE, 500);

        /* Send 2 dummy CRC bytes */
        sd_spi_xfer(dev, 0xFF);
        sd_spi_xfer(dev, 0xFF);

        /* Read data response token */
        uint8_t resp = sd_spi_xfer(dev, 0xFF);
        if ((resp & 0x1F) != 0x05) { /* 0x05 = Data accepted */
            sd_cs_high(dev);
            return SD_ERR_DATA_REJECTED;
        }

        /* Wait until card finishes internal programming */
        if (SD_WaitReady(dev, SD_WAIT_READY_TIMEOUT) != SD_OK) {
            sd_cs_high(dev);
            return SD_ERR_TIMEOUT;
        }
    } else {
        /* Multiple block write */
        uint8_t r1 = sd_send_cmd(dev, CMD25, addr, 0xFF);
        if (r1 != 0x00) {
            sd_cs_high(dev);
            return SD_ERR_CMD_FAILED;
        }

        const uint8_t *ptr = buffer;
        for (uint32_t n = 0; n < count; n++) {
            sd_spi_xfer(dev, 0xFF);
            sd_spi_xfer(dev, TOKEN_START_MULTIBLOCK);

            HAL_SPI_Transmit(dev->hspi, (uint8_t *)ptr, SD_BLOCK_SIZE, 500);
            sd_spi_xfer(dev, 0xFF);
            sd_spi_xfer(dev, 0xFF);

            uint8_t resp = sd_spi_xfer(dev, 0xFF);
            if ((resp & 0x1F) != 0x05) {
                sd_cs_high(dev);
                return SD_ERR_DATA_REJECTED;
            }

            if (SD_WaitReady(dev, SD_WAIT_READY_TIMEOUT) != SD_OK) {
                sd_cs_high(dev);
                return SD_ERR_TIMEOUT;
            }
            ptr += SD_BLOCK_SIZE;
        }

        /* Send Stop Tran token */
        sd_spi_xfer(dev, TOKEN_STOP_TRAN);
        sd_spi_xfer(dev, 0xFF);
        SD_WaitReady(dev, SD_WAIT_READY_TIMEOUT);
    }

    sd_cs_high(dev);
    return SD_OK;
}

uint32_t SD_GetSectorCount(const SD_Dev *dev)
{
    return dev->sector_count;
}

bool SD_IsInitialized(const SD_Dev *dev)
{
    return dev->initialized;
}
