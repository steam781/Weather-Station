import React, { useEffect, useRef, useState } from 'react';
import { FONT_5X7 } from '../firmware/font5x7';
import {
  APP_CONFIG,
  FW_VERSION,
  StationMode,
  TelemetrySample,
} from '../firmware/sensorEngine';

interface Ili9341DisplayProps {
  sample: TelemetrySample;
  mode: StationMode;
  enableSdLogging: boolean;
  heartbeatOn: boolean;
  csvRowCount: number;
  gasBaselineOhm: number;
  onManualPoll: () => void;
  onManualSdLog: () => void;
}

type DisplayPage = 0 | 1 | 2;

// Exact RGB565 colors converted to CSS hex from Core/Inc/ili9341.h
const C = {
  BLACK: '#000000',
  NAVY: '#00007B',
  DARKCYAN: '#007D7B',
  DARKGREY: '#7B7D7B',
  CYAN: '#00FFFF',
  YELLOW: '#FFFF00',
  GREEN: '#00FF00',
  ORANGE: '#FFA500',
  RED: '#FF0000',
  WHITE: '#FFFFFF',
};

function drawChar5x7(
  ctx: CanvasRenderingContext2D,
  x: number,
  y: number,
  ch: string,
  color: string,
  bgColor: string,
  size: number
) {
  const code = ch.charCodeAt(0);
  const idx = code >= 32 && code <= 126 ? code - 32 : 0;
  const glyph = FONT_5X7[idx];

  ctx.fillStyle = bgColor;
  ctx.fillRect(x, y, 6 * size, 8 * size);

  ctx.fillStyle = color;
  for (let col = 0; col < 5; col++) {
    const line = glyph[col];
    for (let row = 0; row < 7; row++) {
      if (line & (1 << row)) {
        ctx.fillRect(x + col * size, y + row * size, size, size);
      }
    }
  }
}

function drawString5x7(
  ctx: CanvasRenderingContext2D,
  x: number,
  y: number,
  text: string,
  color: string,
  bgColor: string,
  size: number,
  maxWidth: number
) {
  let cx = x;
  let cy = y;
  for (let i = 0; i < text.length; i++) {
    if (cx + 6 * size > maxWidth) {
      cx = 0;
      cy += 8 * size;
    }
    drawChar5x7(ctx, cx, cy, text[i], color, bgColor, size);
    cx += 6 * size;
  }
}

