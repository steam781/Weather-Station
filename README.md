# Weather Station Firmware (STM32G431KB)

Embedded C firmware for a modular environmental monitoring station built on the **STMicroelectronics NUCLEO-G431KB** (ARM Cortex-M4 @ 170 MHz). The current codebase implements the complete bare-metal HAL driver layer for environmental/optical sensing, local SPI display rendering, and FatFS MicroSD CSV logging, structured with a build-time `OUTDOOR_MODE` switch for transitioning from an indoor desk station (v1) to a weather-sealed outdoor unit (v2).

## Implemented Drivers

* **Bosch BME680 (`Core/Inc/bme680.h`, `Core/Src/bme680.c`)**
  * I2C forced-mode driver reading temperature, barometric pressure, relative humidity, and calculated dew point.
  * Supports integrated gas heater control and Indoor Air Quality (IAQ) / VOC resistance scoring when `OUTDOOR_MODE` is set to `0`, with clean heater bypass when `OUTDOOR_MODE` is set to `1`.
* **Lite-On LTR-390UV-01 (`Core/Inc/ltr390.h`, `Core/Src/ltr390.c`)**
  * Dual-mode I2C driver switching between Ambient Light Sensing (ALS in lux) and Ultraviolet Sensing (UVS / UV index).
  * Drives automatic PWM display backlight scaling and night-mode button illumination thresholds.
* **ILI9341 2.8" TFT Display (`Core/Inc/ili9341.h`, `Core/Src/ili9341.c`)**
  * SPI1 driver supporting hardware/software reset, portrait and landscape orientation (`MADCTL`), RGB565 drawing primitives, and a built-in 5×7 ASCII font renderer.
  * Integrates `TIM2_CH1` PWM brightness control and enforces strict Chip Select (`TFT_CS`) release after every transaction for shared-bus safety.
* **MicroSD SPI & FatFS CSV Logger (`Core/Inc/sd_spi.h`, `Core/Src/sd_spi.c`, `Core/Inc/sd_logger.h`, `Core/Src/sd_logger.c`)**
  * Low-level SPI block driver supporting SDv1, SDv2 (SDSC/SDHC/SDXC), and MMC cards with automatic clock-speed stepping (slow init clock scaling up to ~10.6 Mbit/s) and dummy-clock MISO release.
  * Bridged to FatFS via `FATFS/Target/user_diskio.c` to automatically create and append timestamped telemetry rows to `WEATHER.CSV`.

## Pinout & Peripheral Mapping

| Peripheral | Pins | Connected Hardware | Notes |
| :--- | :--- | :--- | :--- |
| **I2C1** | `PA15` (SCL), `PB7` (SDA) | BME680 (`0x77`), LTR390 (`0x53`) | 400 kHz Fast Mode |
| **SPI1** | `PA5` (SCK), `PA6` (MISO), `PA7` (MOSI) | ILI9341 TFT + MicroSD | Shared bus (~10.6 Mbit/s), software CS |
| **Display Control** | `PB0` (`TFT_CS`), `PB6` (`TFT_DC`), `PB5` (`TFT_RST`) | ILI9341 TFT | `TFT_CS` idles HIGH |
| **TIM2_CH1** | `PA0` (`TFT_BLK`) | ILI9341 Backlight | 1 kHz PWM dimming (`0–999` duty) |
| **Storage Control** | `PA4` (`SD_CS`) | Adafruit MicroSD Breakout | `SD_CS` idles HIGH |
| **USART2 (BSP)** | `PA2` (TX), `PA3` (RX) | ST-LINK Virtual COM | `printf` debug output (115200 baud) |
| **GPIO Output** | `PA11` (`BTN_LED_EN`), `PB8` (`LD2`) | ULN2003 LED Driver / Onboard LED | Night-mode button ring & heartbeat |

## Configuration & Repository Layout

* `Core/Inc/app_config.h` — Central build configuration (`OUTDOOR_MODE`, 7-bit I2C addresses, polling/logging intervals, oversampling, and lux thresholds).
* `Core/Inc/` & `Core/Src/` — Hardware drivers (`bme680`, `ltr390`, `ili9341`, `sd_spi`, `sd_logger`) and main application loop.
* `FATFS/Target/user_diskio.c` — FatFS disk I/O interface linking `f_mount` / `f_write` calls to `sd_spi`.
