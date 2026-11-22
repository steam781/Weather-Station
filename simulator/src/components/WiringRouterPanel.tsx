import React, { useMemo, useState } from 'react';
import {
  AlertTriangle,
  CheckCircle2,
  ChevronLeft,
  ChevronRight,
  Cpu,
  ListChecks,
  ShieldAlert,
  Sparkles,
} from 'lucide-react';

type ModuleGroup = 'v1' | 'tft' | 'i2c' | 'sd' | 'buttons';

interface WireConnection {
  id: string;
  group: Exclude<ModuleGroup, 'v1'>;
  v1Active: boolean;
  moduleName: string;
  modulePin: string;
  header: 'CN4' | 'CN3';
  pinNumber: number; // 1..15 from top (USB end)
  stm32Pin: string;
  arduinoLabel: string;
  mxFunction: string;
  wireColorName: string;
  wireHex: string;
  note: string;
}

interface HeaderPin {
  header: 'CN4' | 'CN3';
  pin: number;
  label: string;
  sub: string;
  type: 'power' | 'gnd' | 'signal' | 'reserved';
}

const CN4_PINS: HeaderPin[] = [
  { header: 'CN4', pin: 1, label: 'PA9', sub: 'D1 / I2C2_SCL', type: 'signal' },
  { header: 'CN4', pin: 2, label: 'PA10', sub: 'D0 / UART1_RX', type: 'reserved' },
  { header: 'CN4', pin: 3, label: 'NRST', sub: 'RESET', type: 'reserved' },
  { header: 'CN4', pin: 4, label: 'GND', sub: 'Ground', type: 'gnd' },
  { header: 'CN4', pin: 5, label: 'PA12', sub: 'D2 / BTN_BACK', type: 'signal' },
  { header: 'CN4', pin: 6, label: 'PB0', sub: 'D3 / TFT_CS1', type: 'signal' },
  { header: 'CN4', pin: 7, label: 'PB7', sub: 'D4 (SB3!)', type: 'reserved' },
  { header: 'CN4', pin: 8, label: 'PA15', sub: 'D5 (SB2!)', type: 'reserved' },
  { header: 'CN4', pin: 9, label: 'PB6', sub: 'D6 / TFT_DC', type: 'signal' },
  { header: 'CN4', pin: 10, label: 'PF0', sub: 'D7 / BTN_OK', type: 'signal' },
  { header: 'CN4', pin: 11, label: 'PF1', sub: 'D8 / ESP_EN', type: 'reserved' },
  { header: 'CN4', pin: 12, label: 'PA8', sub: 'D9 / I2C2_SDA', type: 'signal' },
  { header: 'CN4', pin: 13, label: 'PA11', sub: 'D10 / LED_EN', type: 'signal' },
  { header: 'CN4', pin: 14, label: 'PB5', sub: 'D11 / TFT_RST', type: 'signal' },
  { header: 'CN4', pin: 15, label: 'PB4', sub: 'D12 / BTN_DOWN', type: 'signal' },
];

const CN3_PINS: HeaderPin[] = [
  { header: 'CN3', pin: 1, label: 'VIN', sub: '7-12V In', type: 'reserved' },
  { header: 'CN3', pin: 2, label: 'GND', sub: 'Ground', type: 'gnd' },
  { header: 'CN3', pin: 3, label: 'NRST', sub: 'RESET', type: 'reserved' },
  { header: 'CN3', pin: 4, label: '+5V', sub: '5V (JP1)', type: 'reserved' },
  { header: 'CN3', pin: 5, label: 'PA2', sub: 'A7 / VCP_TX', type: 'reserved' },
  { header: 'CN3', pin: 6, label: 'PA7', sub: 'A6 / SPI1_MOSI', type: 'signal' },
  { header: 'CN3', pin: 7, label: 'PA6', sub: 'A5 / SPI1_MISO', type: 'signal' },
  { header: 'CN3', pin: 8, label: 'PA5', sub: 'A4 / SPI1_SCK', type: 'signal' },
  { header: 'CN3', pin: 9, label: 'PA4', sub: 'A3 / SD_CS', type: 'signal' },
  { header: 'CN3', pin: 10, label: 'PA3', sub: 'A2 / VCP_RX', type: 'reserved' },
  { header: 'CN3', pin: 11, label: 'PA1', sub: 'A1 / BTN_UP', type: 'signal' },
  { header: 'CN3', pin: 12, label: 'PA0', sub: 'A0 / TFT_BLK', type: 'signal' },
  { header: 'CN3', pin: 13, label: 'AREF', sub: 'Analog Ref', type: 'reserved' },
  { header: 'CN3', pin: 14, label: '+3V3', sub: '3.3V Rail Out', type: 'power' },
  { header: 'CN3', pin: 15, label: 'PB8', sub: 'D13 / LD2 LED', type: 'reserved' },
];

