import React from 'react';
import { Terminal, Trash2 } from 'lucide-react';
import { SerialLogEntry } from '../firmware/sensorEngine';

interface UsartConsoleProps {
  logs: SerialLogEntry[];
  onClear: () => void;
}

export const UsartConsole: React.FC<UsartConsoleProps> = ({ logs, onClear }) => {
  return (
    <div className="bg-[#111827] border border-slate-800 rounded-md p-4 flex flex-col gap-3">
      <div className="flex items-center justify-between gap-3">
        <div>
          <h3 className="text-sm font-semibold text-slate-100 flex items-center gap-2">
            <Terminal className="w-4 h-4 text-cyan-400" />
            USART2 Virtual COM Console
          </h3>
          <p className="text-xs text-slate-400 font-mono mt-0.5">
            PA2 (TX) / PA3 (RX) · 115200 baud · 8N1
          </p>
        </div>

        <button
          type="button"
          onClick={onClear}
          className="inline-flex items-center gap-1.5 px-3 py-1.5 text-xs font-medium rounded border border-slate-700 bg-slate-900 text-slate-300 hover:border-slate-500 hover:text-slate-100 transition-colors whitespace-nowrap shrink-0"
        >
          <Trash2 className="w-3.5 h-3.5" />
          Clear Output
        </button>
      </div>

      <div className="bg-[#07090E] border border-slate-800 rounded p-3 font-mono text-xs leading-relaxed max-h-[280px] overflow-y-auto space-y-0.5">
        {logs.length === 0 ? (
          <div className="text-slate-500 py-4 text-center">
            Serial buffer cleared. Waiting for next SENSOR_POLL_MS tick...
          </div>
        ) : (
          logs.map((entry) => {
            const colorClass =
              entry.level === 'sd'
                ? 'text-emerald-400'
                : entry.level === 'warn'
                ? 'text-amber-400'
                : entry.level === 'info'
                ? 'text-cyan-300'
                : 'text-slate-300';
            return (
              <div key={entry.id} className="flex gap-3">
                <span className="text-slate-600 select-none shrink-0 tabular-nums">
                  [{entry.timestamp}]
                </span>
                <span className={`${colorClass} whitespace-pre-wrap break-all`}>
                  {entry.text}
                </span>
              </div>
            );
          })
        )}
      </div>
    </div>
  );
};
