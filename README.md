# STM32G431KB Portable Weather Station (`v1.0.0`)

Embedded environmental monitoring station built on the **STMicroelectronics NUCLEO-G431KB** running at **170 MHz**. Reads temperature, humidity, barometric pressure, VOC gas resistance / IAQ, ambient light (Lux), and UV index over `I2C2`, renders a real-time 320×240 dashboard on an **ILI9341 TFT** over `SPI1` with automatic PWM backlight dimming, and streams formatted telemetry over USB UART at **115200 baud**.

---

## Hardware Bill of Materials

| Component | Interface | Address / Notes |
| :--- | :--- | :--- |
| **NUCLEO-G431KB** (`STM32G431KBT6`) | MCU (`170 MHz`) | Powered via ST-LINK Micro-USB (`3.3V` logic) |
| **Bosch BME680** Breakout | `I2C2` (`400 kHz`) | `0x77` — Temp, Humidity, Pressure, Gas/IAQ |
| **Lite-On LTR-390UV-01** Breakout | `I2C2` (`400 kHz`) | `0x53` — Ambient Light (Lux) & UV Index |
| **2.8" ILI9341 SPI TFT** (`320 × 240`) | `SPI1` (`10.625 MHz`) | Includes `TIM2_CH1` PWM backlight dimming |
| **MicroSD Card Module** *(v1.1)* | `SPI1` (shared) | Disabled in `v1.0.0` (`ENABLE_SD_LOGGING 0`) |
| **4× Tactile Push Buttons + LED** *(v1.1)* | `EXTI` + `GPIO` | Internal pull-ups enabled; active-low to `GND` |

---

## Pinout & Wiring (`NUCLEO-G431KB` Top View — USB at Top)

### Power Rails
* **`CN3 Pin 14` (`+3V3`)** → Breadboard `+` (`3.3V`) rail *(powers TFT `VCC` & `CS2`, BME680, LTR390)*
* **`CN4 Pin 4` / `CN3 Pin 2` (`GND`)** → Breadboard `-` (`GND`) rail

### `I2C2` Environmental Sensors (Qwiic)
> **Hardware Note:** On the `NUCLEO-G431KB`, factory solder bridges `SB2` and `SB3` tie `PA5`/`PA6` (`SPI1`) to `PB7`/`PA15` (`I2C1`). To run `SPI1` and I2C concurrently without desoldering `SB2`/`SB3`, sensors are routed to **`I2C2`**.

| Sensor Signal | Qwiic Wire Color | STM32 Pin | Nucleo Header Pin |
| :--- | :--- | :--- | :--- |
| `GND` | Black | `GND` | Breadboard `-` rail |
| `3V3` | Red | `+3V3` | Breadboard `+` rail (`CN3 Pin 14`) |
| `SDA` | Blue | `PA8` (`I2C2_SDA`) | **`CN4 Pin 12`** (`D9`) |
| `SCL` | Yellow | `PA9` (`I2C2_SCL`) | **`CN4 Pin 1`** (`D1`) |

### `SPI1` 2.8" ILI9341 TFT Display

| Display Pin | STM32 Pin | Nucleo Header Pin | Function |
| :--- | :--- | :--- | :--- |
| `GND` | `GND` | Breadboard `-` rail | Ground |
| `VCC` | `+3V3` | Breadboard `+` rail | Display power (`3.3V`) |
| `CS2` | `+3V3` | Breadboard `+` rail | XPT2046 touch CS tied inactive (`HIGH`) |
| `CLK` (`SCK`) | `PA5` | **`CN3 Pin 8`** (`A4`) | `SPI1_SCK` (`10.625 Mbit/s`) |
| `MISO` (`SDO`) | `PA6` | **`CN3 Pin 7`** (`A5`) | `SPI1_MISO` |
| `MOSI` (`SDI`) | `PA7` | **`CN3 Pin 6`** (`A6`) | `SPI1_MOSI` |
| `CS1` (`CS`) | `PB0` | **`CN4 Pin 6`** (`D3`) | `TFT_CS` (active-low) |
| `DC` (`RS`) | `PB6` | **`CN4 Pin 9`** (`D6`) | `TFT_DC` (Data / Command) |
| `RES` (`RST`) | `PB5` | **`CN4 Pin 14`** (`D11`) | `TFT_RST` (Hardware Reset) |
| `BLK` (`LED`) | `PA0` | **`CN3 Pin 12`** (`A0`) | `TIM2_CH1` Backlight PWM (`1 kHz`) |

---

## Building & Flashing

1. Open the project in **STM32CubeIDE** (`File → Open Projects from File System...`).
2. Adjust build flags in `Core/Inc/app_config.h` if needed:
   * `OUTDOOR_MODE` (`0` = Indoor with BME680 VOC heater enabled; `1` = Outdoor without gas heater)
   * `ENABLE_SD_LOGGING` (`0` in `v1.0.0`; set to `1` once MicroSD hardware is attached)
3. Build with **`Ctrl + B`** and flash via ST-LINK with **`Ctrl + F11`**.
4. Open a serial terminal (PuTTY / Tera Term) on the **STMicroelectronics STLink Virtual COM Port** at **`115200` baud** (`8-N-1`, Flow Control: `None`).

### Example Serial Boot & Telemetry Output
```text
========================================
  Weather Station v1.0.0  [INDOOR]
  STM32G431KB @ 170 MHz
========================================

I2C2 bus scan (0x01 – 0x7F):
  [0x53] ACK ← LTR390
  [0x77] ACK ← BME680
  2 device(s) found.

BME680: OK (chip ID 0x61)
LTR390: OK (part ID 0xB2)
ILI9341: OK (320 x 240)
MicroSD: Disabled (v1.0.0)

T: 22.4 C  |  P: 1012.8 hPa  |  H: 43.5 %  |  Dew: 9.4 C  |  IAQ: 28 (Good)  |  Gas: 148.2 kOhm
Lux: 312.0  |  UV: 0.00  |  PWM: 468
