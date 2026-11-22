import React, { useMemo, useState } from 'react';
import { TelemetrySample } from '../firmware/sensorEngine';

interface TelemetryChartsProps {
  history: TelemetrySample[];
}

type ChannelGroup = 'thermal' | 'barometric' | 'optical' | 'iaq';

interface SeriesSpec {
  key: string;
  label: string;
  unit: string;
  color: string;
  dashed?: boolean;
  getValue: (s: TelemetrySample) => number;
}

const CHANNEL_GROUPS: Record<
  ChannelGroup,
  { title: string; primary: SeriesSpec; secondary: SeriesSpec }
> = {
  thermal: {
    title: 'Temperature & Dew Point',
    primary: {
      key: 'temp',
      label: 'Temperature',
      unit: '°C',
      color: '#06B6D4',
      getValue: (s) => s.bme.temperature,
    },
    secondary: {
      key: 'dew',
      label: 'Dew Point',
      unit: '°C',
      color: '#F59E0B',
      dashed: true,
      getValue: (s) => s.bme.dewPoint,
    },
  },
  barometric: {
    title: 'Station Pressure & Relative Humidity',
    primary: {
      key: 'pres',
      label: 'Pressure',
      unit: 'hPa',
      color: '#06B6D4',
      getValue: (s) => s.bme.pressure,
    },
    secondary: {
      key: 'hum',
      label: 'Humidity',
      unit: '%',
      color: '#38BDF8',
      dashed: true,
      getValue: (s) => s.bme.humidity,
    },
  },
  optical: {
    title: 'Ambient Light & UV Index',
    primary: {
      key: 'lux',
      label: 'Ambient Light',
      unit: 'Lux',
      color: '#F59E0B',
      getValue: (s) => s.ltr.lux,
    },
    secondary: {
      key: 'uv',
      label: 'UV Index',
      unit: 'UVI',
      color: '#F43F5E',
      dashed: true,
      getValue: (s) => s.ltr.uvIndex,
    },
  },
  iaq: {
    title: 'Indoor Air Quality (IAQ) & Gas Resistance',
    primary: {
      key: 'iaq',
      label: 'IAQ Score',
      unit: 'AQI',
      color: '#10B981',
      getValue: (s) => (s.bme.gasValid ? s.bme.iaqScore : 0),
    },
    secondary: {
      key: 'gas',
      label: 'Gas Resistance',
      unit: 'kΩ',
      color: '#06B6D4',
      dashed: true,
      getValue: (s) => (s.bme.gasValid ? s.bme.gasResistanceOhm / 1000 : 0),
    },
  },
};

