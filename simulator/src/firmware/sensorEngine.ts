// Faithful TypeScript port of STM32G431KB Weather Station sensor math & firmware constants
// Sources: Core/Inc/app_config.h, Core/Src/bme680.c, Core/Src/ltr390.c, Core/Src/sd_logger.c, Core/Src/main.c

export const FW_VERSION = '1.0.0';

export const APP_CONFIG = {
  BME680_I2C_ADDR: 0x77,
  LTR390_I2C_ADDR: 0x53,
  SENSOR_POLL_MS: 2000,
  SD_LOG_INTERVAL_MS: 10000,
  BME680_GAS_TEMP: 320,
  BME680_GAS_DUR: 150,
  BTN_LED_LUX_THRESH: 20.0,
  SD_LOG_FILENAME: 'WEATHER.CSV',
  ENABLE_SD_LOGGING: false, // #define ENABLE_SD_LOGGING 0 (disabled for v1.0.0)
} as const;

export type StationMode = 'indoor' | 'outdoor';

export type IaqCategory =
  | 'Good'
  | 'Moderate'
  | 'Sensitive'
  | 'Unhealthy'
  | 'Very Unhealthy'
  | 'Hazardous'
  | 'N/A';

export interface EnvironmentInputs {
  temperatureC: number;
  humidityPct: number;
  pressureHpa: number;
  altitudeM: number;
  lux: number;
  uvIndex: number;
  gasResistanceKOhm: number;
  alsGainIdx: number; // 0=1x, 1=3x, 2=6x, 3=9x, 4=18x
  uvsGainIdx: number; // 0..4
  resolutionIdx: number; // 0=20bit/400ms .. 4=16bit/25ms
}

export interface BME680Data {
  temperature: number;
  pressure: number;
  seaLevelPressure: number;
  humidity: number;
  dewPoint: number;
  gasResistanceOhm: number;
  gasValid: boolean;
  iaqScore: number;
  iaqText: IaqCategory;
}

export interface LTR390Data {
  alsRaw: number;
  uvsRaw: number;
  lux: number;
  uvIndex: number;
  saturated: boolean;
}

export interface TelemetrySample {
  id: number;
  timestampSec: number;
  clockLabel: string;
  mode: StationMode;
  bme: BME680Data;
  ltr: LTR390Data;
  pwmDuty: number;
  btnLedEn: boolean;
}

export interface CsvLogRow {
  timeSec: number;
  tempC: number;
  presHpa: number;
  humPct: number;
  dewPointC: number;
  gasKOhm: number;
  iaqScore: number;
  iaqStatus: IaqCategory;
  lux: number;
  uvIndex: number;
}

export interface SerialLogEntry {
  id: number;
  timestamp: string;
  level: 'info' | 'sensor' | 'sd' | 'warn';
  text: string;
}

export const GAIN_FACTORS = [1.0, 3.0, 6.0, 9.0, 18.0] as const;
export const GAIN_LABELS = ['1x', '3x', '6x', '9x', '18x'] as const;
export const INTEG_FACTORS = [4.0, 2.0, 1.0, 0.5, 0.25] as const;
export const RESOLUTION_LABELS = [
  '20-bit (400 ms)',
  '19-bit (200 ms)',
  '18-bit (100 ms)',
  '17-bit (50 ms)',
  '16-bit (25 ms)',
] as const;
export const MAX_ADC_COUNTS = [1048575, 524287, 262143, 131071, 65535] as const;

export const CSV_HEADER =
  'Time_s,Temp_C,Pres_hPa,Hum_pct,DewPoint_C,Gas_kOhm,IAQ_Score,IAQ_Status,Lux,UV_Index';

export interface EnvironmentPreset {
  id: string;
  label: string;
  description: string;
  mode?: StationMode;
  values: Partial<EnvironmentInputs>;
}

