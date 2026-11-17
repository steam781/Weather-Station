/**
  ******************************************************************************
  * @file    ltr390.c
  * @brief   LTR-390UV-01 ambient light / UV sensor driver for STM32 HAL (I2C).
  *
  *          The sensor shares a single ADC for ALS and UVS, selectable via a
  *          mode bit in MAIN_CTRL.  This driver reads ALS first, then switches
  *          to UVS mode and reads again, so a full LTR390_Read() takes roughly
  *          two conversion periods (~200 ms at 18-bit resolution).
  *
  *          Reference: Lite-On LTR-390UV-01 datasheet (rev 1.1) §§ 5–6.
  ******************************************************************************
  */

#include "ltr390.h"
#include "app_config.h"
#include <string.h>

/* ════════════════════════════════════════════════════════════════════════════
 *  Gain & integration-time look-up tables (register value → actual factor)
 * ════════════════════════════════════════════════════════════════════════════ */

static const float gain_factor[] = { 1.0f, 3.0f, 6.0f, 9.0f, 18.0f };

/* Integration time in multiples of 100 ms (used as the divisor "T" in the
 * lux / UVI formulas from the datasheet).                                   */
static const float integ_factor[] = { 4.0f, 2.0f, 1.0f, 0.5f, 0.25f };

/* ════════════════════════════════════════════════════════════════════════════
 *  Private helpers — I2C register access
 * ════════════════════════════════════════════════════════════════════════════ */