const CONNECTIONS: WireConnection[] = [
  {
    id: 'rail-3v3',
    group: 'i2c',
    v1Active: true,
    moduleName: 'Breadboard + Rail',
    modulePin: '+3.3V Bus',
    header: 'CN3',
    pinNumber: 14,
    stm32Pin: '+3V3',
    arduinoLabel: '2nd from bottom-right',
    mxFunction: '3.3V Regulated Supply',
    wireColorName: 'Red',
    wireHex: '#ef4444',
    note: 'Powers TFT VCC & CS2, BME680, LTR390, and MicroSD VCC. Keep all sensors strictly on 3.3V.',
  },
  {
    id: 'rail-gnd',
    group: 'i2c',
    v1Active: true,
    moduleName: 'Breadboard - Rail',
    modulePin: 'GND Bus',
    header: 'CN4',
    pinNumber: 4,
    stm32Pin: 'GND',
    arduinoLabel: '4th from top-left',
    mxFunction: 'Common Ground',
    wireColorName: 'Black',
    wireHex: '#64748b',
    note: 'Common ground shared across Nucleo, TFT display, Qwiic sensors, MicroSD, and tactile buttons.',
  },
  {
    id: 'i2c-sda',
    group: 'i2c',
    v1Active: true,
    moduleName: 'BME680 + LTR390 (Qwiic)',
    modulePin: 'SDI / SDA',
    header: 'CN4',
    pinNumber: 12,
    stm32Pin: 'PA8',
    arduinoLabel: 'D9 (4th from bottom-left)',
    mxFunction: 'I2C2_SDA (Fast Mode 400 kHz)',
    wireColorName: 'Blue',
    wireHex: '#3b82f6',
    note: 'Routed to I2C2 on PA8 to avoid the NUCLEO-G431KB factory SB3 solder bridge on PB7.',
  },
  {
    id: 'i2c-scl',
    group: 'i2c',
    v1Active: true,
    moduleName: 'BME680 + LTR390 (Qwiic)',
    modulePin: 'SCK / SCL',
    header: 'CN4',
    pinNumber: 1,
    stm32Pin: 'PA9',
    arduinoLabel: 'D1 (Top-left pin)',
    mxFunction: 'I2C2_SCL (Fast Mode 400 kHz)',
    wireColorName: 'Yellow',
    wireHex: '#eab308',
    note: 'Routed to I2C2 on PA9 to avoid the NUCLEO-G431KB factory SB2 solder bridge on PA15.',
  },
  {
    id: 'tft-vcc',
    group: 'tft',
    v1Active: true,
    moduleName: 'ILI9341 2.8" TFT',
    modulePin: 'VCC & CS2',
    header: 'CN3',
    pinNumber: 14,
    stm32Pin: '+3V3',
    arduinoLabel: 'Breadboard +3V3 Rail',
    mxFunction: '3.3V Power + Touch CS High',
    wireColorName: 'Red',
    wireHex: '#ef4444',
    note: 'Tie CS2 to +3V3 so the XPT2046 touch controller stays deselected on the shared SPI1 bus.',
  },
  {
    id: 'tft-sck',
    group: 'tft',
    v1Active: true,
    moduleName: 'ILI9341 2.8" TFT',
    modulePin: 'CLK (SCK)',
    header: 'CN3',
    pinNumber: 8,
    stm32Pin: 'PA5',
    arduinoLabel: 'A4',
    mxFunction: 'SPI1_SCK (10.625 Mbit/s)',
    wireColorName: 'Orange',
    wireHex: '#f97316',
    note: 'Prescaler 16 (10.625 Mbit/s) eliminates signal ringing across breadboard jumper wires.',
  },
  {
    id: 'tft-miso',
    group: 'tft',
    v1Active: true,
    moduleName: 'ILI9341 2.8" TFT',
    modulePin: 'MISO (SDO)',
    header: 'CN3',
    pinNumber: 7,
    stm32Pin: 'PA6',
    arduinoLabel: 'A5',
    mxFunction: 'SPI1_MISO',
    wireColorName: 'Purple',
    wireHex: '#a855f7',
    note: 'Shared SPI1 MISO bus line (also shared with MicroSD MISO in v1.1).',
  },
  {
    id: 'tft-mosi',
    group: 'tft',
    v1Active: true,
    moduleName: 'ILI9341 2.8" TFT',
    modulePin: 'MOSI (SDI)',
    header: 'CN3',
    pinNumber: 6,
    stm32Pin: 'PA7',
    arduinoLabel: 'A6',
    mxFunction: 'SPI1_MOSI',
    wireColorName: 'Green',
    wireHex: '#22c55e',
    note: 'Shared SPI1 MOSI bus line carrying pixel and command bytes to ILI9341.',
  },
  {
    id: 'tft-cs',
    group: 'tft',
    v1Active: true,
    moduleName: 'ILI9341 2.8" TFT',
    modulePin: 'CS1 (TFT_CS)',
    header: 'CN4',
    pinNumber: 6,
    stm32Pin: 'PB0',
    arduinoLabel: 'D3',
    mxFunction: 'GPIO_Output (Active Low)',
    wireColorName: 'Cyan',
    wireHex: '#06b6d4',
    note: 'Display chip select. Idles HIGH; pulled LOW during ILI9341 SPI transfers.',
  },
  {
    id: 'tft-dc',
    group: 'tft',
    v1Active: true,
    moduleName: 'ILI9341 2.8" TFT',
    modulePin: 'DC (RS)',
    header: 'CN4',
    pinNumber: 9,
    stm32Pin: 'PB6',
    arduinoLabel: 'D6',
    mxFunction: 'GPIO_Output (TFT_DC)',
    wireColorName: 'Teal',
    wireHex: '#14b8a6',
    note: 'LOW = Command register write, HIGH = Display RAM pixel data write.',
  },
  {
    id: 'tft-rst',
    group: 'tft',
    v1Active: true,
    moduleName: 'ILI9341 2.8" TFT',
    modulePin: 'RES (RST)',
    header: 'CN4',
    pinNumber: 14,
    stm32Pin: 'PB5',
    arduinoLabel: 'D11',
    mxFunction: 'GPIO_Output (TFT_RST)',
    wireColorName: 'Pink',
    wireHex: '#ec4899',
    note: 'Hardware reset pulsed LOW during ILI9341_Init() before register configuration.',
  },
  {
    id: 'tft-blk',
    group: 'tft',
    v1Active: true,
    moduleName: 'ILI9341 2.8" TFT',
    modulePin: 'BLK (LED)',
    header: 'CN3',
    pinNumber: 12,
    stm32Pin: 'PA0',
    arduinoLabel: 'A0',
    mxFunction: 'TIM2_CH1 PWM (1 kHz)',
    wireColorName: 'White',
    wireHex: '#f8fafc',
    note: 'Auto-dims screen backlight (duty 150..999) from live LTR390 Lux readings.',
  },
  {
    id: 'sd-cs',
    group: 'sd',
    v1Active: false,
    moduleName: 'MicroSD Reader (v1.1)',
    modulePin: 'CS (SD_CS)',
    header: 'CN3',
    pinNumber: 9,
    stm32Pin: 'PA4',
    arduinoLabel: 'A3',
    mxFunction: 'GPIO_Output (SD_CS)',
    wireColorName: 'Amber',
    wireHex: '#f59e0b',
    note: 'Shares PA5/PA6/PA7 SPI1 bus with TFT. Set ENABLE_SD_LOGGING 1 in app_config.h when wired.',
  },
  {
    id: 'btn-up',
    group: 'buttons',
    v1Active: false,
    moduleName: 'Tactile Button UP (v1.1)',
    modulePin: 'Switch → GND',
    header: 'CN3',
    pinNumber: 11,
    stm32Pin: 'PA1',
    arduinoLabel: 'A1',
    mxFunction: 'EXTI1 Falling + Pull-Up',
    wireColorName: 'Sky',
    wireHex: '#38bdf8',
    note: 'Internal pull-up enabled. Wire tactile switch between PA1 and GND (no external resistor needed).',
  },
  {
    id: 'btn-down',
    group: 'buttons',
    v1Active: false,
    moduleName: 'Tactile Button DOWN (v1.1)',
    modulePin: 'Switch → GND',
    header: 'CN4',
    pinNumber: 15,
    stm32Pin: 'PB4',
    arduinoLabel: 'D12 (Bottom-left pin)',
    mxFunction: 'EXTI4 Falling + Pull-Up',
    wireColorName: 'Indigo',
    wireHex: '#818cf8',
    note: 'Internal pull-up enabled. Wire tactile switch between PB4 and GND.',
  },
  {
    id: 'btn-ok',
    group: 'buttons',
    v1Active: false,
    moduleName: 'Tactile Button OK (v1.1)',
    modulePin: 'Switch → GND',
    header: 'CN4',
    pinNumber: 10,
    stm32Pin: 'PF0',
    arduinoLabel: 'D7',
    mxFunction: 'EXTI0 Falling + Pull-Up',
    wireColorName: 'Emerald',
    wireHex: '#34d399',
    note: 'Internal pull-up enabled. Wire tactile switch between PF0 and GND.',
  },
  {
    id: 'btn-back',
    group: 'buttons',
    v1Active: false,
    moduleName: 'Tactile Button BACK (v1.1)',
    modulePin: 'Switch → GND',
    header: 'CN4',
    pinNumber: 5,
    stm32Pin: 'PA12',
    arduinoLabel: 'D2',
    mxFunction: 'EXTI12 Falling + Pull-Up',
    wireColorName: 'Rose',
    wireHex: '#fb7185',
    note: 'Internal pull-up enabled. Wire tactile switch between PA12 and GND.',
  },
  {
    id: 'btn-led',
    group: 'buttons',
    v1Active: false,
    moduleName: 'Night Button LED (v1.1)',
    modulePin: 'Anode via 220Ω',
    header: 'CN4',
    pinNumber: 13,
    stm32Pin: 'PA11',
    arduinoLabel: 'D10',
    mxFunction: 'GPIO_Output (BTN_LED_EN)',
    wireColorName: 'Lime',
    wireHex: '#a3e635',
    note: 'Drives HIGH (3.3V) when ambient light < 20 Lux. Connect via a 220Ω-330Ω resistor to GND.',
  },
];

