import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  Download,
  Pause,
  Play,
  RefreshCw,
  RotateCcw,
} from 'lucide-react';
import { Ili9341Display } from './components/Ili9341Display';
import { SdLoggerPanel } from './components/SdLoggerPanel';
import { TelemetryCharts } from './components/TelemetryCharts';
import { UsartConsole } from './components/UsartConsole';
import { WiringRouterPanel } from './components/WiringRouterPanel';
import {
  APP_CONFIG,
  CSV_HEADER,
  CsvLogRow,
  ENVIRONMENT_PRESETS,
  EnvironmentInputs,
  FW_VERSION,
  GAIN_LABELS,
  IaqCategory,
  RESOLUTION_LABELS,
  SerialLogEntry,
  StationMode,
  TelemetrySample,
  buildBootSerialLogs,
  calcDewPoint,
  calcSeaLevelPressure,
  computeIaq,
  computeLtr390,
  computePwmDuty,
  formatCsvRow,
  formatTelemetrySerialLines,
  isButtonLedOn,
} from './firmware/sensorEngine';

type WorkspaceView = 'overview' | 'wiring' | 'charts' | 'sdlog' | 'serial';

const INITIAL_INPUTS: EnvironmentInputs = {
  temperatureC: 22.8,
  humidityPct: 44.0,
  pressureHpa: 1013.2,
  altitudeM: 120,
  lux: 310,
  uvIndex: 0.2,
  gasResistanceKOhm: 220,
  alsGainIdx: 1, // 3x
  uvsGainIdx: 4, // 18x
  resolutionIdx: 2, // 18-bit (100 ms)
};

function formatClock(date: Date): string {
  return date.toTimeString().slice(0, 8);
}

function getIaqBadgeStyle(category: IaqCategory): {
  glyph: string;
  colorClass: string;
} {
  switch (category) {
    case 'Good':
      return { glyph: '●', colorClass: 'text-emerald-400' };
    case 'Moderate':
      return { glyph: '▲', colorClass: 'text-amber-400' };
    case 'Sensitive':
    case 'Unhealthy':
      return { glyph: '▲', colorClass: 'text-orange-400' };
    case 'Very Unhealthy':
    case 'Hazardous':
      return { glyph: '✖', colorClass: 'text-rose-400' };
    default:
      return { glyph: '—', colorClass: 'text-slate-400' };
  }
}