static HAL_StatusTypeDef ltr_read(const LTR390_Dev *dev, uint8_t reg,
                                  uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(dev->hi2c, (uint16_t)(dev->addr << 1),
                            reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef ltr_write8(const LTR390_Dev *dev, uint8_t reg,
                                    uint8_t val)
{
    return HAL_I2C_Mem_Write(dev->hi2c, (uint16_t)(dev->addr << 1),
                             reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1, I2C_TIMEOUT_MS);
}

/* ── Wait for the DATA_RDY flag with timeout ─────────────────────────────── */
static LTR390_Status ltr_wait_data(const LTR390_Dev *dev)
{
    uint32_t t0 = HAL_GetTick();
    uint8_t  st;
    do {
        HAL_Delay(10);
        if (ltr_read(dev, LTR390_REG_MAIN_STATUS, &st, 1) != HAL_OK)
            return LTR390_ERR_I2C;
        if (st & LTR390_STATUS_DATA_RDY)
            return LTR390_OK;
    } while ((HAL_GetTick() - t0) < LTR390_CONV_WAIT_MS);

    return LTR390_ERR_TIMEOUT;
}

/* ── Read 20-bit little-endian data from a 3-byte register group ─────────── */
static LTR390_Status ltr_read20(const LTR390_Dev *dev, uint8_t base_reg,
                                uint32_t *out)
{
    uint8_t buf[3];
    if (ltr_read(dev, base_reg, buf, 3) != HAL_OK) return LTR390_ERR_I2C;
    *out = ((uint32_t)(buf[2] & 0x0F) << 16) | ((uint32_t)buf[1] << 8) | buf[0];
    return LTR390_OK;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Public API
 * ════════════════════════════════════════════════════════════════════════════ */

LTR390_Status LTR390_Init(LTR390_Dev *dev, I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    memset(dev, 0, sizeof(*dev));
    dev->hi2c       = hi2c;
    dev->addr       = addr;
    dev->als_gain   = LTR390_ALS_GAIN;
    dev->uvs_gain   = LTR390_UVS_GAIN;
    dev->resolution = LTR390_RESOLUTION;

    /* Verify part ID (upper nibble should be 0xB, revision in lower nibble) */
    uint8_t id = 0;
    if (ltr_read(dev, LTR390_REG_PART_ID, &id, 1) != HAL_OK)
        return LTR390_ERR_I2C;
    if ((id & 0xF0) != (LTR390_PART_ID_VAL & 0xF0))
        return LTR390_ERR_PART_ID;

    /* Set measurement rate / resolution */
    uint8_t meas_rate = ((dev->resolution & 0x07) << 4) | (LTR390_MEAS_RATE & 0x07);
    if (ltr_write8(dev, LTR390_REG_MEAS_RATE, meas_rate) != HAL_OK)
        return LTR390_ERR_I2C;

    /* Set initial gain (ALS) */
    if (ltr_write8(dev, LTR390_REG_GAIN, dev->als_gain) != HAL_OK)
        return LTR390_ERR_I2C;

    /* Enable sensor in ALS mode */
    if (ltr_write8(dev, LTR390_REG_MAIN_CTRL, LTR390_CTRL_EN) != HAL_OK)
        return LTR390_ERR_I2C;

    /* Wait for the first ALS conversion so subsequent reads are valid */
    HAL_Delay(120);

    return LTR390_OK;
}

LTR390_Status LTR390_Read(LTR390_Dev *dev)
{
    LTR390_Status st;
    float gf, tf;

    /* ── 1. ALS reading ────────────────────────────────────────────────── */

    /* Set ALS gain & ALS mode + enable */
    if (ltr_write8(dev, LTR390_REG_GAIN, dev->als_gain) != HAL_OK)
        return LTR390_ERR_I2C;
    if (ltr_write8(dev, LTR390_REG_MAIN_CTRL, LTR390_CTRL_EN) != HAL_OK)
        return LTR390_ERR_I2C;

    st = ltr_wait_data(dev);
    if (st != LTR390_OK) return st;

    st = ltr_read20(dev, LTR390_REG_ALS_DATA_0, &dev->data.als_raw);
    if (st != LTR390_OK) return st;

    /* Check for ADC saturation based on configured resolution bit depth */
    static const uint32_t max_counts[] = { 1048575, 524287, 262143, 131071, 65535 };
    uint8_t res_idx = dev->resolution > 4 ? 2 : dev->resolution;
    dev->data.saturated = (dev->data.als_raw >= (max_counts[res_idx] - 10));

    /* Lux = 0.6 × ALS_DATA / (GAIN × INT_FACTOR) × WFAC
     * WFAC = 1 (no window cover).                                          */
    gf = gain_factor[dev->als_gain];
    tf = integ_factor[dev->resolution];
    dev->data.lux = 0.6f * (float)dev->data.als_raw / (gf * tf);

    /* ── 2. UVS reading ────────────────────────────────────────────────── */

    /* Switch to UVS gain & UVS mode + enable */
    if (ltr_write8(dev, LTR390_REG_GAIN, dev->uvs_gain) != HAL_OK)
        return LTR390_ERR_I2C;
    if (ltr_write8(dev, LTR390_REG_MAIN_CTRL, LTR390_CTRL_EN | LTR390_CTRL_UVS_MODE) != HAL_OK)
        return LTR390_ERR_I2C;

    st = ltr_wait_data(dev);
    if (st != LTR390_OK) return st;

    st = ltr_read20(dev, LTR390_REG_UVS_DATA_0, &dev->data.uvs_raw);
    if (st != LTR390_OK) return st;

    /* UV Index = UVS_DATA / UV_SENSITIVITY
     * UV_SENSITIVITY = 2300 at gain=18×, resolution=20-bit (400 ms).
     * Scale for current settings: sens = 2300 × (G/18) × (T/4).            */
    gf = gain_factor[dev->uvs_gain];
    tf = integ_factor[dev->resolution];
    float sens = 2300.0f * (gf / 18.0f) * (tf / 4.0f);
    dev->data.uv_index = (float)dev->data.uvs_raw / sens;

    /* ── 3. Return to ALS mode (standby for next read) ─────────────────── */
    if (ltr_write8(dev, LTR390_REG_GAIN, dev->als_gain) != HAL_OK)
        return LTR390_ERR_I2C;
    if (ltr_write8(dev, LTR390_REG_MAIN_CTRL, LTR390_CTRL_EN) != HAL_OK)
        return LTR390_ERR_I2C;

    return LTR390_OK;
}

LTR390_Status LTR390_Standby(LTR390_Dev *dev)
{
    /* Clear enable bit in MAIN_CTRL to shut down internal oscillator (< 1 µA) */
    if (ltr_write8(dev, LTR390_REG_MAIN_CTRL, 0x00) != HAL_OK)
        return LTR390_ERR_I2C;
    return LTR390_OK;
}

LTR390_Status LTR390_Wakeup(LTR390_Dev *dev)
{
    /* Re-enable sensor in ALS mode */
    if (ltr_write8(dev, LTR390_REG_GAIN, dev->als_gain) != HAL_OK)
        return LTR390_ERR_I2C;
    if (ltr_write8(dev, LTR390_REG_MAIN_CTRL, LTR390_CTRL_EN) != HAL_OK)
        return LTR390_ERR_I2C;
    HAL_Delay(100);
    return LTR390_OK;
}

LTR390_Status LTR390_SetGain(LTR390_Dev *dev, uint8_t als_gain, uint8_t uvs_gain)
{
    if (als_gain > 4 || uvs_gain > 4) return LTR390_ERR_I2C;
    dev->als_gain = als_gain;
    dev->uvs_gain = uvs_gain;
    return ltr_write8(dev, LTR390_REG_GAIN, dev->als_gain) == HAL_OK ? LTR390_OK : LTR390_ERR_I2C;
}

const LTR390_Data *LTR390_GetData(const LTR390_Dev *dev)
{
    return &dev->data;
}