export const WiringRouterPanel: React.FC = () => {
  const [filter, setFilter] = useState<ModuleGroup>('v1');
  const [selectedId, setSelectedId] = useState<string>('i2c-sda');
  const [guidedMode, setGuidedMode] = useState<boolean>(false);
  const [completedIds, setCompletedIds] = useState<Record<string, boolean>>({});

  const visibleConnections = useMemo(() => {
    if (filter === 'v1') {
      return CONNECTIONS.filter((c) => c.v1Active);
    }
    return CONNECTIONS.filter((c) => c.group === filter);
  }, [filter]);

  const activeWire = useMemo(() => {
    return (
      visibleConnections.find((c) => c.id === selectedId) ||
      visibleConnections[0] ||
      CONNECTIONS[0]
    );
  }, [visibleConnections, selectedId]);

  const activeIndex = useMemo(() => {
    const idx = visibleConnections.findIndex((c) => c.id === activeWire.id);
    return idx >= 0 ? idx : 0;
  }, [visibleConnections, activeWire]);

  const completedCount = useMemo(
    () => visibleConnections.filter((c) => completedIds[c.id]).length,
    [visibleConnections, completedIds]
  );

  const toggleCompleted = (id: string) => {
    setCompletedIds((prev) => ({ ...prev, [id]: !prev[id] }));
  };

  const stepWire = (delta: number) => {
    if (visibleConnections.length === 0) return;
    const nextIdx =
      (activeIndex + delta + visibleConnections.length) % visibleConnections.length;
    setSelectedId(visibleConnections[nextIdx].id);
  };

  /* SVG layout constants for interactive Nucleo-32 board */
  const boardX = 245;
  const boardY = 36;
  const boardW = 190;
  const boardH = 388;
  const pinStartY = 82;
  const pinPitchY = 22;

  const getPinCoords = (header: 'CN4' | 'CN3', pinNum: number) => {
    const x = header === 'CN4' ? boardX + 14 : boardX + boardW - 14;
    const y = pinStartY + (pinNum - 1) * pinPitchY;
    return { x, y };
  };

  return (
    <div className="bg-[#111827] text-slate-100 border border-slate-800 rounded-md p-5 flex flex-col gap-4">
      {/* Header & Mode Controls */}
      <div className="flex flex-col lg:flex-row lg:items-center lg:justify-between gap-4 pb-4 border-b border-slate-800">
        <div>
          <div className="flex items-center gap-2">
            <Cpu className="w-4 h-4 text-cyan-400" />
            <h2 className="text-base font-semibold tracking-tight text-slate-100">
              Interactive Hardware Routing Canvas — NUCLEO-G431KB
            </h2>
          </div>
          <p className="text-xs text-slate-400 mt-1">
            Seat the Nucleo with the <strong className="text-slate-200">Micro-USB port at the TOP</strong>{' '}
            and the <strong className="text-slate-200">STM32 chip facing UP</strong>. Click any jumper
            wire or table row to trace its physical header pin.
          </p>
        </div>

        <div className="flex flex-wrap items-center gap-2">
          <button
            type="button"
            onClick={() => setGuidedMode((g) => !g)}
            className={`inline-flex items-center gap-1.5 px-3 py-1.5 rounded text-xs font-medium border transition-colors ${
              guidedMode
                ? 'bg-emerald-500/15 border-emerald-400 text-emerald-300'
                : 'bg-[#07090E] border-slate-700 text-slate-300 hover:border-cyan-500/60'
            }`}
          >
            <ListChecks className="w-3.5 h-3.5" />
            {guidedMode ? 'Guided Step-by-Step: ON' : 'Guided Step-by-Step: OFF'}
          </button>
        </div>
      </div>

      {/* Hardware Safety Banner */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-3 text-xs">
        <div className="bg-[#07090E] border border-amber-500/30 rounded p-3 flex items-start gap-2.5">
          <AlertTriangle className="w-4 h-4 text-amber-400 shrink-0 mt-0.5" />
          <div>
            <span className="font-semibold text-amber-300">Why I2C2 (PA9 / PA8) Is Used:</span>{' '}
            <span className="text-slate-400">
              Factory solder bridges <code className="text-slate-200 font-mono">SB2</code> &amp;{' '}
              <code className="text-slate-200 font-mono">SB3</code> tie{' '}
              <code className="text-slate-200 font-mono">SPI1</code> (`PA5`/`PA6`) to{' '}
              <code className="text-slate-200 font-mono">I2C1</code> (`PB7`/`PA15`). Using{' '}
              <code className="text-cyan-300 font-mono">I2C2</code> avoids bus collisions with zero
              soldering.
            </span>
          </div>
        </div>
        <div className="bg-[#07090E] border border-cyan-500/30 rounded p-3 flex items-start gap-2.5">
          <ShieldAlert className="w-4 h-4 text-cyan-400 shrink-0 mt-0.5" />
          <div>
            <span className="font-semibold text-cyan-300">3.3V Logic &amp; Power Rule:</span>{' '}
            <span className="text-slate-400">
              Power TFT <code className="text-slate-200 font-mono">VCC</code>,{' '}
              <code className="text-slate-200 font-mono">CS2</code>, BME680, and LTR390 strictly from{' '}
              <code className="text-emerald-300 font-mono">+3V3 (CN3 Pin 14)</code>. Always unplug USB
              before moving wires.
            </span>
          </div>
        </div>
      </div>

      {/* Filter Tabs */}
      <div className="flex flex-wrap gap-2">
        {(
          [
            { id: 'v1', label: 'v1.0.0 Active (12 Wires)' },
            { id: 'tft', label: 'ILI9341 TFT (SPI1)' },
            { id: 'i2c', label: 'BME680 + LTR390 (I2C2)' },
            { id: 'sd', label: 'MicroSD (v1.1)' },
            { id: 'buttons', label: 'Buttons & LED (v1.1)' },
          ] as { id: ModuleGroup; label: string }[]
        ).map((tab) => (
          <button
            key={tab.id}
            type="button"
            onClick={() => {
              setFilter(tab.id);
              const nextList =
                tab.id === 'v1'
                  ? CONNECTIONS.filter((c) => c.v1Active)
                  : CONNECTIONS.filter((c) => c.group === tab.id);
              if (nextList.length > 0) setSelectedId(nextList[0].id);
            }}
            className={`px-3 py-1.5 rounded text-xs font-medium transition-colors border ${
              filter === tab.id
                ? 'bg-slate-800 border-cyan-400 text-cyan-300'
                : 'bg-[#07090E] border-slate-800 text-slate-400 hover:text-slate-200'
            }`}
          >
            {tab.label}
          </button>
        ))}
      </div>

      {/* Guided Step-by-Step Banner */}
      {guidedMode && (
        <div className="p-3.5 rounded bg-[#07090E] border border-emerald-500/40 flex flex-col sm:flex-row sm:items-center justify-between gap-3">
          <div>
            <div className="text-xs font-semibold text-emerald-300 flex items-center gap-1.5">
              <Sparkles className="w-3.5 h-3.5" />
              Step {activeIndex + 1} of {visibleConnections.length} — {completedCount}/
              {visibleConnections.length} Checked Off
            </div>
            <div className="text-xs text-slate-300 mt-0.5">
              Unplug USB, connect{' '}
              <span className="font-mono text-white">{activeWire.moduleName}</span> (
              <span className="font-mono text-cyan-300">{activeWire.modulePin}</span>) to{' '}
              <span className="font-mono text-amber-300">
                {activeWire.header} Pin {activeWire.pinNumber} ({activeWire.stm32Pin})
              </span>
              .
            </div>
          </div>
          <div className="flex items-center gap-2 shrink-0">
            <button
              type="button"
              onClick={() => stepWire(-1)}
              className="inline-flex items-center gap-1 px-2.5 py-1 rounded bg-slate-800 hover:bg-slate-700 text-xs text-slate-200"
            >
              <ChevronLeft className="w-3.5 h-3.5" />
              Prev
            </button>
            <button
              type="button"
              onClick={() => toggleCompleted(activeWire.id)}
              className={`inline-flex items-center gap-1 px-3 py-1 rounded text-xs font-medium ${
                completedIds[activeWire.id]
                  ? 'bg-emerald-600 text-white'
                  : 'bg-slate-800 border border-slate-600 text-slate-200 hover:bg-slate-700'
              }`}
            >
              <CheckCircle2 className="w-3.5 h-3.5" />
              {completedIds[activeWire.id] ? 'Connected' : 'Mark Connected'}
            </button>
            <button
              type="button"
              onClick={() => stepWire(1)}
              className="inline-flex items-center gap-1 px-2.5 py-1 rounded bg-cyan-500 hover:bg-cyan-400 text-xs font-semibold text-slate-950"
            >
              Next
              <ChevronRight className="w-3.5 h-3.5" />
            </button>
          </div>
        </div>
      )}

      {/* Main Split View: Interactive SVG Routing Canvas + Connection Inspector */}
      <div className="grid grid-cols-1 xl:grid-cols-12 gap-5 items-start">
        {/* Left: SVG Board & Wire Router */}
        <div className="xl:col-span-7 bg-[#07090E] border border-slate-800 rounded-md p-3 overflow-x-auto">
          <svg
            viewBox="0 0 680 450"
            className="w-full h-auto min-w-[560px] select-none"
            role="img"
            aria-label="NUCLEO-G431KB interactive wiring diagram"
          >
            {/* Left Peripheral Box (CN4 targets) */}
            <rect
              x="14"
              y="46"
              width="156"
              height="368"
              rx="8"
              fill="#0B0E17"
              stroke="#1e293b"
              strokeWidth="1.5"
            />
            <text x="92" y="68" textAnchor="middle" fill="#94a3b8" fontSize="10.5" fontWeight="700">
              LEFT HEADER (CN4)
            </text>
            <text x="92" y="83" textAnchor="middle" fill="#64748b" fontSize="9">
              I2C2 · TFT Ctrl · Buttons
            </text>

            {/* Right Peripheral Box (CN3 targets) */}
            <rect
              x="510"
              y="46"
              width="156"
              height="368"
              rx="8"
              fill="#0B0E17"
              stroke="#1e293b"
              strokeWidth="1.5"
            />
            <text x="588" y="68" textAnchor="middle" fill="#94a3b8" fontSize="10.5" fontWeight="700">
              RIGHT HEADER (CN3)
            </text>
            <text x="588" y="83" textAnchor="middle" fill="#64748b" fontSize="9">
              SPI1 · PWM · 3.3V Rail
            </text>

            {/* NUCLEO-G431KB PCB Body */}
            <rect
              x={boardX}
              y={boardY}
              width={boardW}
              height={boardH}
              rx="10"
              fill="#111827"
              stroke="#334155"
              strokeWidth="2"
            />

            {/* Micro-USB Port at Top */}
            <rect
              x={boardX + boardW / 2 - 26}
              y={boardY - 12}
              width="52"
              height="24"
              rx="4"
              fill="#475569"
              stroke="#94a3b8"
              strokeWidth="1.5"
            />
            <text
              x={boardX + boardW / 2}
              y={boardY + 4}
              textAnchor="middle"
              fill="#f8fafc"
              fontSize="9"
              fontWeight="700"
            >
              USB TOP
            </text>

            {/* Board Silkscreen & LQFP32 Chip */}
            <text
              x={boardX + boardW / 2}
              y={boardY + 32}
              textAnchor="middle"
              fill="#e2e8f0"
              fontSize="11"
              fontWeight="700"
            >
              NUCLEO-G431KB
            </text>
            <text
              x={boardX + boardW / 2}
              y={boardY + 45}
              textAnchor="middle"
              fill="#22d3ee"
              fontSize="8.5"
              fontWeight="600"
            >
              TOP VIEW (CHIP UP)
            </text>

            <rect
              x={boardX + boardW / 2 - 30}
              y={boardY + 150}
              width="60"
              height="60"
              rx="6"
              fill="#1e293b"
              stroke="#475569"
              strokeWidth="1.5"
            />
            <text
              x={boardX + boardW / 2}
              y={boardY + 178}
              textAnchor="middle"
              fill="#cbd5e1"
              fontSize="9"
              fontWeight="700"
            >
              STM32
            </text>
            <text
              x={boardX + boardW / 2}
              y={boardY + 190}
              textAnchor="middle"
              fill="#94a3b8"
              fontSize="8"
            >
              G431KB
            </text>

            {/* Wires + Peripheral Endpoints */}
            {visibleConnections.map((conn) => {
              const isSelected = conn.id === activeWire.id;
              const isDimmed = guidedMode && !isSelected;
              const isLeft = conn.header === 'CN4';
              const { x: pinX, y: pinY } = getPinCoords(conn.header, conn.pinNumber);
              const modX = isLeft ? 162 : 518;
              const ctrlX1 = isLeft ? modX + 36 : modX - 36;
              const ctrlX2 = isLeft ? pinX - 36 : pinX + 36;
              const pathD = `M ${modX} ${pinY} C ${ctrlX1} ${pinY}, ${ctrlX2} ${pinY}, ${pinX} ${pinY}`;

              return (
                <g
                  key={conn.id}
                  onClick={() => setSelectedId(conn.id)}
                  className="cursor-pointer"
                  opacity={isDimmed ? 0.22 : 1}
                >
                  <path d={pathD} fill="none" stroke="transparent" strokeWidth="12" />
                  <path
                    d={pathD}
                    fill="none"
                    stroke={conn.wireHex}
                    strokeWidth={isSelected ? 4 : 2.2}
                    strokeLinecap="round"
                  />
                  <rect
                    x={isLeft ? 22 : 518}
                    y={pinY - 8.5}
                    width="140"
                    height="17"
                    rx="4"
                    fill={isSelected ? '#1e293b' : '#0B0E17'}
                    stroke={isSelected ? conn.wireHex : '#334155'}
                    strokeWidth={isSelected ? 1.5 : 1}
                  />
                  <text
                    x={isLeft ? 92 : 588}
                    y={pinY + 3}
                    textAnchor="middle"
                    fill={isSelected ? '#ffffff' : '#cbd5e1'}
                    fontSize="9"
                    fontWeight={isSelected ? '700' : '500'}
                  >
                    {conn.modulePin}
                  </text>
                </g>
              );
            })}

            {/* CN4 (Left) Header Pins */}
            {CN4_PINS.map((p) => {
              const { x, y } = getPinCoords('CN4', p.pin);
              const isSelected = activeWire.header === 'CN4' && activeWire.pinNumber === p.pin;
              const fill =
                p.type === 'gnd'
                  ? '#64748b'
                  : p.type === 'power'
                    ? '#ef4444'
                    : p.type === 'signal'
                      ? '#22d3ee'
                      : '#334155';
              return (
                <g key={`cn4-${p.pin}`}>
                  <circle
                    cx={x}
                    cy={y}
                    r={isSelected ? 6 : 4.2}
                    fill={fill}
                    stroke={isSelected ? '#ffffff' : '#0B0E17'}
                    strokeWidth="1.5"
                  />
                  <text x={x + 10} y={y + 3} fill="#e2e8f0" fontSize="9" fontWeight="600">
                    {p.pin}. {p.label}
                  </text>
                </g>
              );
            })}

            {/* CN3 (Right) Header Pins */}
            {CN3_PINS.map((p) => {
              const { x, y } = getPinCoords('CN3', p.pin);
              const isSelected = activeWire.header === 'CN3' && activeWire.pinNumber === p.pin;
              const fill =
                p.type === 'gnd'
                  ? '#64748b'
                  : p.type === 'power'
                    ? '#ef4444'
                    : p.type === 'signal'
                      ? '#22d3ee'
                      : '#334155';
              return (
                <g key={`cn3-${p.pin}`}>
                  <circle
                    cx={x}
                    cy={y}
                    r={isSelected ? 6 : 4.2}
                    fill={fill}
                    stroke={isSelected ? '#ffffff' : '#0B0E17'}
                    strokeWidth="1.5"
                  />
                  <text
                    x={x - 10}
                    y={y + 3}
                    textAnchor="end"
                    fill="#e2e8f0"
                    fontSize="9"
                    fontWeight="600"
                  >
                    {p.label} .{p.pin}
                  </text>
                </g>
              );
            })}
          </svg>
        </div>

        {/* Right: Selected Wire Inspector & Connection List */}
        <div className="xl:col-span-5 flex flex-col gap-4">
          {/* Inspector Card */}
          <div className="bg-[#07090E] border border-slate-700 rounded-md p-4">
            <div className="flex items-center justify-between gap-2">
              <span className="text-[11px] uppercase tracking-wider text-cyan-400 font-semibold">
                Selected Route
              </span>
              <span className="inline-flex items-center gap-1.5 text-xs text-slate-300 bg-slate-900 border border-slate-700 px-2.5 py-0.5 rounded-full">
                <span
                  className="w-2.5 h-2.5 rounded-full border border-white/40"
                  style={{ backgroundColor: activeWire.wireHex }}
                />
                {activeWire.wireColorName} Wire
              </span>
            </div>

            <div className="mt-3 grid grid-cols-2 gap-2.5 text-xs">
              <div className="bg-[#0B0E17] border border-slate-800 rounded p-2.5">
                <div className="text-slate-400 text-[10px] uppercase">Peripheral Endpoint</div>
                <div className="font-semibold text-white mt-0.5">{activeWire.moduleName}</div>
                <div className="font-mono text-cyan-300 mt-0.5">{activeWire.modulePin}</div>
              </div>
              <div className="bg-[#0B0E17] border border-slate-800 rounded p-2.5">
                <div className="text-slate-400 text-[10px] uppercase">Nucleo Header Target</div>
                <div className="font-semibold text-amber-300 mt-0.5">
                  {activeWire.header} Pin {activeWire.pinNumber} ({activeWire.stm32Pin})
                </div>
                <div className="font-mono text-slate-300 mt-0.5">{activeWire.arduinoLabel}</div>
              </div>
            </div>

            <div className="mt-2.5 text-xs text-slate-300 bg-[#0B0E17] border border-slate-800 rounded p-2.5">
              <div className="font-mono text-[11px] text-emerald-300 mb-1">
                CubeMX: {activeWire.mxFunction}
              </div>
              <p className="text-slate-400 leading-relaxed">{activeWire.note}</p>
            </div>
          </div>

          {/* Interactive Pinout Checklist Table */}
          <div className="bg-[#07090E] border border-slate-800 rounded-md overflow-hidden">
            <div className="px-3.5 py-2.5 border-b border-slate-800 flex items-center justify-between bg-[#0B0E17]">
              <span className="text-xs font-semibold text-slate-200">
                Pin-to-Pin Route Table ({visibleConnections.length})
              </span>
              <span className="text-[11px] text-slate-400">Click row to highlight</span>
            </div>
            <div className="max-h-[250px] overflow-y-auto divide-y divide-slate-800/70">
              {visibleConnections.map((conn) => {
                const isSelected = conn.id === activeWire.id;
                return (
                  <button
                    key={conn.id}
                    type="button"
                    onClick={() => setSelectedId(conn.id)}
                    className={`w-full text-left px-3.5 py-2 text-xs flex items-center justify-between gap-2 transition-colors ${
                      isSelected
                        ? 'bg-cyan-500/15 text-white'
                        : 'hover:bg-slate-900 text-slate-300'
                    }`}
                  >
                    <div className="flex items-center gap-2 min-w-0">
                      <span
                        className="w-2.5 h-2.5 rounded-full shrink-0"
                        style={{ backgroundColor: conn.wireHex }}
                      />
                      <span className="truncate font-medium">{conn.modulePin}</span>
                      <span className="text-slate-500 text-[11px] truncate">
                        ({conn.moduleName})
                      </span>
                    </div>
                    <span className="font-mono text-[11px] text-amber-300 shrink-0 tabular-nums">
                      {conn.header}#{conn.pinNumber} · {conn.stm32Pin}
                    </span>
                  </button>
                );
              })}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default WiringRouterPanel;