export const Ili9341Display: React.FC<Ili9341DisplayProps> = ({
  sample,
  mode,
  enableSdLogging,
  heartbeatOn,
  csvRowCount,
  gasBaselineOhm,
  onManualPoll,
  onManualSdLog,
}) => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);
  const [page, setPage] = useState<DisplayPage>(0);
  const [sleeping, setSleeping] = useState(false);
  const [portrait, setPortrait] = useState(false);

  const width = portrait ? 240 : 320;
  const height = portrait ? 320 : 240;

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    ctx.imageSmoothingEnabled = false;

    if (sleeping) {
      ctx.fillStyle = C.BLACK;
      ctx.fillRect(0, 0, width, height);
      return;
    }

    const { bme, ltr, pwmDuty, btnLedEn } = sample;

    // Clear screen to black
    ctx.fillStyle = C.BLACK;
    ctx.fillRect(0, 0, width, height);

    // Header bar (TFT_DrawDashboardLayout)
    ctx.fillStyle = C.NAVY;
    ctx.fillRect(0, 0, width, 30);

    if (page === 0) {
      drawString5x7(
        ctx,
        10,
        8,
        portrait ? 'WEATHER v1.0' : 'WEATHER STATION v1.0',
        C.WHITE,
        C.NAVY,
        2,
        width
      );
    } else if (page === 1) {
      drawString5x7(ctx, 10, 8, 'BUS & PIN DIAGNOSTICS', C.WHITE, C.NAVY, 2, width);
    } else {
      drawString5x7(ctx, 10, 8, 'SD LOGGER & QNH', C.WHITE, C.NAVY, 2, width);
    }

    // Horizontal cyan divider at y=31
    ctx.fillStyle = C.DARKCYAN;
    ctx.fillRect(0, 31, width, 1);

    if (page === 0) {
      if (!portrait) {
        // Exact 320x240 layout from Core/Src/main.c lines 115-180
        ctx.fillStyle = C.DARKGREY;
        ctx.fillRect(0, 130, width, 1);
        ctx.fillRect(160, 32, 1, 198);

        ctx.fillStyle = C.DARKCYAN;
        ctx.fillRect(0, 230, width, 1);

        // Left column labels
        drawString5x7(ctx, 10, 38, 'TEMPERATURE', C.CYAN, C.BLACK, 1, width);
        drawString5x7(ctx, 10, 84, 'HUMIDITY', C.CYAN, C.BLACK, 1, width);
        drawString5x7(ctx, 10, 138, 'PRESSURE', C.CYAN, C.BLACK, 1, width);
        drawString5x7(ctx, 10, 184, 'DEW POINT', C.CYAN, C.BLACK, 1, width);

        // Right column labels
        drawString5x7(ctx, 170, 38, 'AMBIENT LIGHT', C.YELLOW, C.BLACK, 1, width);
        drawString5x7(ctx, 170, 84, 'UV INDEX', C.YELLOW, C.BLACK, 1, width);
        drawString5x7(ctx, 170, 138, 'AIR QUALITY (IAQ)', C.GREEN, C.BLACK, 1, width);
        drawString5x7(ctx, 170, 184, 'GAS RESISTANCE', C.GREEN, C.BLACK, 1, width);

        // Live values
        drawString5x7(
          ctx,
          10,
          52,
          `${bme.temperature.toFixed(1).padStart(5, ' ')} C`,
          C.WHITE,
          C.BLACK,
          2,
          width
        );
        drawString5x7(
          ctx,
          10,
          98,
          `${bme.humidity.toFixed(1).padStart(5, ' ')} %`,
          C.WHITE,
          C.BLACK,
          2,
          width
        );
        drawString5x7(
          ctx,
          10,
          152,
          `${bme.pressure.toFixed(1).padStart(6, ' ')} hPa`,
          C.WHITE,
          C.BLACK,
          2,
          width
        );
        drawString5x7(
          ctx,
          10,
          198,
          `${bme.dewPoint.toFixed(1).padStart(5, ' ')} C`,
          C.WHITE,
          C.BLACK,
          2,
          width
        );

        drawString5x7(
          ctx,
          170,
          52,
          `${ltr.lux.toFixed(0).padStart(5, ' ')} Lux`,
          C.WHITE,
          C.BLACK,
          2,
          width
        );
        drawString5x7(
          ctx,
          170,
          98,
          `${ltr.uvIndex.toFixed(1).padStart(4, ' ')} UV `,
          C.WHITE,
          C.BLACK,
          2,
          width
        );

        if (bme.gasValid) {
          const iaqColor =
            bme.iaqScore <= 50 ? C.GREEN : bme.iaqScore <= 100 ? C.YELLOW : C.ORANGE;
          const label = bme.iaqText.slice(0, 6).padEnd(6, ' ');
          drawString5x7(
            ctx,
            170,
            152,
            `${bme.iaqScore.toFixed(0).padStart(3, ' ')} ${label}`,
            iaqColor,
            C.BLACK,
            2,
            width
          );
          drawString5x7(
            ctx,
            170,
            198,
            `${(bme.gasResistanceOhm / 1000).toFixed(1).padStart(5, ' ')} kOhm`,
            C.WHITE,
            C.BLACK,
            2,
            width
          );
        } else {
          drawString5x7(
            ctx,
            170,
            152,
            mode === 'outdoor' ? 'BYPASSED  ' : 'Warmup... ',
            C.DARKGREY,
            C.BLACK,
            2,
            width
          );
          drawString5x7(ctx, 170, 198, '--------- ', C.DARKGREY, C.BLACK, 2, width);
        }
      } else {
        // Portrait 240x320 stacked layout
        ctx.fillStyle = C.DARKCYAN;
        ctx.fillRect(0, 310, width, 1);

        const rows: [string, string, string][] = [
          ['TEMPERATURE', `${bme.temperature.toFixed(1)} C`, C.CYAN],
          ['HUMIDITY', `${bme.humidity.toFixed(1)} %`, C.CYAN],
          ['PRESSURE', `${bme.pressure.toFixed(1)} hPa`, C.CYAN],
          ['DEW POINT', `${bme.dewPoint.toFixed(1)} C`, C.CYAN],
          ['AMBIENT LIGHT', `${ltr.lux.toFixed(0)} Lux`, C.YELLOW],
          ['UV INDEX', `${ltr.uvIndex.toFixed(1)} UV`, C.YELLOW],
          [
            'AIR QUALITY',
            bme.gasValid ? `${bme.iaqScore.toFixed(0)} ${bme.iaqText}` : 'N/A',
            bme.gasValid ? C.GREEN : C.DARKGREY,
          ],
        ];
        rows.forEach(([label, val, col], idx) => {
          const y = 38 + idx * 38;
          drawString5x7(ctx, 10, y, label, col, C.BLACK, 1, width);
          drawString5x7(ctx, 10, y + 12, val, C.WHITE, C.BLACK, 2, width);
          ctx.fillStyle = C.DARKGREY;
          ctx.fillRect(0, y + 33, width, 1);
        });
      }
    } else if (page === 1) {
      // Page 2: I2C2 & SPI1 hardware diagnostics
      drawString5x7(ctx, 10, 42, 'I2C2 BUS (PA15/PB7 @ 400kHz)', C.CYAN, C.BLACK, 1, width);
      drawString5x7(ctx, 10, 56, '0x77 BME680  : ACK (ID 0x61)', C.GREEN, C.BLACK, 1, width);
      drawString5x7(ctx, 10, 70, '0x53 LTR390  : ACK (ID 0xB2)', C.GREEN, C.BLACK, 1, width);

      ctx.fillStyle = C.DARKGREY;
      ctx.fillRect(0, 88, width, 1);

      drawString5x7(ctx, 10, 96, 'SPI1 SHARED BUS (PA5/6/7)', C.YELLOW, C.BLACK, 1, width);
      drawString5x7(ctx, 10, 110, 'PB0 TFT_CS   : IDLE HIGH', C.WHITE, C.BLACK, 1, width);
      drawString5x7(ctx, 10, 124, 'PA4 SD_CS    : IDLE HIGH', C.WHITE, C.BLACK, 1, width);

      ctx.fillStyle = C.DARKGREY;
      ctx.fillRect(0, 142, width, 1);

      drawString5x7(ctx, 10, 150, 'PWM & GPIO TELEMETRY', C.CYAN, C.BLACK, 1, width);
      drawString5x7(
        ctx,
        10,
        166,
        `TIM2_CH1 PWM : ${pwmDuty}/999`,
        C.WHITE,
        C.BLACK,
        1,
        width
      );
      drawString5x7(
        ctx,
        10,
        180,
        `PA11 BTN_LED : ${btnLedEn ? 'HIGH (NIGHT)' : 'LOW (DAY)'}`,
        btnLedEn ? C.YELLOW : C.WHITE,
        C.BLACK,
        1,
        width
      );
      drawString5x7(
        ctx,
        10,
        194,
        `OUTDOOR_MODE : ${mode === 'outdoor' ? '1 (LOW PWR)' : '0 (INDOOR)'}`,
        C.WHITE,
        C.BLACK,
        1,
        width
      );
      drawString5x7(
        ctx,
        10,
        208,
        `FW VERSION   : v${FW_VERSION}`,
        C.DARKGREY,
        C.BLACK,
        1,
        width
      );
    } else {
      // Page 3: SD Logger & Sea-level QNH
      drawString5x7(ctx, 10, 42, 'MICROSD FATFS LOGGER', C.GREEN, C.BLACK, 1, width);
      drawString5x7(
        ctx,
        10,
        58,
        enableSdLogging ? `FILE: ${APP_CONFIG.SD_LOG_FILENAME}` : 'SD: DISABLED',
        enableSdLogging ? C.WHITE : C.DARKGREY,
        C.BLACK,
        2,
        width
      );
      drawString5x7(
        ctx,
        10,
        82,
        enableSdLogging ? `ROWS LOGGED: ${csvRowCount}` : `ENABLE_SD_LOGGING = 0 (v${FW_VERSION})`,
        enableSdLogging ? C.CYAN : C.YELLOW,
        C.BLACK,
        1,
        width
      );
      drawString5x7(
        ctx,
        10,
        96,
        `UPTIME TICK: ${sample.timestampSec} s`,
        C.WHITE,
        C.BLACK,
        1,
        width
      );

      ctx.fillStyle = C.DARKGREY;
      ctx.fillRect(0, 118, width, 1);

      drawString5x7(ctx, 10, 128, 'HYPSOMETRIC QNH & BASELINE', C.YELLOW, C.BLACK, 1, width);
      drawString5x7(
        ctx,
        10,
        144,
        `QNH: ${bme.seaLevelPressure.toFixed(1)} hPa`,
        C.WHITE,
        C.BLACK,
        2,
        width
      );
      drawString5x7(
        ctx,
        10,
        172,
        `GAS BASELINE: ${(gasBaselineOhm / 1000).toFixed(1)} kOhm`,
        C.WHITE,
        C.BLACK,
        1,
        width
      );
      drawString5x7(
        ctx,
        10,
        188,
        `ALS RAW: ${ltr.alsRaw}  UVS RAW: ${ltr.uvsRaw}`,
        C.DARKGREY,
        C.BLACK,
        1,
        width
      );
    }
  }, [sample, mode, enableSdLogging, page, sleeping, portrait, width, height, csvRowCount, gasBaselineOhm]);

  // Simulate TIM2_CH1 PWM backlight dimming visually (minimum readable 0.35 so UI remains visible)
  const backlightOpacity = sleeping ? 0.08 : 0.38 + (sample.pwmDuty / 999) * 0.62;

  return (
    <div className="bg-[#111827] border border-slate-800 rounded-md p-4 flex flex-col gap-4">
      {/* Header row */}
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h2 className="text-sm font-semibold text-slate-100">
            ILI9341 2.8&quot; TFT Display Emulator
          </h2>
          <p className="text-xs text-slate-400 font-mono">
            SPI1 (PB0 CS / PB6 DC / PB5 RST) · {width}×{height} RGB565
          </p>
        </div>

        <div className="flex items-center gap-2">
          <button
            type="button"
            onClick={() => setPortrait((p) => !p)}
            className="px-3 py-1.5 text-xs font-medium rounded border border-slate-700 bg-slate-900 text-slate-200 hover:border-cyan-500/60 hover:text-cyan-300 transition-colors whitespace-nowrap shrink-0"
          >
            {portrait ? 'Landscape (320×240)' : 'Portrait (240×320)'}
          </button>
          <button
            type="button"
            onClick={() => setSleeping((s) => !s)}
            className="px-3 py-1.5 text-xs font-medium rounded border border-slate-700 bg-slate-900 text-slate-200 hover:border-amber-500/60 hover:text-amber-300 transition-colors whitespace-nowrap shrink-0"
          >
            {sleeping ? 'Wake Display' : 'Sleep Display'}
          </button>
        </div>
      </div>

      {/* Bezel & Canvas */}
      <div className="flex flex-col items-center justify-center bg-[#07090E] border border-slate-800 rounded p-4">
        <div className="relative bg-black p-2.5 rounded border border-slate-700 shadow-inner">
          <canvas
            ref={canvasRef}
            width={width}
            height={height}
            style={{
              opacity: backlightOpacity,
              imageRendering: 'pixelated',
            }}
            className={`block bg-black transition-opacity duration-200 ${
              portrait ? 'w-[240px] h-[320px]' : 'w-full max-w-[440px] aspect-[4/3]'
            }`}
          />
          {sleeping && (
            <div className="absolute inset-0 flex items-center justify-center pointer-events-none">
              <span className="text-xs font-mono text-slate-500 tracking-wider uppercase">
                ILI9341 Sleep Mode (DISPOFF)
              </span>
            </div>
          )}
        </div>

        {/* Physical Nucleo Button Ring & Heartbeat LED bar */}
        <div className="w-full max-w-[460px] mt-4 pt-3 border-t border-slate-800 flex flex-wrap items-center justify-between gap-3">
          <div className="flex items-center gap-2">
            {(
              [
                {
                  label: 'UP',
                  title: 'BTN_UP (PA1): Previous TFT screen',
                  onClick: () => setPage((p) => ((p + 2) % 3) as DisplayPage),
                },
                {
                  label: 'DOWN',
                  title: 'BTN_DOWN (PB4): Next TFT screen',
                  onClick: () => setPage((p) => ((p + 1) % 3) as DisplayPage),
                },
                {
                  label: 'OK',
                  title: 'BTN_OK (PF0): Poll sensors now',
                  onClick: onManualPoll,
                },
                {
                  label: 'BACK',
                  title: 'BTN_BACK (PA12): Log row to SD',
                  onClick: onManualSdLog,
                },
              ] as const
            ).map((btn) => (
              <button
                key={btn.label}
                type="button"
                title={btn.title}
                onClick={btn.onClick}
                className={`px-3 py-1.5 text-xs font-mono font-medium rounded border transition-colors whitespace-nowrap shrink-0 ${
                  sample.btnLedEn
                    ? 'bg-amber-950/40 border-amber-400/70 text-amber-200 hover:bg-amber-900/50'
                    : 'bg-slate-900 border-slate-700 text-slate-300 hover:border-cyan-500/60 hover:text-cyan-200'
                }`}
              >
                {btn.label}
              </button>
            ))}
          </div>

          <div className="flex items-center gap-4 text-xs font-mono text-slate-400">
            <span>
              PA11 Ring:{' '}
              <span className={sample.btnLedEn ? 'text-amber-400' : 'text-slate-300'}>
                {sample.btnLedEn ? 'ON (<20 lux)' : 'OFF'}
              </span>
            </span>
            <span>
              PB8 LD2:{' '}
              <span className={heartbeatOn ? 'text-emerald-400' : 'text-slate-500'}>
                {heartbeatOn ? 'HIGH' : 'LOW'}
              </span>
            </span>
          </div>
        </div>
      </div>

      {/* Screen page selector */}
      <div className="flex items-center justify-between text-xs text-slate-400">
        <div className="flex items-center gap-1 p-1 bg-[#07090E] border border-slate-800 rounded">
          {(
            [
              { id: 0, label: 'Dashboard' },
              { id: 1, label: 'Bus & Pins' },
              { id: 2, label: 'SD & QNH' },
            ] as const
          ).map((tab) => (
            <button
              key={tab.id}
              type="button"
              onClick={() => setPage(tab.id)}
              className={`px-2.5 py-1 rounded text-xs font-medium transition-colors whitespace-nowrap shrink-0 ${
                page === tab.id
                  ? 'bg-slate-800 text-cyan-300'
                  : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              {tab.label}
            </button>
          ))}
        </div>
        <span className="font-mono tabular-nums">
          TIM2_CH1 PWM: {sample.pwmDuty} / 999
        </span>
      </div>
    </div>
  );
};
