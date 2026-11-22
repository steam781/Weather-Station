/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "app_fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "app_config.h"
#include "bme680.h"
#include "ltr390.h"
#include "ili9341.h"
#include "sd_spi.h"
#include "sd_logger.h"
#include <stdio.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define FW_VERSION  "1.0.0"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

COM_InitTypeDef BspCOMInit;

I2C_HandleTypeDef hi2c2;

RTC_HandleTypeDef hrtc;

SPI_HandleTypeDef hspi1;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
static BME680_Dev   bme680;
static LTR390_Dev   ltr390;
static ILI9341_Dev  tft;
SD_Dev              sd_card;
static uint32_t     lastSensorRead = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_RTC_Init(void);
static void MX_SPI1_Init(void);
static void MX_TIM2_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */
static void I2C_BusScan(I2C_HandleTypeDef *hi2c);
static void TFT_DrawDashboardLayout(void);
static void TFT_UpdateDashboardValues(const BME680_Data *bd, const LTR390_Data *ld);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* ── I2C bus scan: probe every 7-bit address and print results ──────────── */
static void I2C_BusScan(I2C_HandleTypeDef *hi2c)
{
    printf("I2C1 bus scan (0x01 – 0x7F):\r\n");
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 128; addr++) {
        if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(addr << 1), 1, 10) == HAL_OK) {
            printf("  [0x%02X] ACK", addr);
            if (addr == BME680_I2C_ADDR) printf(" ← BME680");
            if (addr == LTR390_I2C_ADDR) printf(" ← LTR390");
            printf("\r\n");
            found++;
        }
    }
    printf("  %u device(s) found.\r\n\r\n", found);
}

/* ── Draw static dashboard layout on ILI9341 ────────────────────────────── */
static void TFT_DrawDashboardLayout(void)
{
    /* Clear screen to dark navy */
    ILI9341_FillScreen(&tft, ILI9341_BLACK);

    /* Header Bar */
    ILI9341_FillRect(&tft, 0, 0, tft.width, 30, ILI9341_NAVY);
    ILI9341_DrawString(&tft, 10, 8, "WEATHER STATION v1.0", ILI9341_WHITE, ILI9341_NAVY, 2);

    /* Cards Outline / Divider Lines */
    ILI9341_DrawHLine(&tft, 0, 31, tft.width, ILI9341_DARKCYAN);
    ILI9341_DrawHLine(&tft, 0, 130, tft.width, ILI9341_DARKGREY);
    ILI9341_DrawVLine(&tft, 160, 32, 198, ILI9341_DARKGREY);
    ILI9341_DrawHLine(&tft, 0, 230, tft.width, ILI9341_DARKCYAN);

    /* Section Labels */
    /* Left column */
    ILI9341_DrawString(&tft, 10, 38, "TEMPERATURE", ILI9341_CYAN, ILI9341_BLACK, 1);
    ILI9341_DrawString(&tft, 10, 84, "HUMIDITY", ILI9341_CYAN, ILI9341_BLACK, 1);
    ILI9341_DrawString(&tft, 10, 138, "PRESSURE", ILI9341_CYAN, ILI9341_BLACK, 1);
    ILI9341_DrawString(&tft, 10, 184, "DEW POINT", ILI9341_CYAN, ILI9341_BLACK, 1);

    /* Right column */
    ILI9341_DrawString(&tft, 170, 38, "AMBIENT LIGHT", ILI9341_YELLOW, ILI9341_BLACK, 1);
    ILI9341_DrawString(&tft, 170, 84, "UV INDEX", ILI9341_YELLOW, ILI9341_BLACK, 1);
    ILI9341_DrawString(&tft, 170, 138, "AIR QUALITY (IAQ)", ILI9341_GREEN, ILI9341_BLACK, 1);
    ILI9341_DrawString(&tft, 170, 184, "GAS RESISTANCE", ILI9341_GREEN, ILI9341_BLACK, 1);
}

