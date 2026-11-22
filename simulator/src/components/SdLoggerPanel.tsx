import React from 'react';
import { Download, FileSpreadsheet, Plus, Trash2 } from 'lucide-react';
import {
  APP_CONFIG,
  CSV_HEADER,
  CsvLogRow,
  formatCsvRow,
} from '../firmware/sensorEngine';

interface SdLoggerPanelProps {
  rows: CsvLogRow[];
  enableSdLogging: boolean;
  nextLogInSec: number;
  onToggleSdLogging: (enabled: boolean) => void;
  onLogNow: () => void;
  onClear: () => void;
  onExportCsv: () => void;
}

export const SdLoggerPanel: React.FC<SdLoggerPanelProps> = ({
  rows,
  enableSdLogging,
  nextLogInSec,
  onToggleSdLogging,
  onLogNow,
  onClear,
  onExportCsv,
}) => {
  const estimatedBytes =
    CSV_HEADER.length +
    2 +
    rows.reduce((acc, r) => acc + formatCsvRow(r).length + 2, 0);

  return (
    <div className="bg-[#111827] border border-slate-800 rounded-md p-4 flex flex-col gap-3">
      <div className="flex flex-wrap items-center justify-between gap-3">
        <div>
          <h3 className="text-sm font-semibold text-slate-100 flex items-center gap-2">
            <FileSpreadsheet className="w-4 h-4 text-cyan-400" />
            MicroSD FatFS Logger ({APP_CONFIG.SD_LOG_FILENAME})
          </h3>
          <p className="text-xs text-slate-400 font-mono tabular-nums mt-0.5">
            {enableSdLogging
              ? `${rows.length} rows · ${estimatedBytes} bytes · Next auto-append in ${nextLogInSec}s`
              : 'ENABLE_SD_LOGGING = 0 · Disabled by default in v1.0.0'}
          </p>
        </div>

        <div className="flex flex-wrap items-center gap-2">
          <button
            type="button"
            onClick={() => onToggleSdLogging(!enableSdLogging)}
            className={`inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium rounded border transition-colors whitespace-nowrap shrink-0 ${
              enableSdLogging
                ? 'border-emerald-500/50 bg-emerald-950/30 text-emerald-300 hover:bg-emerald-950/50'
                : 'border-amber-500/50 bg-amber-950/30 text-amber-300 hover:bg-amber-950/50'
            }`}
          >
            {enableSdLogging ? 'Disable SD' : 'Enable SD'}
          </button>
          <button
            type="button"
            disabled={!enableSdLogging}
            onClick={onLogNow}
            className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium rounded border border-slate-700 bg-slate-900 text-slate-200 hover:border-cyan-500/60 hover:text-cyan-300 disabled:opacity-40 disabled:pointer-events-none transition-colors whitespace-nowrap shrink-0"
          >
            <Plus className="w-3.5 h-3.5" />
            Append Row
          </button>
          <button
            type="button"
            onClick={onClear}
            className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium rounded border border-slate-700 bg-slate-900 text-slate-300 hover:border-rose-500/60 hover:text-rose-300 transition-colors whitespace-nowrap shrink-0"
          >
            <Trash2 className="w-3.5 h-3.5" />
            Reset File
          </button>
          <button
            type="button"
            onClick={onExportCsv}
            className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium rounded bg-cyan-500 text-slate-950 hover:bg-cyan-400 transition-colors whitespace-nowrap shrink-0"
          >
            <Download className="w-3.5 h-3.5" />
            Download CSV
          </button>
        </div>
      </div>

      <div className="border border-slate-800 rounded bg-[#07090E] overflow-x-auto max-h-[280px] overflow-y-auto">
        <table className="w-full text-left border-collapse text-xs font-mono tabular-nums">
          <thead className="sticky top-0 bg-[#0B0E17] border-b border-slate-800 text-slate-400">
            <tr>
              <th className="py-2 px-2.5 font-medium">Time_s</th>
              <th className="py-2 px-2.5 font-medium">Temp_C</th>
              <th className="py-2 px-2.5 font-medium">Pres_hPa</th>
              <th className="py-2 px-2.5 font-medium">Hum_pct</th>
              <th className="py-2 px-2.5 font-medium">DewPoint_C</th>
              <th className="py-2 px-2.5 font-medium">Gas_kOhm</th>
              <th className="py-2 px-2.5 font-medium">IAQ_Score</th>
              <th className="py-2 px-2.5 font-medium">IAQ_Status</th>
              <th className="py-2 px-2.5 font-medium">Lux</th>
              <th className="py-2 px-2.5 font-medium">UV_Index</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-slate-800/70 text-slate-200">
            {rows.length === 0 ? (
              <tr>
                <td colSpan={10} className="py-6 text-center text-slate-500">
                  {enableSdLogging
                    ? 'No CSV rows logged yet. Waiting for SD_LOG_INTERVAL_MS (10s) or manual append.'
                    : 'MicroSD card initialization and periodic CSV logging are gated off (ENABLE_SD_LOGGING = 0).'}
                </td>
              </tr>
            ) : (
              [...rows].reverse().map((r, idx) => (
                <tr key={`${r.timeSec}-${idx}`} className="hover:bg-slate-900/60">
                  <td className="py-1.5 px-2.5 text-slate-400">{r.timeSec}</td>
                  <td className="py-1.5 px-2.5">{r.tempC.toFixed(2)}</td>
                  <td className="py-1.5 px-2.5">{r.presHpa.toFixed(2)}</td>
                  <td className="py-1.5 px-2.5">{r.humPct.toFixed(1)}</td>
                  <td className="py-1.5 px-2.5">{r.dewPointC.toFixed(2)}</td>
                  <td className="py-1.5 px-2.5">{r.gasKOhm.toFixed(1)}</td>
                  <td className="py-1.5 px-2.5">{r.iaqScore.toFixed(0)}</td>
                  <td className="py-1.5 px-2.5">{r.iaqStatus}</td>
                  <td className="py-1.5 px-2.5">{r.lux.toFixed(1)}</td>
                  <td className="py-1.5 px-2.5">{r.uvIndex.toFixed(2)}</td>
                </tr>
              ))
            )}
          </tbody>
        </table>
      </div>
    </div>
  );
};