export const ENVIRONMENT_PRESETS: EnvironmentPreset[] = [
  {
    id: 'lab-bench',
    label: 'Lab Bench',
    description: 'Indoor 22.8 °C, 44% RH, clean VOC baseline, 310 lux',
    mode: 'indoor',
    values: {
      temperatureC: 22.8,
      humidityPct: 44.0,
      pressureHpa: 1013.2,
      lux: 310,
      uvIndex: 0.2,
      gasResistanceKOhm: 220,
    },
  },
  {
    id: 'bright-midday',
    label: 'Bright Midday',
    description: 'Outdoor 27.4 °C, 38% RH, 940 lux, UV 7.4',
    mode: 'outdoor',
    values: {
      temperatureC: 27.4,
      humidityPct: 38.0,
      pressureHpa: 1018.6,
      lux: 940,
      uvIndex: 7.4,
      gasResistanceKOhm: 240,
    },
  },
  {
    id: 'night-fog',
    label: 'Nighttime Fog',
    description: '8.2 °C, 91% RH, 6 lux (triggers PA11 button LED ring)',
    mode: 'outdoor',
    values: {
      temperatureC: 8.2,
      humidityPct: 91.0,
      pressureHpa: 998.4,
      lux: 6,
      uvIndex: 0.0,
      gasResistanceKOhm: 195,
    },
  },
  {
    id: 'high-voc',
    label: 'VOC Spike Event',
    description: 'Indoor elevated VOCs (42 kOhm gas resistance, IAQ > 200)',
    mode: 'indoor',
    values: {
      temperatureC: 24.5,
      humidityPct: 58.0,
      pressureHpa: 1009.5,
      lux: 240,
      uvIndex: 0.1,
      gasResistanceKOhm: 42,
    },
  },
];

// Magnus dew point formula from Core/Src/bme680.c lines 320-328
export function calcDewPoint(tempC: number, humPct: number): number {
  if (humPct <= 0) return tempC;
  const alpha = (17.27 * tempC) / (237.7 + tempC) + Math.log(humPct / 100.0);
  return (237.7 * alpha) / (17.27 - alpha);
}

// Hypsometric equation for sea-level pressure (QNH) from Core/Src/bme680.c lines 382-387
export function calcSeaLevelPressure(
  stationPressHpa: number,
  altitudeM: number,
  tempC: number
): number {
  const tempK = tempC + 273.15;
  return (
    stationPressHpa *
    Math.pow(1.0 - (0.0065 * altitudeM) / (tempK + 0.0065 * altitudeM), -5.257)
  );
}

// IAQ Index Estimation (0-500 scale) from Core/Src/bme680.c lines 330-369
export function computeIaq(
  gasResistanceOhm: number,
  humidityPct: number,
  prevBaselineOhm: number,
  gasEnabled: boolean
): {
  gasValid: boolean;
  iaqScore: number;
  iaqText: IaqCategory;
  nextBaselineOhm: number;
} {
  if (!gasEnabled || gasResistanceOhm <= 0) {
    return {
      gasValid: false,
      iaqScore: 0,
      iaqText: 'N/A',
      nextBaselineOhm: prevBaselineOhm,
    };
  }

  let nextBaseline = prevBaselineOhm;
  if (nextBaseline <= 0) {
    nextBaseline = gasResistanceOhm;
  } else if (gasResistanceOhm > nextBaseline) {
    nextBaseline = nextBaseline * 0.9 + gasResistanceOhm * 0.1;
  }

  let gasRatio = gasResistanceOhm / nextBaseline;
  if (gasRatio > 1.0) gasRatio = 1.0;
  if (gasRatio < 0.0) gasRatio = 0.0;

  let humRatio =
    humidityPct >= 40.0
      ? 1.0 - (humidityPct - 40.0) / 60.0
      : 1.0 - (40.0 - humidityPct) / 40.0;
  if (humRatio > 1.0) humRatio = 1.0;
  if (humRatio < 0.0) humRatio = 0.0;

  const cleanFraction = 0.75 * gasRatio + 0.25 * humRatio;
  const iaqScore = (1.0 - cleanFraction) * 500.0;

  let iaqText: IaqCategory;
  if (iaqScore <= 50.0) iaqText = 'Good';
  else if (iaqScore <= 100.0) iaqText = 'Moderate';
  else if (iaqScore <= 150.0) iaqText = 'Sensitive';
  else if (iaqScore <= 200.0) iaqText = 'Unhealthy';
  else if (iaqScore <= 300.0) iaqText = 'Very Unhealthy';
  else iaqText = 'Hazardous';

  return {
    gasValid: true,
    iaqScore,
    iaqText,
    nextBaselineOhm: nextBaseline,
  };
}

