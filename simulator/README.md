# Weather Station (STM32G431KB) — Web Simulation & Telemetry Console

Interactive React + TypeScript web application replicating the core sensing, display, and logging logic of the **STM32G431KB Weather Station** firmware (`steam781/Weather-Station`) in AI Studio.

## Ported Firmware Logic

* **BME680 Environmental & IAQ Engine (`src/firmware/sensorEngine.ts`)**
  * Computes Magnus dew point, hypsometric sea-level pressure (QNH), slow upward clean-air gas baseline tracking, and 0–500 EPA-scale IAQ scoring (`Good` through `Hazardous`).
  * Implements the `OUTDOOR_MODE` switch (`0 = Indoor` with 320 °C gas heater / IAQ active; `1 = Outdoor` with gas heater bypassed).
* **Lite-On LTR-390UV-01 Optical & UV Engine (`src/firmware/sensorEngine.ts`)**
  * Models 20-bit ALS/UVS ADC counts across configurable gains (`1x–18x`) and integration resolutions (`16–20 bit`), saturation detection, lux and UV index computation, `TIM2_CH1` PWM backlight auto-dimming (`150–999`), and `PA11` night-mode button ring illumination (`< 20.0 lux`).
* **ILI9341 2.8" TFT Display Emulator (`src/components/Ili9341Display.tsx`, `src/firmware/font5x7.ts`)**
  * Pixel-accurate HTML5 Canvas renderer using the original 5×7 ASCII bitmap font table and RGB565 palette, supporting Landscape (`320×240`) and Portrait (`240×320`) orientations, sleep/wake mode, PWM brightness scaling, and tactile Nucleo buttons (`UP`, `DOWN`, `OK`, `BACK`).
* **MicroSD FatFS CSV Logger & USART2 Serial Console (`src/components/SdLoggerPanel.tsx`, `src/components/UsartConsole.tsx`)**
  * Auto-appends rows every 10 seconds to an in-memory `WEATHER.CSV` buffer with real CSV download export, alongside a live `USART2` (`115200 baud`) console stream.

## Running Locally

```bash
npm install
npm run dev
```

The Vite development server listens on `http://0.0.0.0:3000`.