export const TelemetryCharts: React.FC<TelemetryChartsProps> = ({ history }) => {
  const [group, setGroup] = useState<ChannelGroup>('thermal');
  const [hoverIndex, setHoverIndex] = useState<number | null>(null);

  const spec = CHANNEL_GROUPS[group];
  const samples = useMemo(() => history.slice(-40), [history]);

  const width = 760;
  const height = 220;
  const padL = 52;
  const padR = 52;
  const padT = 18;
  const padB = 28;
  const plotW = width - padL - padR;
  const plotH = height - padT - padB;

  const { pMin, pMax, sMin, sMax, primaryPath, secondaryPath, points } = useMemo(() => {
    if (samples.length === 0) {
      return {
        pMin: 0,
        pMax: 1,
        sMin: 0,
        sMax: 1,
        primaryPath: '',
        secondaryPath: '',
        points: [],
      };
    }

    const pVals = samples.map(spec.primary.getValue);
    const sVals = samples.map(spec.secondary.getValue);

    const calcBounds = (vals: number[]) => {
      const min = Math.min(...vals);
      const max = Math.max(...vals);
      const span = max - min;
      const margin = span < 0.5 ? 2 : span * 0.18;
      return [min - margin, max + margin] as const;
    };

    const [lo1, hi1] = calcBounds(pVals);
    const [lo2, hi2] =
      group === 'thermal' ? [lo1, hi1] : calcBounds(sVals);

    const mapX = (i: number) =>
      samples.length === 1
        ? padL + plotW / 2
        : padL + (i / (samples.length - 1)) * plotW;

    const mapY = (val: number, lo: number, hi: number) =>
      padT + plotH - ((val - lo) / (hi - lo || 1)) * plotH;

    const pts = samples.map((s, i) => {
      const pv = pVals[i];
      const sv = sVals[i];
      return {
        x: mapX(i),
        y1: mapY(pv, lo1, hi1),
        y2: mapY(sv, lo2, hi2),
        pv,
        sv,
        sample: s,
      };
    });

    const pPath = pts
      .map((pt, i) => `${i === 0 ? 'M' : 'L'} ${pt.x.toFixed(1)} ${pt.y1.toFixed(1)}`)
      .join(' ');
    const sPath = pts
      .map((pt, i) => `${i === 0 ? 'M' : 'L'} ${pt.x.toFixed(1)} ${pt.y2.toFixed(1)}`)
      .join(' ');

    return {
      pMin: lo1,
      pMax: hi1,
      sMin: lo2,
      sMax: hi2,
      primaryPath: pPath,
      secondaryPath: sPath,
      points: pts,
    };
  }, [samples, spec, group, plotW, plotH]);

  const activePt =
    hoverIndex !== null && points[hoverIndex]
      ? points[hoverIndex]
      : points[points.length - 1] ?? null;

  return (
    <div className="bg-[#111827] border border-slate-800 rounded-md p-4 flex flex-col gap-3">
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h3 className="text-sm font-semibold text-slate-100">{spec.title}</h3>
          <p className="text-xs text-slate-400">
            Dual-channel telemetry buffer ({samples.length} samples ·{' '}
            {spec.primary.label} solid, {spec.secondary.label} dashed)
          </p>
        </div>

        <div className="flex items-center gap-1 p-1 bg-[#07090E] border border-slate-800 rounded">
          {(
            [
              { id: 'thermal', label: 'Temp / Dew' },
              { id: 'barometric', label: 'Pres / Hum' },
              { id: 'optical', label: 'Lux / UV' },
              { id: 'iaq', label: 'IAQ / Gas' },
            ] as const
          ).map((tab) => (
            <button
              key={tab.id}
              type="button"
              onClick={() => setGroup(tab.id)}
              className={`px-2.5 py-1 text-xs font-medium rounded transition-colors whitespace-nowrap shrink-0 ${
                group === tab.id
                  ? 'bg-slate-800 text-cyan-300'
                  : 'text-slate-400 hover:text-slate-200'
              }`}
            >
              {tab.label}
            </button>
          ))}
        </div>
      </div>

      {/* Crosshair Readout HUD */}
      {activePt && (
        <div className="flex flex-wrap items-center justify-between gap-4 px-3 py-2 bg-[#07090E] border border-slate-800 rounded text-xs font-mono tabular-nums">
          <span className="text-slate-400">
            T+{activePt.sample.timestampSec}s · {activePt.sample.clockLabel}
          </span>
          <div className="flex items-center gap-5">
            <span style={{ color: spec.primary.color }}>
              {spec.primary.label}: {activePt.pv.toFixed(1)} {spec.primary.unit}
            </span>
            <span style={{ color: spec.secondary.color }}>
              {spec.secondary.label}: {activePt.sv.toFixed(1)} {spec.secondary.unit}
            </span>
          </div>
        </div>
      )}

      {/* SVG Canvas */}
      <div className="w-full overflow-hidden">
        <svg
          viewBox={`0 0 ${width} ${height}`}
          className="w-full h-[220px] cursor-crosshair select-none"
          onMouseLeave={() => setHoverIndex(null)}
          onMouseMove={(e) => {
            if (points.length === 0) return;
            const rect = e.currentTarget.getBoundingClientRect();
            const relX = ((e.clientX - rect.left) / rect.width) * width;
            let closest = 0;
            let bestDist = Infinity;
            for (let i = 0; i < points.length; i++) {
              const d = Math.abs(points[i].x - relX);
              if (d < bestDist) {
                bestDist = d;
                closest = i;
              }
            }
            setHoverIndex(closest);
          }}
        >
          {/* Horizontal grid lines */}
          {[0, 0.25, 0.5, 0.75, 1].map((t) => {
            const y = padT + t * plotH;
            const leftVal = pMax - t * (pMax - pMin);
            const rightVal = sMax - t * (sMax - sMin);
            return (
              <g key={t}>
                <line
                  x1={padL}
                  y1={y}
                  x2={width - padR}
                  y2={y}
                  stroke="#1E293B"
                  strokeWidth={1}
                />
                <text
                  x={padL - 6}
                  y={y + 3}
                  textAnchor="end"
                  fill="#94A3B8"
                  fontSize="10"
                  fontFamily="IBM Plex Mono, monospace"
                >
                  {leftVal.toFixed(1)}
                </text>
                {group !== 'thermal' && (
                  <text
                    x={width - padR + 6}
                    y={y + 3}
                    textAnchor="start"
                    fill="#94A3B8"
                    fontSize="10"
                    fontFamily="IBM Plex Mono, monospace"
                  >
                    {rightVal.toFixed(1)}
                  </text>
                )}
              </g>
            );
          })}

          {/* Primary & Secondary curves */}
          {primaryPath && (
            <path
              d={primaryPath}
              fill="none"
              stroke={spec.primary.color}
              strokeWidth={2}
            />
          )}
          {secondaryPath && (
            <path
              d={secondaryPath}
              fill="none"
              stroke={spec.secondary.color}
              strokeWidth={1.75}
              strokeDasharray="5 4"
            />
          )}

          {/* Hover hairline & point markers */}
          {activePt && (
            <g>
              <line
                x1={activePt.x}
                y1={padT}
                x2={activePt.x}
                y2={padT + plotH}
                stroke="#475569"
                strokeWidth={1}
                strokeDasharray="2 2"
              />
              <circle
                cx={activePt.x}
                cy={activePt.y1}
                r={3.5}
                fill={spec.primary.color}
              />
              <circle
                cx={activePt.x}
                cy={activePt.y2}
                r={3.5}
                fill={spec.secondary.color}
              />
            </g>
          )}

          {/* X-axis tick labels */}
          {points.length > 0 && (
            <>
              <text
                x={padL}
                y={height - 6}
                textAnchor="start"
                fill="#64748B"
                fontSize="10"
                fontFamily="IBM Plex Mono, monospace"
              >
                T+{points[0].sample.timestampSec}s
              </text>
              <text
                x={width - padR}
                y={height - 6}
                textAnchor="end"
                fill="#64748B"
                fontSize="10"
                fontFamily="IBM Plex Mono, monospace"
              >
                T+{points[points.length - 1].sample.timestampSec}s
              </text>
            </>
          )}
        </svg>
      </div>
    </div>
  );
};
