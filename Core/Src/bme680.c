/**
  ******************************************************************************
  * @file    bme680.c
  * @brief   BME680 environmental sensor driver — forced-mode T/P/H/Gas readout
  *          with float compensation (Cortex-M4 single-precision FPU).
  *
  *          Reference: Bosch BME680 datasheet (BST-BME680-DS001-00) §§ 3.3–3.4.
  *          Compensation formulas are the floating-point variants from the Bosch
  *          BME68x API (v4.4.8) for readability on an FPU-equipped core.
  ******************************************************************************
  */

#include "bme680.h"
#include "app_config.h"
#include <string.h>
#include <math.h>

/* ════════════════════════════════════════════════════════════════════════════
 *  Private helpers — I2C register access
 * ════════════════════════════════════════════════════════════════════════════ */

static HAL_StatusTypeDef bme_read(const BME680_Dev *dev, uint8_t reg,
                                  uint8_t *buf, uint16_t len)
{
    return HAL_I2C_Mem_Read(dev->hi2c, (uint16_t)(dev->addr << 1),
                            reg, I2C_MEMADD_SIZE_8BIT,
                            buf, len, I2C_TIMEOUT_MS);
}

static HAL_StatusTypeDef bme_write8(const BME680_Dev *dev, uint8_t reg,
                                    uint8_t val)
{
    return HAL_I2C_Mem_Write(dev->hi2c, (uint16_t)(dev->addr << 1),
                             reg, I2C_MEMADD_SIZE_8BIT,
                             &val, 1, I2C_TIMEOUT_MS);
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Calibration data extraction
 * ════════════════════════════════════════════════════════════════════════════ */

static BME680_Status bme_read_calib(BME680_Dev *dev)
{
    uint8_t c1[BME680_REG_COEFF1_LEN];   /* 0x89 – 0xA1 → index 0..24  */
    uint8_t c2[BME680_REG_COEFF2_LEN];   /* 0xE1 – 0xF0 → index 25..40 */
    uint8_t tmp;

    if (bme_read(dev, BME680_REG_COEFF1, c1, sizeof(c1)) != HAL_OK) return BME680_ERR_I2C;
    if (bme_read(dev, BME680_REG_COEFF2, c2, sizeof(c2)) != HAL_OK) return BME680_ERR_I2C;

    /*
     * Combined coefficient array layout used by the Bosch API:
     *   index  0..24 = c1[0..24]   (registers 0x89–0xA1)
     *   index 25..40 = c2[0..15]   (registers 0xE1–0xF0)
     *
     * Below we use c1[i] for index 0..24 and c2[i-25] for index 25..40.
     */

    BME680_CalibData *cal = &dev->calib;

    /* Temperature */
    cal->par_t1 = (uint16_t)(c2[9]  << 8) | c2[8];         /* idx 34,33 */
    cal->par_t2 = (int16_t)((c1[2]  << 8) | c1[1]);        /* idx  2, 1 */
    cal->par_t3 = (int8_t)c1[3];                            /* idx  3    */

    /* Pressure */
    cal->par_p1  = (uint16_t)(c1[6]  << 8) | c1[5];
    cal->par_p2  = (int16_t)((c1[8]  << 8) | c1[7]);
    cal->par_p3  = (int8_t)c1[9];
    cal->par_p4  = (int16_t)((c1[12] << 8) | c1[11]);
    cal->par_p5  = (int16_t)((c1[14] << 8) | c1[13]);
    cal->par_p6  = (int8_t)c1[16];
    cal->par_p7  = (int8_t)c1[15];
    cal->par_p8  = (int16_t)((c1[20] << 8) | c1[19]);
    cal->par_p9  = (int16_t)((c1[22] << 8) | c1[21]);
    cal->par_p10 = c1[23];

    /* Humidity — note the nibble-shared byte at index 26 (c2[1]) */
    cal->par_h1 = (uint16_t)((c2[2] << 4) | (c2[1] & 0x0F));  /* idx 27,26 */
    cal->par_h2 = (uint16_t)((c2[0] << 4) | (c2[1] >> 4));     /* idx 25,26 */
    cal->par_h3 = (int8_t)c2[3];
    cal->par_h4 = (int8_t)c2[4];
    cal->par_h5 = (int8_t)c2[5];
    cal->par_h6 = c2[6];
    cal->par_h7 = (int8_t)c2[7];

    /* Gas heater */
    cal->par_gh1 = (int8_t)c2[12];                           /* idx 37 */
    cal->par_gh2 = (int16_t)((c2[11] << 8) | c2[10]);        /* idx 36,35 */
    cal->par_gh3 = (int8_t)c2[13];                            /* idx 38 */

    /* Additional calibration from standalone registers */
    if (bme_read(dev, BME680_REG_RES_HEAT_RANGE, &tmp, 1) != HAL_OK) return BME680_ERR_I2C;
    cal->res_heat_range = (tmp >> 4) & 0x03;

    if (bme_read(dev, BME680_REG_RES_HEAT_VAL, &tmp, 1) != HAL_OK) return BME680_ERR_I2C;
    cal->res_heat_val = (int8_t)tmp;

    if (bme_read(dev, BME680_REG_RANGE_SW_ERR, &tmp, 1) != HAL_OK) return BME680_ERR_I2C;
    cal->range_sw_err = (int8_t)((int8_t)tmp) >> 4;  /* signed nibble */

    return BME680_OK;
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Heater target resistance & wait-time encoding
 * ════════════════════════════════════════════════════════════════════════════ */

static uint8_t calc_res_heat(const BME680_CalibData *cal, uint16_t target_temp,
                             int16_t amb_temp_x100)
{
    float amb = (float)amb_temp_x100 / 100.0f;
    float var1 = ((float)cal->par_gh1 / 16.0f) + 49.0f;
    float var2 = (((float)cal->par_gh2 / 32768.0f) * 0.0005f) + 0.00235f;
    float var3 = (float)cal->par_gh3 / 1024.0f;
    float var4 = var1 * (1.0f + (var2 * (float)target_temp));
    float var5 = var4 + (var3 * amb);
    float res  = 3.4f * ((var5 * (4.0f / (4.0f + (float)cal->res_heat_range))
                 * (1.0f / (1.0f + ((float)cal->res_heat_val * 0.002f)))) - 25.0f);
    return (uint8_t)res;
}

static uint8_t calc_gas_wait(uint16_t dur_ms)
{
    uint8_t factor = 0;
    uint16_t dur = dur_ms;
    if (dur >= 0xFC0) return 0xFF;
    while (dur > 0x3F) {
        dur /= 4;
        factor++;
    }
    return (uint8_t)(dur + (factor * 64));
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Compensation: ADC → physical units (float, Bosch datasheet formulas)
 * ════════════════════════════════════════════════════════════════════════════ */

static float comp_temperature(BME680_CalibData *cal, uint32_t adc_temp)
{
    float var1 = ((float)adc_temp / 16384.0f) - ((float)cal->par_t1 / 1024.0f);
    float var2 = var1 * (float)cal->par_t2;
    float var3 = (var1 * var1) / 16384.0f * (float)cal->par_t3;
    cal->t_fine = var2 + var3;                     /* store for P & H comp */
    return cal->t_fine / 5120.0f;                  /* °C                   */
}

static float comp_pressure(const BME680_CalibData *cal, uint32_t adc_pres)
{
    float var1 = (cal->t_fine / 2.0f) - 64000.0f;
    float var2 = var1 * var1 * ((float)cal->par_p6 / 131072.0f);
    var2 += var1 * (float)cal->par_p5 * 2.0f;
    var2 = (var2 / 4.0f) + ((float)cal->par_p4 * 65536.0f);
    var1 = ((((float)cal->par_p3 * var1 * var1) / 16384.0f)
           + ((float)cal->par_p2 * var1)) / 524288.0f;
    var1 = (1.0f + var1 / 32768.0f) * (float)cal->par_p1;
    if (var1 == 0.0f) return 0.0f;                 /* avoid division by 0 */

    float press = 1048576.0f - (float)adc_pres;
    press = ((press - var2 / 4096.0f) * 6250.0f) / var1;
    var1 = ((float)cal->par_p9 * press * press) / 2147483648.0f;
    var2 = press * ((float)cal->par_p8 / 32768.0f);
    float var3 = (press / 256.0f) * (press / 256.0f) * (press / 256.0f)
                 * ((float)cal->par_p10 / 131072.0f);
    press += (var1 + var2 + var3 + ((float)cal->par_p7 * 128.0f)) / 16.0f;
    return press / 100.0f;                         /* Pa → hPa            */
}

static float comp_humidity(const BME680_CalibData *cal, uint16_t adc_hum,
                           float temp_comp)
{
    float var1 = (float)adc_hum
                 - (((float)cal->par_h1 * 16.0f)
                    + (((float)cal->par_h3 / 2.0f) * temp_comp));
    float var2 = var1
                 * (((float)cal->par_h2 / 262144.0f)
                    * (1.0f + (((float)cal->par_h4 / 16384.0f) * temp_comp)
                       + (((float)cal->par_h5 / 1048576.0f)
                          * temp_comp * temp_comp)));
    float var3 = (float)cal->par_h6 / 16384.0f;
    float var4 = (float)cal->par_h7 / 2097152.0f;
    float hum  = var2 + ((var3 + (var4 * temp_comp)) * var2 * var2);

    if (hum > 100.0f) hum = 100.0f;
    if (hum <   0.0f) hum =   0.0f;
    return hum;
}

static float comp_gas(const BME680_CalibData *cal, uint16_t adc_gas,
                      uint8_t gas_range)
{
    static const float k1[16] = {
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f,-1.0f, 0.0f,-0.8f,
        0.0f, 0.0f,-0.2f,-0.5f, 0.0f,-1.0f, 0.0f, 0.0f
    };
    static const float k2[16] = {
        0.0f, 0.0f, 0.0f, 0.0f, 0.1f, 0.7f, 0.0f,-0.8f,
       -0.1f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f
    };

    float var1 = 1340.0f + 5.0f * (float)cal->range_sw_err;
    float var2 = var1 * (1.0f + k1[gas_range] / 100.0f);
    float var3 = 1.0f + (k2[gas_range] / 100.0f);
    float gas  = 1.0f / (var3 * 0.000000125f * (float)(1 << gas_range)
                 * ((((float)adc_gas - 512.0f) / var2) + 1.0f));
    return gas;                                    /* Ω                    */
}

/* ════════════════════════════════════════════════════════════════════════════
 *  Public API
 * ════════════════════════════════════════════════════════════════════════════ */

BME680_Status BME680_Init(BME680_Dev *dev, I2C_HandleTypeDef *hi2c, uint8_t addr)
{
    memset(dev, 0, sizeof(*dev));
    dev->hi2c = hi2c;
    dev->addr = addr;

    /* Soft reset */
    if (bme_write8(dev, BME680_REG_SOFT_RESET, BME680_SOFT_RESET_CMD) != HAL_OK)
        return BME680_ERR_I2C;
    HAL_Delay(10);

    /* Verify chip ID */
    uint8_t id = 0;
    if (bme_read(dev, BME680_REG_CHIP_ID, &id, 1) != HAL_OK)
        return BME680_ERR_I2C;
    if (id != BME680_CHIP_ID)
        return BME680_ERR_CHIP_ID;

    /* Read calibration data */
    BME680_Status st = bme_read_calib(dev);
    if (st != BME680_OK) return st;

    /* Configure humidity oversampling (must be written before ctrl_meas) */
    if (bme_write8(dev, BME680_REG_CTRL_HUM, BME680_OS_HUM) != HAL_OK)
        return BME680_ERR_I2C;

    /* IIR filter off (config register, bits [4:2]) */
    if (bme_write8(dev, BME680_REG_CONFIG, 0x00) != HAL_OK)
        return BME680_ERR_I2C;

    return BME680_OK;
}

BME680_Status BME680_ReadForced(BME680_Dev *dev, bool enable_gas)
{
    BME680_CalibData *cal = &dev->calib;

    /* ── 1. Configure gas heater (set-point 0) ─────────────────────────── */
    if (enable_gas) {
        /* Use actual ambient temperature if valid, otherwise fallback to 25 °C */
        int16_t amb_temp_x100 = 2500;
        if (dev->data.temperature >= -40.0f && dev->data.temperature <= 85.0f) {
            amb_temp_x100 = (int16_t)(dev->data.temperature * 100.0f);
        }
        uint8_t rh = calc_res_heat(cal, BME680_GAS_TEMP, amb_temp_x100);
        if (bme_write8(dev, BME680_REG_RES_HEAT0, rh) != HAL_OK)
            return BME680_ERR_I2C;

        uint8_t gw = calc_gas_wait(BME680_GAS_DUR);
        if (bme_write8(dev, BME680_REG_GAS_WAIT0, gw) != HAL_OK)
            return BME680_ERR_I2C;

        /* Enable gas, select heater set-point 0 (nb_conv = 0) */
        if (bme_write8(dev, BME680_REG_CTRL_GAS1, 0x10) != HAL_OK)
            return BME680_ERR_I2C;
    } else {
        /* Disable gas measurement */
        if (bme_write8(dev, BME680_REG_CTRL_GAS1, 0x00) != HAL_OK)
            return BME680_ERR_I2C;
    }

    /* ── 2. Set oversampling + forced mode ─────────────────────────────── */
    uint8_t ctrl = (BME680_OS_TEMP << 5) | (BME680_OS_PRES << 2) | BME680_MODE_FORCED;
    if (bme_write8(dev, BME680_REG_CTRL_MEAS, ctrl) != HAL_OK)
        return BME680_ERR_I2C;

    /* ── 3. Poll new_data_0 bit until conversion is done ───────────────── */
    uint32_t t0 = HAL_GetTick();
    uint8_t  status;
    do {
        HAL_Delay(10);
        if (bme_read(dev, BME680_REG_MEAS_STATUS, &status, 1) != HAL_OK)
            return BME680_ERR_I2C;
        if (status & 0x80) break;                  /* new_data_0 set       */
    } while ((HAL_GetTick() - t0) < BME680_MEAS_WAIT_MS);

    if (!(status & 0x80))
        return BME680_ERR_TIMEOUT;

    /* ── 4. Burst-read raw ADC data: 0x1D – 0x2B (15 bytes) ───────────── */
    uint8_t raw[15];
    if (bme_read(dev, BME680_REG_MEAS_STATUS, raw, sizeof(raw)) != HAL_OK)
        return BME680_ERR_I2C;

    /* Offsets relative to 0x1D:
     *   Press  MSB/LSB/XLSB = raw[2]/raw[3]/raw[4]   (0x1F/0x20/0x21)
     *   Temp   MSB/LSB/XLSB = raw[5]/raw[6]/raw[7]   (0x22/0x23/0x24)
     *   Hum    MSB/LSB      = raw[8]/raw[9]           (0x25/0x26)
     *   Gas    MSB/LSB      = raw[13]/raw[14]         (0x2A/0x2B)
     */
    uint32_t adc_pres = ((uint32_t)raw[2] << 12) | ((uint32_t)raw[3] << 4) | (raw[4] >> 4);
    uint32_t adc_temp = ((uint32_t)raw[5] << 12) | ((uint32_t)raw[6] << 4) | (raw[7] >> 4);
    uint16_t adc_hum  = ((uint16_t)raw[8] << 8)  |  raw[9];
    uint16_t adc_gas  = ((uint16_t)raw[13] << 2) | (raw[14] >> 6);
    uint8_t  gas_range = raw[14] & 0x0F;
    bool     gas_valid = (raw[14] >> 5) & 0x01;
    bool     heat_stab = (raw[14] >> 4) & 0x01;

    /* ── 5. Compensate ─────────────────────────────────────────────────── */
    dev->data.temperature    = comp_temperature(cal, adc_temp);
    dev->data.pressure       = comp_pressure(cal, adc_pres);
    dev->data.humidity       = comp_humidity(cal, adc_hum, dev->data.temperature);
    dev->data.gas_valid      = enable_gas && gas_valid && heat_stab;
    dev->data.gas_resistance = dev->data.gas_valid
                               ? comp_gas(cal, adc_gas, gas_range)
                               : 0.0f;

    /* ── 6. Dew Point calculation (Magnus formula) ─────────────────────── */
    float temp = dev->data.temperature;
    float hum  = dev->data.humidity;
    if (hum > 0.0f) {
        float alpha = ((17.27f * temp) / (237.7f + temp)) + logf(hum / 100.0f);
        dev->data.dew_point = (237.7f * alpha) / (17.27f - alpha);
    } else {
        dev->data.dew_point = temp;
    }

    /* ── 7. Indoor Air Quality (IAQ) Index Estimation (0–500 scale) ────── */
    if (dev->data.gas_valid && dev->data.gas_resistance > 0.0f) {
        /* Update clean-air baseline resistance (slow upward tracking) */
        if (dev->gas_baseline <= 0.0f) {
            dev->gas_baseline = dev->data.gas_resistance;
        } else if (dev->data.gas_resistance > dev->gas_baseline) {
            dev->gas_baseline = (dev->gas_baseline * 0.9f) + (dev->data.gas_resistance * 0.1f);
        }

        /* Gas ratio contribution (75% weight) */
        float gas_ratio = dev->data.gas_resistance / dev->gas_baseline;
        if (gas_ratio > 1.0f) gas_ratio = 1.0f;
        if (gas_ratio < 0.0f) gas_ratio = 0.0f;

        /* Humidity deviation contribution (25% weight, target 40% RH) */
        float hum_ratio;
        if (hum >= 40.0f) {
            hum_ratio = 1.0f - ((hum - 40.0f) / 60.0f);
        } else {
            hum_ratio = 1.0f - ((40.0f - hum) / 40.0f);
        }
        if (hum_ratio > 1.0f) hum_ratio = 1.0f;
        if (hum_ratio < 0.0f) hum_ratio = 0.0f;

        /* Combined clean air fraction: 1.0 = best, 0.0 = worst */
        float clean_fraction = (0.75f * gas_ratio) + (0.25f * hum_ratio);

        /* Map to 0–500 standard EPA AQI scale */
        dev->data.iaq_score = (1.0f - clean_fraction) * 500.0f;

        if (dev->data.iaq_score <= 50.0f)       dev->data.iaq_text = "Good";
        else if (dev->data.iaq_score <= 100.0f) dev->data.iaq_text = "Moderate";
        else if (dev->data.iaq_score <= 150.0f) dev->data.iaq_text = "Sensitive";
        else if (dev->data.iaq_score <= 200.0f) dev->data.iaq_text = "Unhealthy";
        else if (dev->data.iaq_score <= 300.0f) dev->data.iaq_text = "Very Unhealthy";
        else                                    dev->data.iaq_text = "Hazardous";
    } else {
        dev->data.iaq_score = 0.0f;
        dev->data.iaq_text  = "N/A";
    }

    return BME680_OK;
}

BME680_Status BME680_Sleep(BME680_Dev *dev)
{
    /* Write 0x00 to ctrl_meas to put sensor into sleep mode */
    if (bme_write8(dev, BME680_REG_CTRL_MEAS, BME680_MODE_SLEEP) != HAL_OK)
        return BME680_ERR_I2C;
    return BME680_OK;
}

float BME680_CalcSeaLevelPressure(float station_press_hpa, float altitude_m, float temp_c)
{
    /* Hypsometric equation for sea-level pressure (QNH) */
    float temp_k = temp_c + 273.15f;
    return station_press_hpa * powf(1.0f - (0.0065f * altitude_m) / (temp_k + 0.0065f * altitude_m), -5.257f);
}

const BME680_Data *BME680_GetData(const BME680_Dev *dev)
{
    return &dev->data;
}
