# STM32G431KB Portable Weather Station (`v1.0.0`)

[![Release](https://img.shields.io/github/v/release/steam781/Weather-Station?label=firmware)](https://github.com/steam781/Weather-Station/releases/tag/v.1.0.0)
[![Interactive Simulator & Wiring Guide](https://img.shields.io/badge/Live_Demo-Simulator_%26_Wiring_Canvas-06b6d4)](https://steam781.github.io/Weather-Station/)

Embedded environmental monitoring station built on the **STMicroelectronics NUCLEO-G431KB** (`STM32G431KBT6` running at **170 MHz**). Reads temperature, humidity, barometric pressure, dew point, VOC gas resistance / IAQ, ambient light (Lux), and UV index over `I2C2`, renders a real-time 320×240 dashboard on an **ILI9341 TFT** over `SPI1` with automatic PWM backlight dimming, and streams formatted telemetry over USB UART at **115200 baud**.

> **Interactive Browser Simulator & Pinout Canvas:** Open the [**Live Simulator & Wiring Router**](https://steam781.github.io/Weather-Station/) to test sensor scenarios, emulate the ILI9341 display, and trace every jumper wire on the `NUCLEO-G431KB`.

---

## Hardware Bill of Materials

| Component | Interface | Address / Notes |
| :--- | :--- | :--- |
| **NUCLEO-G431KB** (`STM32G431KBT6`) | MCU (`170 MHz`) | Powered via ST-LINK Micro-USB (`3.3V` logic) |
| **Bosch BME680** Breakout | `I2C2` (`400 kHz`) | `0x77` — Temp, Humidity, Pressure, Dew Point, Gas/IAQ |
| **Lite-On LTR-390UV-01** Breakout | `I2C2` (`400 kHz`) | `0x53` — Ambient Light (Lux) & UV Index |
| **2.8" ILI9341 SPI TFT** (`320 × 240`) | `SPI1` (`10.625 MHz`) | Includes `TIM2_CH1` PWM backlight dimming |
| **MicroSD Card Reader** *(v1.1)* | `SPI1` (shared) | Disabled in `v.1.0.0` (`ENABLE_SD_LOGGING 0`) |
| **4× Tactile Push Buttons + LED** *(v1.1)* | `EXTI` + `GPIO` | Internal pull-ups enabled; active-low to `GND` |

---

## Pinout & Wiring (`NUCLEO-G431KB` Top View — USB at Top)

Seat the **NUCLEO-G431KB** into the breadboard with the **Micro-USB port at the TOP** and the **STM32 chip facing UP** (`CN4` on the left, `CN3` on the right).

### 1. Power Rails
* **`CN3 Pin 14` (`+3V3`, 2nd from bottom-right)** → Breadboard `+` (`3.3V`) rail
* **`CN4 Pin 4` (`GND`, 4th from top-left)** → Breadboard `-` (`GND`) rail

### 2. `I2C2` Environmental Sensors (Qwiic)
> **Why `I2C2`?** On the `NUCLEO-G431KB`, factory solder bridges `SB2` and `SB3` tie `PA5`/`PA6` (`SPI1`) to `PB7`/`PA15` (`I2C1`). Routing sensors to **`I2C2`** (`PA9` / `PA8`) prevents SPI/I2C bus collisions without modifying PCB solder bridges.

| Sensor Signal | Qwiic Color | STM32 Pin | Nucleo Header Pin |
| :--- | :--- | :--- | :--- |
| `GND` | Black | `GND` | Breadboard `-` rail |
| `3V3` | Red | `+3V3` | Breadboard `+` rail (`CN3 Pin 14`) |
| `SDA` | Blue | `PA8` (`I2C2_SDA`) | **`CN4 Pin 12`** (`D9`, 4th from bottom-left) |
| `SCL` | Yellow | `PA9` (`I2C2_SCL`) | **`CN4 Pin 1`** (`D1`, top-left pin) |

### 3. `SPI1` 2.8" ILI9341 TFT Display

| Display Pin | STM32 Pin | Nucleo Header Pin | Function |
| :--- | :--- | :--- | :--- |
| `GND` | `GND` | Breadboard `-` rail | Ground |
| `VCC` | `+3V3` | Breadboard `+` rail | Display supply (`3.3V`) |
| `CS2` | `+3V3` | Breadboard `+` rail | XPT2046 touch CS tied inactive (`HIGH`) |
| `CLK` (`SCK`) | `PA5` | **`CN3 Pin 8`** (`A4`) | `SPI1_SCK` (`10.625 Mbit/s`) |
| `MISO` (`SDO`) | `PA6` | **`CN3 Pin 7`** (`A5`) | `SPI1_MISO` |
| `MOSI` (`SDI`) | `PA7` | **`CN3 Pin 6`** (`A6`) | `SPI1_MOSI` |
| `CS1` (`CS`) | `PB0` | **`CN4 Pin 6`** (`D3`) | `TFT_CS` (active-low) |
| `DC` (`RS`) | `PB6` | **`CN4 Pin 9`** (`D6`) | `TFT_DC` (Data / Command) |
| `RES` (`RST`) | `PB5` | **`CN4 Pin 14`** (`D11`) | `TFT_RST` (Hardware Reset) |
| `BLK` (`LED`) | `PA0` | **`CN3 Pin 12`** (`A0`) | `TIM2_CH1` Backlight PWM (`1 kHz`) |

---

## Flashing or Building Firmware

### Option A: Flash Prebuilt Binaries (`v.1.0.0`)
1. Download `Weather-Station.bin`, `Weather-Station.hex`, or `Weather-Station.elf` from [**Releases (`v.1.0.0`)**](https://github.com/steam781/Weather-Station/releases/tag/v.1.0.0).
2. Connect the **NUCLEO-G431KB** via Micro-USB and flash using **STM32CubeProgrammer** (flash `.bin` to `0x08000000`, or load `.hex`/`.elf` directly).
3. Open a serial terminal at **`115200` baud** (`8-N-1`, Flow Control: `None`).

### Option B: Build From Source (STM32CubeIDE)
1. Open the repository folder in **STM32CubeIDE** (`File → Open Projects from File System...`).
2. Configure build flags in `Core/Inc/app_config.h`:
   * `OUTDOOR_MODE` (`0` = Indoor with BME680 VOC heater enabled; `1` = Outdoor with gas heater bypassed)
   * `ENABLE_SD_LOGGING` (`0` in `v.1.0.0`; set to `1` once MicroSD hardware is connected)
3. Build (`Ctrl + B`) and run (`Ctrl + F11`).

---

## Web Simulator & Interactive Wiring Canvas (`simulator/`)

The `simulator/` folder contains a React + TypeScript + Vite web application replicating the firmware's BME680/LTR390 physics, ILI9341 5×7 font renderer, `USART2` serial console, and an interactive `NUCLEO-G431KB` pin-routing diagram.

```bash
cd simulator
npm install
npm run dev