// LTR390 dual-mode ALS/UVS conversion from Core/Src/ltr390.c lines 114-173
export function computeLtr390(inputs: EnvironmentInputs): LTR390Data {
  const resIdx = inputs.resolutionIdx > 4 ? 2 : inputs.resolutionIdx;
  const maxCount = MAX_ADC_COUNTS[resIdx];

  const alsGf = GAIN_FACTORS[inputs.alsGainIdx] ?? 3.0;
  const uvsGf = GAIN_FACTORS[inputs.uvsGainIdx] ?? 18.0;
  const tf = INTEG_FACTORS[resIdx] ?? 1.0;

  const alsRawExact = Math.round((inputs.lux * alsGf * tf) / 0.6);
  const alsRaw = Math.min(maxCount, Math.max(0, alsRawExact));
  const saturated = alsRaw >= maxCount - 10;
  const lux = (0.6 * alsRaw) / (alsGf * tf);

  const sens = 2300.0 * (uvsGf / 18.0) * (tf / 4.0);
  const uvsRawExact = Math.round(inputs.uvIndex * sens);
  const uvsRaw = Math.min(maxCount, Math.max(0, uvsRawExact));
  const uvIndex = sens > 0 ? uvsRaw / sens : 0;

  return {
    alsRaw,
    uvsRaw,
    lux,
    uvIndex,
    saturated,
  };
}

// TIM2_CH1 PWM auto-dimming from Core/Src/main.c lines 322-325
export function computePwmDuty(lux: number): number {
  let pwm = Math.floor(lux * 1.5);
  if (pwm < 150) pwm = 150;
  if (pwm > 999) pwm = 999;
  return pwm;
}

export function isButtonLedOn(lux: number): boolean {
  return lux < APP_CONFIG.BTN_LED_LUX_THRESH;
}

export function formatCsvRow(row: CsvLogRow): string {
  return [
    row.timeSec.toString(),
    row.tempC.toFixed(2),
    row.presHpa.toFixed(2),
    row.humPct.toFixed(1),
    row.dewPointC.toFixed(2),
    row.gasKOhm.toFixed(1),
    row.iaqScore.toFixed(0),
    row.iaqStatus,
    row.lux.toFixed(1),
    row.uvIndex.toFixed(2),
  ].join(',');
}

export function buildBootSerialLogs(
  mode: StationMode,
  enableSdLogging: boolean
): string[] {
  const base = [
    '========================================',
    `  Weather Station v${FW_VERSION}  [${mode === 'outdoor' ? 'OUTDOOR' : 'INDOOR'}]`,
    '  STM32G431KB @ 170 MHz',
    '========================================',
    'I2C2 bus scan (0x01 - 0x7F):',
    '  [0x53] ACK <- LTR390',
    '  [0x77] ACK <- BME680',
    '  2 device(s) found.',
    'BME680: OK (chip ID 0x61)',
    'LTR390: OK (part ID 0xB2)',
    'ILI9341: OK (320 x 240)',
  ];

  if (enableSdLogging) {
    return [
      ...base,
      'MicroSD: OK (Type 0x0C SDHC, Capacity: 15288 MB)',
      `SD_Logger: Created new ${APP_CONFIG.SD_LOG_FILENAME} with headers`,
    ];
  }

  return [...base, `MicroSD: Disabled (v${FW_VERSION})`];
}

export function formatTelemetrySerialLines(sample: TelemetrySample): string[] {
  const { bme, ltr, pwmDuty } = sample;
  let line1 = `T: ${bme.temperature.toFixed(1)} C  |  P: ${bme.pressure.toFixed(
    1
  )} hPa  |  H: ${bme.humidity.toFixed(1)} %  |  Dew: ${bme.dewPoint.toFixed(1)} C`;
  if (bme.gasValid) {
    line1 += `  |  IAQ: ${bme.iaqScore.toFixed(0)} (${bme.iaqText})  |  Gas: ${(
      bme.gasResistanceOhm / 1000.0
    ).toFixed(1)} kOhm`;
  }
  const line2 = `Lux: ${ltr.lux.toFixed(1)}  |  UV: ${ltr.uvIndex.toFixed(
    2
  )}  |  PWM: ${pwmDuty}`;
  return [line1, line2];
}