/* ── Update live values on ILI9341 dashboard ────────────────────────────── */
static void TFT_UpdateDashboardValues(const BME680_Data *bd, const LTR390_Data *ld)
{
    char buf[32];

    /* Temperature */
    snprintf(buf, sizeof(buf), "%5.1f C", bd->temperature);
    ILI9341_DrawString(&tft, 10, 52, buf, ILI9341_WHITE, ILI9341_BLACK, 2);

    /* Humidity */
    snprintf(buf, sizeof(buf), "%5.1f %%", bd->humidity);
    ILI9341_DrawString(&tft, 10, 98, buf, ILI9341_WHITE, ILI9341_BLACK, 2);

    /* Pressure */
    snprintf(buf, sizeof(buf), "%6.1f hPa", bd->pressure);
    ILI9341_DrawString(&tft, 10, 152, buf, ILI9341_WHITE, ILI9341_BLACK, 2);

    /* Dew Point */
    snprintf(buf, sizeof(buf), "%5.1f C", bd->dew_point);
    ILI9341_DrawString(&tft, 10, 198, buf, ILI9341_WHITE, ILI9341_BLACK, 2);

    /* Ambient Light (Lux) */
    snprintf(buf, sizeof(buf), "%5.0f Lux", ld->lux);
    ILI9341_DrawString(&tft, 170, 52, buf, ILI9341_WHITE, ILI9341_BLACK, 2);

    /* UV Index */
    snprintf(buf, sizeof(buf), "%4.1f UV ", ld->uv_index);
    ILI9341_DrawString(&tft, 170, 98, buf, ILI9341_WHITE, ILI9341_BLACK, 2);

    /* Air Quality (IAQ) */
    if (bd->gas_valid) {
        snprintf(buf, sizeof(buf), "%3.0f %-6s", bd->iaq_score, bd->iaq_text);
        uint16_t iaq_color = (bd->iaq_score <= 50.0f) ? ILI9341_GREEN :
                             (bd->iaq_score <= 100.0f) ? ILI9341_YELLOW : ILI9341_ORANGE;
        ILI9341_DrawString(&tft, 170, 152, buf, iaq_color, ILI9341_BLACK, 2);
    } else {
        ILI9341_DrawString(&tft, 170, 152, "Warmup... ", ILI9341_DARKGREY, ILI9341_BLACK, 2);
    }

    /* Gas Resistance */
    if (bd->gas_valid) {
        snprintf(buf, sizeof(buf), "%5.1f kOhm", bd->gas_resistance / 1000.0f);
        ILI9341_DrawString(&tft, 170, 198, buf, ILI9341_WHITE, ILI9341_BLACK, 2);
    } else {
        ILI9341_DrawString(&tft, 170, 198, "--------- ", ILI9341_DARKGREY, ILI9341_BLACK, 2);
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_RTC_Init();
  MX_SPI1_Init();
  MX_TIM2_Init();
  if (MX_FATFS_Init() != APP_OK) {
    Error_Handler();
  }
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Initialize leds */
  BSP_LED_Init(LED_GREEN);

  /* Initialize COM1 port (115200, 8 bits (7-bit data + 1 stop bit), no parity */
  BspCOMInit.BaudRate   = 115200;
  BspCOMInit.WordLength = COM_WORDLENGTH_8B;
  BspCOMInit.StopBits   = COM_STOPBITS_1;
  BspCOMInit.Parity     = COM_PARITY_NONE;
  BspCOMInit.HwFlowCtl  = COM_HWCONTROL_NONE;
  if (BSP_COM_Init(COM1, &BspCOMInit) != BSP_ERROR_NONE)
  {
    Error_Handler();
  }

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */

  /* ── Startup banner over USART2 ──────────────────────────────────────── */
    printf("\r\n");
    printf("========================================\r\n");
  #if OUTDOOR_MODE
    printf("  Weather Station v%s  [OUTDOOR]\r\n", FW_VERSION);
  #else
    printf("  Weather Station v%s  [INDOOR]\r\n", FW_VERSION);
  #endif
    printf("  STM32G431KB @ 170 MHz\r\n");
    printf("========================================\r\n\r\n");

    /* ── I2C bus scan ────────────────────────────────────────────────────── */
    I2C_BusScan(&hi2c2);

    /* ── BME680 init ─────────────────────────────────────────────────────── */
    printf("BME680: ");
    BME680_Status bme_st = BME680_Init(&bme680, &hi2c2, BME680_I2C_ADDR);
    if (bme_st == BME680_OK)
        printf("OK (chip ID 0x61)\r\n");
    else
        printf("FAIL (err %d)\r\n", bme_st);

    /* ── LTR390 init ─────────────────────────────────────────────────────── */
    printf("LTR390: ");
    LTR390_Status ltr_st = LTR390_Init(&ltr390, &hi2c2, LTR390_I2C_ADDR);
    if (ltr_st == LTR390_OK)
        printf("OK (part ID 0xB2)\r\n");
    else
        printf("FAIL (err %d)\r\n", ltr_st);

    /* ── ILI9341 2.8\" TFT display init ────────────────────────────────────── */
    printf("ILI9341: ");
    ILI9341_Init(&tft, &hspi1, &htim2, TIM_CHANNEL_1);
    ILI9341_SetRotation(&tft, ILI9341_ROTATION_LANDSCAPE);
    printf("OK (%u x %u)\r\n", tft.width, tft.height);

    /* Draw static dashboard frame on screen */
    TFT_DrawDashboardLayout();

    /* ── MicroSD card init (SPI1) ────────────────────────────────────────── */
    #if ENABLE_SD_LOGGING
        printf("MicroSD: ");
        SD_Status sd_st = SD_Init(&sd_card, &hspi1);
        if (sd_st == SD_OK) {
            uint32_t mb = (sd_card.sector_count / 2048);
            printf("OK (Type 0x%02X, Capacity: %lu MB)\r\n", sd_card.card_type, (unsigned long)mb);
            SD_Logger_Init();
            printf("\r\n");
        } else {
            printf("FAIL (err %d - check card insertion)\r\n\r\n", sd_st);
        }
    #else
        printf("MicroSD: Disabled (v%s)\r\n\r\n", FW_VERSION);
    #endif
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    /* ── Periodic sensor read & display refresh (every SENSOR_POLL_MS) ── */
    if ((HAL_GetTick() - lastSensorRead) >= SENSOR_POLL_MS)
    {
        lastSensorRead = HAL_GetTick();

        /* BME680 — forced mode T/P/H + Gas */
#if OUTDOOR_MODE
        BME680_Status bs = BME680_ReadForced(&bme680, false);
#else
        BME680_Status bs = BME680_ReadForced(&bme680, true);
#endif

        /* LTR390 — ALS (lux) + UVS (UV index) */
        LTR390_Status ls = LTR390_Read(&ltr390);

        if (bs == BME680_OK && ls == LTR390_OK)
        {
            const BME680_Data *bd = BME680_GetData(&bme680);
            const LTR390_Data *ld = LTR390_GetData(&ltr390);

            /* 1. Update ILI9341 display */
            TFT_UpdateDashboardValues(bd, ld);

            /* 2. Ambient auto-dimming: adjust backlight PWM from lux */
            uint16_t pwm_duty = (uint16_t)(ld->lux * 1.5f);
            if (pwm_duty < 150)  pwm_duty = 150;  /* Minimum readable level */
            if (pwm_duty > 999)  pwm_duty = 999;
            ILI9341_SetBacklight(&tft, pwm_duty);

            /* 3. Night-mode button LED logic: PA11 HIGH when dark (< 20 lux) */
            if (ld->lux < BTN_LED_LUX_THRESH) {
                HAL_GPIO_WritePin(BTN_LED_EN_GPIO_Port, BTN_LED_EN_Pin, GPIO_PIN_SET);
            } else {
                HAL_GPIO_WritePin(BTN_LED_EN_GPIO_Port, BTN_LED_EN_Pin, GPIO_PIN_RESET);
            }

            /* 4. Console log */
            printf("T: %.1f C  |  P: %.1f hPa  |  H: %.1f %%  |  Dew: %.1f C",
                   bd->temperature, bd->pressure, bd->humidity, bd->dew_point);
            if (bd->gas_valid)
                printf("  |  IAQ: %.0f (%s)  |  Gas: %.1f kOhm",
                       bd->iaq_score, bd->iaq_text, bd->gas_resistance / 1000.0f);
            printf("\r\nLux: %.1f  |  UV: %.2f  |  PWM: %u\r\n\r\n",
                   ld->lux, ld->uv_index, pwm_duty);

			#if ENABLE_SD_LOGGING
            /* 5. MicroSD CSV Logging (every SD_LOG_INTERVAL_MS) */
            static uint32_t lastSdLog = 0;
            if ((HAL_GetTick() - lastSdLog) >= SD_LOG_INTERVAL_MS) {
                lastSdLog = HAL_GetTick();
                if (SD_Logger_IsActive()) {
                    SD_Logger_LogWeather(HAL_GetTick() / 1000, bd, ld);
                }
            }
			#endif
        }
        else
        {
            printf("Sensor read error: BME680=%d, LTR390=%d\r\n", bs, ls);
        }

        /* Heartbeat LED toggle */
		BSP_LED_Toggle(LED_GREEN);
    }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
  RCC_OscInitStruct.PLL.PLLN = 85;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x40B285C2;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  hrtc.Init.OutPutPullUp = RTC_OUTPUT_PULLUP_NONE;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the WakeUp
  */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 59, RTC_WAKEUPCLOCK_CK_SPRE_16BITS) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 7;
  hspi1.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi1.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 169;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(ESP_EN_GPIO_Port, ESP_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(SD_CS_GPIO_Port, SD_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BTN_LED_EN_GPIO_Port, BTN_LED_EN_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, TFT_RST_Pin|TFT_DC_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : BTN_OK_Pin */
  GPIO_InitStruct.Pin = BTN_OK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN_OK_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : ESP_EN_Pin */
  GPIO_InitStruct.Pin = ESP_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(ESP_EN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : BTN_UP_Pin BTN_BACK_Pin */
  GPIO_InitStruct.Pin = BTN_UP_Pin|BTN_BACK_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : SD_CS_Pin BTN_LED_EN_Pin */
  GPIO_InitStruct.Pin = SD_CS_Pin|BTN_LED_EN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : TFT_CS_Pin TFT_RST_Pin TFT_DC_Pin */
  GPIO_InitStruct.Pin = TFT_CS_Pin|TFT_RST_Pin|TFT_DC_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : BTN_DOWN_Pin */
  GPIO_InitStruct.Pin = BTN_DOWN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(BTN_DOWN_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);

  HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  HAL_NVIC_SetPriority(EXTI4_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI4_IRQn);

  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/**
  * @}
  */

/**
  * @}
  */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