export function App() {
  const [view, setView] = useState<WorkspaceView>('overview');
  const [mode, setMode] = useState<StationMode>('indoor');
  const [enableSdLogging, setEnableSdLogging] = useState<boolean>(
    APP_CONFIG.ENABLE_SD_LOGGING
  );
  const [inputs, setInputs] = useState<EnvironmentInputs>(INITIAL_INPUTS);
  const [running, setRunning] = useState(true);
  const [naturalDrift, setNaturalDrift] = useState(true);
  const [heartbeatOn, setHeartbeatOn] = useState(true);

  const uptimeSecRef = useRef(0);
  const sampleIdRef = useRef(1);
  const serialIdRef = useRef(1);
  const gasBaselineRef = useRef(INITIAL_INPUTS.gasResistanceKOhm * 1000);
  const lastSdLogSecRef = useRef(0);

  const createSample = useCallback(
    (env: EnvironmentInputs, stMode: StationMode, elapsedSec: number): TelemetrySample => {
      const dewPoint = calcDewPoint(env.temperatureC, env.humidityPct);
      const seaLevelPressure = calcSeaLevelPressure(
        env.pressureHpa,
        env.altitudeM,
        env.temperatureC
      );
      const gasOhm = env.gasResistanceKOhm * 1000;
      const iaq = computeIaq(
        gasOhm,
        env.humidityPct,
        gasBaselineRef.current,
        stMode === 'indoor'
      );
      gasBaselineRef.current = iaq.nextBaselineOhm;

      const ltr = computeLtr390(env);
      const pwmDuty = computePwmDuty(ltr.lux);
      const btnLedEn = isButtonLedOn(ltr.lux);

      return {
        id: sampleIdRef.current++,
        timestampSec: elapsedSec,
        clockLabel: formatClock(new Date()),
        mode: stMode,
        bme: {
          temperature: env.temperatureC,
          pressure: env.pressureHpa,
          seaLevelPressure,
          humidity: env.humidityPct,
          dewPoint,
          gasResistanceOhm: iaq.gasValid ? gasOhm : 0,
          gasValid: iaq.gasValid,
          iaqScore: iaq.iaqScore,
          iaqText: iaq.iaqText,
        },
        ltr,
        pwmDuty,
        btnLedEn,
      };
    },
    []
  );

  const [history, setHistory] = useState<TelemetrySample[]>(() => {
    const initialSample = createSample(INITIAL_INPUTS, 'indoor', 0);
    return [initialSample];
  });

  const [csvRows, setCsvRows] = useState<CsvLogRow[]>(() => {
    if (!APP_CONFIG.ENABLE_SD_LOGGING) return [];
    const s = createSample(INITIAL_INPUTS, 'indoor', 0);
    return [
      {
        timeSec: 0,
        tempC: s.bme.temperature,
        presHpa: s.bme.pressure,
        humPct: s.bme.humidity,
        dewPointC: s.bme.dewPoint,
        gasKOhm: s.bme.gasValid ? s.bme.gasResistanceOhm / 1000 : 0,
        iaqScore: s.bme.gasValid ? s.bme.iaqScore : 0,
        iaqStatus: s.bme.iaqText,
        lux: s.ltr.lux,
        uvIndex: s.ltr.uvIndex,
      },
    ];
  });

  const [serialLogs, setSerialLogs] = useState<SerialLogEntry[]>(() => {
    const now = formatClock(new Date());
    return buildBootSerialLogs('indoor', APP_CONFIG.ENABLE_SD_LOGGING).map((text) => ({
      id: serialIdRef.current++,
      timestamp: now,
      level: text.startsWith('SD_Logger') ? 'sd' : 'info',
      text,
    }));
  });

  const latestSample = history[history.length - 1];

  const appendSerialLines = useCallback(
    (lines: string[], level: SerialLogEntry['level'] = 'sensor') => {
      const ts = formatClock(new Date());
      setSerialLogs((prev) => [
        ...prev.slice(-140),
        ...lines.map((text) => ({
          id: serialIdRef.current++,
          timestamp: ts,
          level,
          text,
        })),
      ]);
    },
    []
  );

  const appendCsvRow = useCallback(
    (sample: TelemetrySample) => {
      const row: CsvLogRow = {
        timeSec: sample.timestampSec,
        tempC: sample.bme.temperature,
        presHpa: sample.bme.pressure,
        humPct: sample.bme.humidity,
        dewPointC: sample.bme.dewPoint,
        gasKOhm: sample.bme.gasValid ? sample.bme.gasResistanceOhm / 1000 : 0,
        iaqScore: sample.bme.gasValid ? sample.bme.iaqScore : 0,
        iaqStatus: sample.bme.iaqText,
        lux: sample.ltr.lux,
        uvIndex: sample.ltr.uvIndex,
      };
      setCsvRows((prev) => [...prev.slice(-240), row]);
      appendSerialLines(
        [`SD_Logger: Appended row at T+${sample.timestampSec}s to ${APP_CONFIG.SD_LOG_FILENAME}`],
        'sd'
      );
    },
    [appendSerialLines]
  );

  const pollOnce = useCallback(
    (forceSdLog = false) => {
      uptimeSecRef.current += 2;
      const nextElapsed = uptimeSecRef.current;

      setInputs((currInputs) => {
        const nextEnv = naturalDrift
          ? {
              ...currInputs,
              temperatureC: Number(
                Math.min(
                  45,
                  Math.max(-10, currInputs.temperatureC + (Math.random() - 0.5) * 0.16)
                ).toFixed(2)
              ),
              humidityPct: Number(
                Math.min(
                  95,
                  Math.max(10, currInputs.humidityPct + (Math.random() - 0.5) * 0.4)
                ).toFixed(1)
              ),
              pressureHpa: Number(
                Math.min(
                  1040,
                  Math.max(960, currInputs.pressureHpa + (Math.random() - 0.5) * 0.1)
                ).toFixed(2)
              ),
              lux: Math.round(
                Math.min(1200, Math.max(0, currInputs.lux + (Math.random() - 0.5) * 8))
              ),
              gasResistanceKOhm: Number(
                Math.min(
                  350,
                  Math.max(10, currInputs.gasResistanceKOhm + (Math.random() - 0.5) * 2.5)
                ).toFixed(1)
              ),
            }
          : currInputs;

        const nextSample = createSample(nextEnv, mode, nextElapsed);
        setHistory((prev) => [...prev.slice(-119), nextSample]);
        setHeartbeatOn((h) => !h);
        appendSerialLines(formatTelemetrySerialLines(nextSample), 'sensor');

        if (
          enableSdLogging &&
          (forceSdLog ||
            nextElapsed - lastSdLogSecRef.current >= APP_CONFIG.SD_LOG_INTERVAL_MS / 1000)
        ) {
          lastSdLogSecRef.current = nextElapsed;
          appendCsvRow(nextSample);
        }

        return nextEnv;
      });
    },
    [naturalDrift, mode, enableSdLogging, createSample, appendSerialLines, appendCsvRow]
  );

  useEffect(() => {
    if (!running) return;
    const timer = window.setInterval(() => {
      pollOnce(false);
    }, APP_CONFIG.SENSOR_POLL_MS);
    return () => window.clearInterval(timer);
  }, [running, pollOnce]);

  const handleModeChange = (nextMode: StationMode) => {
    if (nextMode === mode) return;
    setMode(nextMode);
    const nextSample = createSample(inputs, nextMode, uptimeSecRef.current);
    setHistory((prev) => [...prev.slice(-119), nextSample]);
    appendSerialLines(
      [
        `Build switch: OUTDOOR_MODE = ${nextMode === 'outdoor' ? '1 [OUTDOOR]' : '0 [INDOOR]'}`,
        nextMode === 'outdoor'
          ? 'BME680 gas heater bypassed; Stop Mode profile active.'
          : 'BME680 gas heater enabled (320 C / 150 ms); IAQ scoring active.',
      ],
      'warn'
    );
  };

  const handleSdLoggingToggle = (enabled: boolean) => {
    if (enabled === enableSdLogging) return;
    setEnableSdLogging(enabled);
    if (enabled) {
      lastSdLogSecRef.current = uptimeSecRef.current;
      appendSerialLines(
        [
          'Build switch: ENABLE_SD_LOGGING = 1',
          'MicroSD: OK (Type 0x0C SDHC, Capacity: 15288 MB)',
          `SD_Logger: Initialized ${APP_CONFIG.SD_LOG_FILENAME}`,
        ],
        'sd'
      );
      appendCsvRow(latestSample);
    } else {
      appendSerialLines(
        [
          'Build switch: ENABLE_SD_LOGGING = 0',
          `MicroSD: Disabled (v${FW_VERSION})`,
        ],
        'warn'
      );
    }
  };

  const applyPreset = (presetId: string) => {
    const preset = ENVIRONMENT_PRESETS.find((p) => p.id === presetId);
    if (!preset) return;
    const merged: EnvironmentInputs = { ...inputs, ...preset.values };
    const nextMode = preset.mode ?? mode;
    setInputs(merged);
    setMode(nextMode);
    const sample = createSample(merged, nextMode, uptimeSecRef.current);
    setHistory((prev) => [...prev.slice(-119), sample]);
    appendSerialLines(
      [
        `Preset loaded: ${preset.label} (${preset.description})`,
        ...formatTelemetrySerialLines(sample),
      ],
      'info'
    );
  };

  const handleExportCsv = () => {
    const content = [CSV_HEADER, ...csvRows.map(formatCsvRow)].join('\r\n') + '\r\n';
    const blob = new Blob([content], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = APP_CONFIG.SD_LOG_FILENAME;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  };

  const nextLogInSec = useMemo(() => {
    const elapsedSinceLog = latestSample.timestampSec - lastSdLogSecRef.current;
    const rem = Math.max(0, 10 - elapsedSinceLog);
    return rem;
  }, [latestSample.timestampSec]);

  const iaqBadge = getIaqBadgeStyle(latestSample.bme.iaqText);

  return (
    <div className="min-h-screen bg-[#07090E] text-slate-100 flex flex-col">
      {/* 3-Zone Top Bar Contract */}
      <header className="border-b border-slate-800 bg-[#0B0E17] px-6 py-3.5 flex items-center justify-between gap-8">
        <span className="text-base font-semibold tracking-tight text-slate-100 whitespace-nowrap shrink-0">
          STM32G431KB Weather Station
        </span>

        <nav className="hidden md:flex items-center gap-6 text-sm font-medium text-slate-400">
          {(
            [
              { id: 'overview', label: 'Console' },
              { id: 'wiring', label: 'Wiring & Pinout' },
              { id: 'charts', label: 'Time Series' },
              { id: 'sdlog', label: 'SD Log' },
              { id: 'serial', label: 'USART2 Serial' },
            ] as const
          ).map((item) => (
            <button
              key={item.id}
              type="button"
              onClick={() => setView(item.id)}
              className={`pb-0.5 transition-colors whitespace-nowrap shrink-0 border-b ${
                view === item.id
                  ? 'text-cyan-400 border-cyan-400'
                  : 'text-slate-400 border-transparent hover:text-slate-100'
              }`}
            >
              {item.label}
            </button>
          ))}
        </nav>

        <div className="flex items-center gap-3 shrink-0">
          <button
            type="button"
            onClick={handleExportCsv}
            className="inline-flex items-center gap-2 px-4 py-2 text-xs font-semibold rounded bg-cyan-500 text-slate-950 hover:bg-cyan-400 transition-colors whitespace-nowrap shrink-0"
          >
            <Download className="w-3.5 h-3.5" />
            Export WEATHER.CSV
          </button>
        </div>
      </header>

      {/* Mobile View Switcher */}
      <div className="md:hidden border-b border-slate-800 bg-[#0B0E17] px-4 py-2 flex items-center gap-2 overflow-x-auto">
        {(
          [
            { id: 'overview', label: 'Console' },
            { id: 'charts', label: 'Time Series' },
            { id: 'sdlog', label: 'SD Log' },
            { id: 'serial', label: 'USART2 Serial' },
          ] as const
        ).map((item) => (
          <button
            key={item.id}
            type="button"
            onClick={() => setView(item.id)}
            className={`px-3 py-1 text-xs font-medium rounded whitespace-nowrap shrink-0 ${
              view === item.id
                ? 'bg-slate-800 text-cyan-300'
                : 'text-slate-400 hover:text-slate-200'
            }`}
          >
            {item.label}
          </button>
        ))}
      </div>

      {/* Main Asymmetric Split Console */}
      <div className="flex-1 flex flex-col lg:flex-row max-w-[1600px] w-full mx-auto">
        {/* Left Control & Parameter Column */}
        <aside className="w-full lg:w-[340px] shrink-0 border-b lg:border-b-0 lg:border-r border-slate-800 bg-[#0B0E17] p-5 flex flex-col gap-5">
          {/* Firmware Mode Switch */}
          <section className="flex flex-col gap-2">
            <div className="flex items-center justify-between">
              <span className="text-xs font-semibold tracking-wider uppercase text-slate-400">
                Build Mode (OUTDOOR_MODE)
              </span>
              <span className="text-xs font-mono text-slate-400">
                {mode === 'indoor' ? '0' : '1'}
              </span>
            </div>
            <div className="grid grid-cols-2 gap-1 p-1 bg-[#07090E] border border-slate-800 rounded">
              <button
                type="button"
                onClick={() => handleModeChange('indoor')}
                className={`py-1.5 px-2 text-xs font-medium rounded transition-colors whitespace-nowrap ${
                  mode === 'indoor'
                    ? 'bg-slate-800 text-cyan-300'
                    : 'text-slate-400 hover:text-slate-200'
                }`}
              >
                Indoor (IAQ On)
              </button>
              <button
                type="button"
                onClick={() => handleModeChange('outdoor')}
                className={`py-1.5 px-2 text-xs font-medium rounded transition-colors whitespace-nowrap ${
                  mode === 'outdoor'
                    ? 'bg-slate-800 text-amber-300'
                    : 'text-slate-400 hover:text-slate-200'
                }`}
              >
                Outdoor (Bypass)
              </button>
            </div>
          </section>

          {/* SD Logging Build Gate (ENABLE_SD_LOGGING) */}
          <section className="flex flex-col gap-2">
            <div className="flex items-center justify-between">
              <span className="text-xs font-semibold tracking-wider uppercase text-slate-400">
                SD Logging (ENABLE_SD_LOGGING)
              </span>
              <span className="text-xs font-mono text-slate-400">
                {enableSdLogging ? '1' : '0'}
              </span>
            </div>
            <div className="grid grid-cols-2 gap-1 p-1 bg-[#07090E] border border-slate-800 rounded">
              <button
                type="button"
                onClick={() => handleSdLoggingToggle(false)}
                className={`py-1.5 px-2 text-xs font-medium rounded transition-colors whitespace-nowrap ${
                  !enableSdLogging
                    ? 'bg-slate-800 text-amber-300'
                    : 'text-slate-400 hover:text-slate-200'
                }`}
              >
                Disabled (v1.0.0)
              </button>
              <button
                type="button"
                onClick={() => handleSdLoggingToggle(true)}
                className={`py-1.5 px-2 text-xs font-medium rounded transition-colors whitespace-nowrap ${
                  enableSdLogging
                    ? 'bg-slate-800 text-emerald-300'
                    : 'text-slate-400 hover:text-slate-200'
                }`}
              >
                Enabled (CSV)
              </button>
            </div>
          </section>

          {/* Sampling Loop Controls */}
          <section className="flex flex-col gap-2 pt-4 border-t border-slate-800/80">
            <div className="flex items-center justify-between">
              <span className="text-xs font-semibold tracking-wider uppercase text-slate-400">
                Main Loop (2000 ms)
              </span>
              <span className="text-xs font-mono tabular-nums text-slate-400">
                T+{latestSample.timestampSec}s
              </span>
            </div>
            <div className="grid grid-cols-2 gap-2">
              <button
                type="button"
                onClick={() => setRunning((r) => !r)}
                className="inline-flex items-center justify-center gap-1.5 py-2 px-3 text-xs font-medium rounded border border-slate-700 bg-slate-900 text-slate-200 hover:border-cyan-500/60 transition-colors whitespace-nowrap"
              >
                {running ? (
                  <>
                    <Pause className="w-3.5 h-3.5 text-amber-400" />
                    Pause Loop
                  </>
                ) : (
                  <>
                    <Play className="w-3.5 h-3.5 text-emerald-400" />
                    Resume Loop
                  </>
                )}
              </button>
              <button
                type="button"
                onClick={() => pollOnce(false)}
                className="inline-flex items-center justify-center gap-1.5 py-2 px-3 text-xs font-medium rounded border border-slate-700 bg-slate-900 text-slate-200 hover:border-cyan-500/60 transition-colors whitespace-nowrap"
              >
                <RefreshCw className="w-3.5 h-3.5 text-cyan-400" />
                Poll Now
              </button>
            </div>

            <div className="flex items-center justify-between pt-1">
              <label className="flex items-center gap-2 text-xs text-slate-300 cursor-pointer select-none">
                <input
                  type="checkbox"
                  checked={naturalDrift}
                  onChange={(e) => setNaturalDrift(e.target.checked)}
                  className="accent-cyan-500 rounded"
                />
                Sensor micro-drift
              </label>

              <button
                type="button"
                onClick={() => {
                  gasBaselineRef.current = inputs.gasResistanceKOhm * 1000;
                  const s = createSample(inputs, mode, uptimeSecRef.current);
                  setHistory((prev) => [...prev.slice(-119), s]);
                  appendSerialLines(
                    [
                      `BME680: Gas baseline recalibrated to ${inputs.gasResistanceKOhm.toFixed(
                        1
                      )} kOhm`,
                    ],
                    'info'
                  );
                }}
                className="inline-flex items-center gap-1 text-xs text-slate-400 hover:text-cyan-300 transition-colors"
              >
                <RotateCcw className="w-3 h-3" />
                Reset IAQ Baseline
              </button>
            </div>
          </section>

          {/* Environment Presets */}
          <section className="flex flex-col gap-2 pt-4 border-t border-slate-800/80">
            <span className="text-xs font-semibold tracking-wider uppercase text-slate-400">
              Environment Scenarios
            </span>
            <div className="grid grid-cols-2 gap-1.5">
              {ENVIRONMENT_PRESETS.map((preset) => (
                <button
                  key={preset.id}
                  type="button"
                  onClick={() => applyPreset(preset.id)}
                  className="text-left px-2.5 py-2 text-xs font-medium rounded border border-slate-800 bg-[#07090E] text-slate-300 hover:border-cyan-500/60 hover:text-cyan-200 transition-colors truncate"
                  title={preset.description}
                >
                  {preset.label}
                </button>
              ))}
            </div>
          </section>

          {/* Sensor Input Sliders */}
          <section className="flex flex-col gap-3.5 pt-4 border-t border-slate-800/80">
            <span className="text-xs font-semibold tracking-wider uppercase text-slate-400">
              Simulated Physical Inputs
            </span>

            {[
              {
                label: 'Temperature',
                value: inputs.temperatureC,
                min: -10,
                max: 45,
                step: 0.1,
                unit: '°C',
                format: (v: number) => v.toFixed(1),
                onChange: (v: number) =>
                  setInputs((p) => ({ ...p, temperatureC: v })),
              },
              {
                label: 'Relative Humidity',
                value: inputs.humidityPct,
                min: 10,
                max: 95,
                step: 0.5,
                unit: '%',
                format: (v: number) => v.toFixed(1),
                onChange: (v: number) =>
                  setInputs((p) => ({ ...p, humidityPct: v })),
              },
              {
                label: 'Station Pressure',
                value: inputs.pressureHpa,
                min: 960,
                max: 1040,
                step: 0.2,
                unit: 'hPa',
                format: (v: number) => v.toFixed(1),
                onChange: (v: number) =>
                  setInputs((p) => ({ ...p, pressureHpa: v })),
              },
              {
                label: 'Station Altitude',
                value: inputs.altitudeM,
                min: 0,
                max: 2000,
                step: 10,
                unit: 'm',
                format: (v: number) => v.toFixed(0),
                onChange: (v: number) =>
                  setInputs((p) => ({ ...p, altitudeM: v })),
              },
              {
                label: 'Ambient Light',
                value: inputs.lux,
                min: 0,
                max: 1200,
                step: 5,
                unit: 'Lux',
                format: (v: number) => v.toFixed(0),
                onChange: (v: number) => setInputs((p) => ({ ...p, lux: v })),
              },
              {
                label: 'UV Index',
                value: inputs.uvIndex,
                min: 0,
                max: 11,
                step: 0.1,
                unit: 'UVI',
                format: (v: number) => v.toFixed(1),
                onChange: (v: number) =>
                  setInputs((p) => ({ ...p, uvIndex: v })),
              },
              {
                label: 'VOC Gas Resistance',
                value: inputs.gasResistanceKOhm,
                min: 10,
                max: 350,
                step: 1,
                unit: 'kΩ',
                format: (v: number) => v.toFixed(0),
                onChange: (v: number) =>
                  setInputs((p) => ({ ...p, gasResistanceKOhm: v })),
              },
            ].map((ctrl) => (
              <div key={ctrl.label} className="flex flex-col gap-1">
                <div className="flex items-center justify-between text-xs">
                  <span className="text-slate-300">{ctrl.label}</span>
                  <span className="font-mono tabular-nums text-cyan-300">
                    {ctrl.format(ctrl.value)}{' '}
                    <span className="text-slate-500">{ctrl.unit}</span>
                  </span>
                </div>
                <input
                  type="range"
                  min={ctrl.min}
                  max={ctrl.max}
                  step={ctrl.step}
                  value={ctrl.value}
                  onChange={(e) => ctrl.onChange(Number(e.target.value))}
                  className="lab-slider w-full"
                />
              </div>
            ))}
          </section>

          {/* LTR390 ADC & Gain Config */}
          <section className="flex flex-col gap-2.5 pt-4 border-t border-slate-800/80">
            <span className="text-xs font-semibold tracking-wider uppercase text-slate-400">
              LTR-390UV ADC Config
            </span>
            <div className="grid grid-cols-2 gap-2">
              <label className="flex flex-col gap-1 text-xs text-slate-400">
                ALS Gain
                <select
                  value={inputs.alsGainIdx}
                  onChange={(e) =>
                    setInputs((p) => ({
                      ...p,
                      alsGainIdx: Number(e.target.value),
                    }))
                  }
                  className="bg-[#07090E] border border-slate-800 rounded px-2 py-1.5 text-xs font-mono text-slate-200"
                >
                  {GAIN_LABELS.map((g, i) => (
                    <option key={g} value={i}>
                      {g}
                    </option>
                  ))}
                </select>
              </label>

              <label className="flex flex-col gap-1 text-xs text-slate-400">
                UVS Gain
                <select
                  value={inputs.uvsGainIdx}
                  onChange={(e) =>
                    setInputs((p) => ({
                      ...p,
                      uvsGainIdx: Number(e.target.value),
                    }))
                  }
                  className="bg-[#07090E] border border-slate-800 rounded px-2 py-1.5 text-xs font-mono text-slate-200"
                >
                  {GAIN_LABELS.map((g, i) => (
                    <option key={g} value={i}>
                      {g}
                    </option>
                  ))}
                </select>
              </label>
            </div>

            <label className="flex flex-col gap-1 text-xs text-slate-400">
              ADC Resolution
              <select
                value={inputs.resolutionIdx}
                onChange={(e) =>
                  setInputs((p) => ({
                    ...p,
                    resolutionIdx: Number(e.target.value),
                  }))
                }
                className="bg-[#07090E] border border-slate-800 rounded px-2 py-1.5 text-xs font-mono text-slate-200"
              >
                {RESOLUTION_LABELS.map((label, i) => (
                  <option key={label} value={i}>
                    {label}
                  </option>
                ))}
              </select>
            </label>
          </section>
        </aside>

        {/* Main Telemetry Stage */}
        <main className="flex-1 min-w-0 p-5 lg:p-6 flex flex-col gap-6">
          {/* 8-Metric Precision Telemetry Strip */}
          <section className="grid grid-cols-2 sm:grid-cols-4 xl:grid-cols-8 gap-3">
            <div className="bg-[#111827] border border-slate-800 rounded-md p-3.5">
              <div className="text-[11px] uppercase tracking-wider text-slate-400">
                Temperature
              </div>
              <div className="mt-1 flex items-baseline font-mono tabular-nums">
                <span className="text-2xl font-semibold text-slate-100">
                  {latestSample.bme.temperature.toFixed(1)}
                </span>
                <span className="text-xs text-slate-400 ml-1">°C</span>
              </div>
            </div>

            <div className="bg-[#111827] border border-slate-800 rounded-md p-3.5">
              <div className="text-[11px] uppercase tracking-wider text-slate-400">
                Dew Point
              </div>
              <div className="mt-1 flex items-baseline font-mono tabular-nums">
                <span className="text-2xl font-semibold text-slate-100">
                  {latestSample.bme.dewPoint.toFixed(1)}
                </span>
                <span className="text-xs text-slate-400 ml-1">°C</span>
              </div>
            </div>

            <div className="bg-[#111827] border border-slate-800 rounded-md p-3.5">
              <div className="text-[11px] uppercase tracking-wider text-slate-400">
                Humidity
              </div>
              <div className="mt-1 flex items-baseline font-mono tabular-nums">
                <span className="text-2xl font-semibold text-slate-100">
                  {latestSample.bme.humidity.toFixed(1)}
                </span>
                <span className="text-xs text-slate-400 ml-1">%</span>
              </div>
            </div>

            <div className="bg-[#111827] border border-slate-800 rounded-md p-3.5">
              <div className="text-[11px] uppercase tracking-wider text-slate-400">
                Pressure
              </div>
              <div className="mt-1 flex items-baseline font-mono tabular-nums">
                <span className="text-2xl font-semibold text-slate-100">
                  {latestSample.bme.pressure.toFixed(1)}
                </span>
                <span className="text-xs text-slate-400 ml-1">hPa</span>
              </div>
            </div>

            <div className="bg-[#111827] border border-slate-800 rounded-md p-3.5">
              <div className="text-[11px] uppercase tracking-wider text-slate-400">
                Ambient Light
              </div>
              <div className="mt-1 flex items-baseline font-mono tabular-nums">
                <span className="text-2xl font-semibold text-amber-300">
                  {latestSample.ltr.lux.toFixed(0)}
                </span>
                <span className="text-xs text-slate-400 ml-1">Lux</span>
              </div>
            </div>

            <div className="bg-[#111827] border border-slate-800 rounded-md p-3.5">
              <div className="text-[11px] uppercase tracking-wider text-slate-400">
                UV Index
              </div>
              <div className="mt-1 flex items-baseline font-mono tabular-nums">
                <span className="text-2xl font-semibold text-amber-300">
                  {latestSample.ltr.uvIndex.toFixed(1)}
                </span>
                <span className="text-xs text-slate-400 ml-1">UVI</span>
              </div>
            </div>

            <div className="bg-[#111827] border border-slate-800 rounded-md p-3.5">
              <div className="text-[11px] uppercase tracking-wider text-slate-400">
                IAQ Score
              </div>
              <div className="mt-1 flex items-baseline font-mono tabular-nums">
                <span className="text-2xl font-semibold text-slate-100">
                  {latestSample.bme.gasValid
                    ? latestSample.bme.iaqScore.toFixed(0)
                    : '—'}
                </span>
                <span className={`text-xs ml-1.5 ${iaqBadge.colorClass}`}>
                  {iaqBadge.glyph} {latestSample.bme.iaqText}
                </span>
              </div>
            </div>

            <div className="bg-[#111827] border border-slate-800 rounded-md p-3.5">
              <div className="text-[11px] uppercase tracking-wider text-slate-400">
                Gas Resistance
              </div>
              <div className="mt-1 flex items-baseline font-mono tabular-nums">
                <span className="text-2xl font-semibold text-slate-100">
                  {latestSample.bme.gasValid
                    ? (latestSample.bme.gasResistanceOhm / 1000).toFixed(1)
                    : '—'}
                </span>
                <span className="text-xs text-slate-400 ml-1">kΩ</span>
              </div>
            </div>
          </section>

          {/* Console Overview: ILI9341 Display + Hardware Peripheral Matrix */}
          {view === 'overview' && (
            <>
              <section className="grid grid-cols-1 xl:grid-cols-12 gap-5 items-stretch">
                <div className="xl:col-span-7">
                  <Ili9341Display
                    sample={latestSample}
                    mode={mode}
                    enableSdLogging={enableSdLogging}
                    heartbeatOn={heartbeatOn}
                    csvRowCount={csvRows.length}
                    gasBaselineOhm={gasBaselineRef.current}
                    onManualPoll={() => pollOnce(false)}
                    onManualSdLog={() => {
                      if (enableSdLogging) pollOnce(true);
                    }}
                  />
                </div>

                <div className="xl:col-span-5 bg-[#111827] border border-slate-800 rounded-md p-4 flex flex-col justify-between gap-4">
                  <div className="flex items-start justify-between gap-3">
                    <div>
                      <h2 className="text-sm font-semibold text-slate-100">
                        STM32G431KB Peripheral &amp; Telemetry Bus
                      </h2>
                      <p className="text-xs text-slate-400 mt-0.5">
                        Cortex-M4 @ 170 MHz · Live register &amp; derived physics state
                      </p>
                    </div>
                    <button
                      type="button"
                      onClick={() => setView('wiring')}
                      className="px-2.5 py-1 text-[11px] font-medium rounded border border-cyan-500/40 bg-cyan-500/10 text-cyan-300 hover:bg-cyan-500/20 transition-colors shrink-0"
                    >
                      Wiring Canvas →
                    </button>
                  </div>

                  <dl className="divide-y divide-slate-800/80 text-xs font-mono tabular-nums">
                    <div className="py-2.5 flex items-center justify-between gap-4">
                      <dt className="text-slate-400">Sea-Level QNH (Altitude {inputs.altitudeM} m)</dt>
                      <dd className="text-cyan-300 font-medium">
                        {latestSample.bme.seaLevelPressure.toFixed(2)} hPa
                      </dd>
                    </div>
                    <div className="py-2.5 flex items-center justify-between gap-4">
                      <dt className="text-slate-400">Clean-Air Gas Baseline</dt>
                      <dd className="text-slate-200">
                        {(gasBaselineRef.current / 1000).toFixed(1)} kΩ
                      </dd>
                    </div>
                    <div className="py-2.5 flex items-center justify-between gap-4">
                      <dt className="text-slate-400">LTR390 ALS / UVS Raw ADC</dt>
                      <dd className="text-slate-200">
                        {latestSample.ltr.alsRaw} / {latestSample.ltr.uvsRaw}
                        {latestSample.ltr.saturated ? ' (SAT)' : ''}
                      </dd>
                    </div>
                    <div className="py-2.5 flex items-center justify-between gap-4">
                      <dt className="text-slate-400">TIM2_CH1 Backlight (PA0)</dt>
                      <dd className="text-slate-200">
                        {latestSample.pwmDuty} / 999 (
                        {((latestSample.pwmDuty / 999) * 100).toFixed(0)}%)
                      </dd>
                    </div>
                    <div className="py-2.5 flex items-center justify-between gap-4">
                      <dt className="text-slate-400">Night Button LED (PA11)</dt>
                      <dd
                        className={
                          latestSample.btnLedEn ? 'text-amber-400' : 'text-slate-300'
                        }
                      >
                        {latestSample.btnLedEn
                          ? 'HIGH (Lux < 20.0)'
                          : 'LOW (Lux ≥ 20.0)'}
                      </dd>
                    </div>
                    <div className="py-2.5 flex items-center justify-between gap-4">
                      <dt className="text-slate-400">I2C2 Sensors (PA9/PA8)</dt>
                      <dd className="text-emerald-400">0x77 BME680 · 0x53 LTR390</dd>
                    </div>
                    <div className="py-2.5 flex items-center justify-between gap-4">
                      <dt className="text-slate-400">SPI1 Shared Bus (PA5/6/7)</dt>
                      <dd className="text-slate-200">
                        TFT (PB0) · MicroSD (PA4)
                      </dd>
                    </div>
                    <div className="py-2.5 flex items-center justify-between gap-4">
                      <dt className="text-slate-400">MicroSD Logger (ENABLE_SD_LOGGING)</dt>
                      <dd
                        className={
                          enableSdLogging ? 'text-emerald-400' : 'text-slate-400'
                        }
                      >
                        {enableSdLogging
                          ? `Enabled (next in ${nextLogInSec}s)`
                          : `Disabled (v${FW_VERSION})`}
                      </dd>
                    </div>
                  </dl>
                </div>
              </section>

              <TelemetryCharts history={history} />

              <section className="grid grid-cols-1 xl:grid-cols-2 gap-5">
                <SdLoggerPanel
                  rows={csvRows}
                  enableSdLogging={enableSdLogging}
                  nextLogInSec={nextLogInSec}
                  onToggleSdLogging={handleSdLoggingToggle}
                  onLogNow={() => pollOnce(true)}
                  onClear={() => setCsvRows([])}
                  onExportCsv={handleExportCsv}
                />
                <UsartConsole
                  logs={serialLogs}
                  onClear={() => setSerialLogs([])}
                />
              </section>
            </>
          )}

          {view === 'wiring' && <WiringRouterPanel />}

          {view === 'charts' && <TelemetryCharts history={history} />}

          {view === 'sdlog' && (
            <SdLoggerPanel
              rows={csvRows}
              enableSdLogging={enableSdLogging}
              nextLogInSec={nextLogInSec}
              onToggleSdLogging={handleSdLoggingToggle}
              onLogNow={() => pollOnce(true)}
              onClear={() => setCsvRows([])}
              onExportCsv={handleExportCsv}
            />
          )}

          {view === 'serial' && (
            <UsartConsole
              logs={serialLogs}
              onClear={() => setSerialLogs([])}
            />
          )}
        </main>
      </div>
    </div>
  );
}

export default App;
