import { AppState } from '../types';

interface DSPEngineProps {
  state: AppState;
  onUpdate: (updates: Partial<AppState>) => void;
}

function BigSlider({
  label,
  sublabel,
  value,
  min,
  max,
  unit,
  color,
  icon,
  description,
  onChange,
}: {
  label: string;
  sublabel: string;
  value: number;
  min: number;
  max: number;
  unit: string;
  color: string;
  icon: string;
  description: string;
  onChange: (v: number) => void;
}) {
  const pct = ((value - min) / (max - min)) * 100;

  return (
    <div className="bg-slate-800/60 border border-slate-700/50 rounded-xl p-5">
      <div className="flex items-start justify-between mb-1">
        <div className="flex items-center gap-2">
          <span className="text-2xl">{icon}</span>
          <div>
            <div className="text-white font-semibold text-sm">{label}</div>
            <div className="text-slate-500 text-xs">{sublabel}</div>
          </div>
        </div>
        <div className="text-right">
          <div className="font-mono font-bold text-lg" style={{ color }}>
            {value.toLocaleString()}
            <span className="text-sm ml-1 font-normal text-slate-400">{unit}</span>
          </div>
        </div>
      </div>

      <p className="text-slate-400 text-xs mb-4 ml-9">{description}</p>

      <div className="relative">
        <div className="absolute inset-0 flex items-center">
          <div className="w-full h-3 rounded-full bg-slate-700/70 overflow-hidden">
            <div
              className="h-full rounded-full transition-all duration-100"
              style={{ width: `${pct}%`, backgroundColor: color, boxShadow: `0 0 10px ${color}60` }}
            />
          </div>
        </div>
        <input
          type="range"
          min={min}
          max={max}
          value={value}
          onChange={e => onChange(Number(e.target.value))}
          className="relative w-full h-3 opacity-0 cursor-pointer"
          style={{ zIndex: 10 }}
        />
      </div>

      <div className="flex justify-between mt-2 text-xs text-slate-500 font-mono">
        <span>{min} {unit}</span>
        <span>{max} {unit}</span>
      </div>
    </div>
  );
}

export default function DSPEngine({ state, onUpdate }: DSPEngineProps) {
  return (
    <div className="space-y-5">
      <div>
        <h2 className="text-xl font-bold text-white tracking-tight">DSP Engine</h2>
        <p className="text-slate-400 text-sm mt-0.5">Precision noise control & latency tuning</p>
      </div>

      {/* Noise Gate */}
      <BigSlider
        label="Dynamic Noise Gate"
        sublabel="NOISE_THRESHOLD"
        value={state.noiseThreshold}
        min={0}
        max={100}
        unit="%"
        color="#22d3ee"
        icon="🔇"
        description="Sets the amplitude floor below which the DSP engine mutes the signal. Higher values cut more background hiss."
        onChange={v => onUpdate({ noiseThreshold: v })}
      />

      {/* Filters section */}
      <div className="bg-slate-800/60 border border-slate-700/50 rounded-xl p-4">
        <div className="flex items-center gap-2 mb-4">
          <span className="text-xl">🎛️</span>
          <div>
            <div className="text-white font-semibold text-sm">Environment Filters</div>
            <div className="text-slate-500 text-xs">Frequency cutoff controls for room acoustics</div>
          </div>
        </div>

        <div className="space-y-6">
          {/* Low-Pass filter */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <div>
                <span className="text-slate-200 text-sm font-semibold">Low-Pass Filter</span>
                <span className="ml-2 text-xs text-slate-500 font-mono">(Hiss Killer)</span>
              </div>
              <span className="font-mono font-bold text-emerald-400 text-sm">{state.lowPass.toLocaleString()} Hz</span>
            </div>
            <p className="text-slate-500 text-xs mb-2">Blocks frequencies ABOVE this point. Reduces high-frequency hiss, static, and digital noise.</p>
            <div className="relative h-3 bg-slate-700/70 rounded-full overflow-hidden">
              <div
                className="h-full rounded-full bg-emerald-500 transition-all duration-100"
                style={{ width: `${((state.lowPass - 1000) / (20000 - 1000)) * 100}%`, boxShadow: '0 0 8px #22c55e60' }}
              />
            </div>
            <input
              type="range"
              min={1000}
              max={20000}
              step={100}
              value={state.lowPass}
              onChange={e => onUpdate({ lowPass: Number(e.target.value) })}
              className="w-full mt-1 cursor-pointer"
              style={{ accentColor: '#22c55e' }}
            />
            <div className="flex justify-between text-xs text-slate-500 font-mono">
              <span>1 kHz</span>
              <span>20 kHz</span>
            </div>
          </div>

          {/* High-Pass filter */}
          <div>
            <div className="flex justify-between items-center mb-1">
              <div>
                <span className="text-slate-200 text-sm font-semibold">High-Pass Filter</span>
                <span className="ml-2 text-xs text-slate-500 font-mono">(Hum/Thump Killer)</span>
              </div>
              <span className="font-mono font-bold text-rose-400 text-sm">{state.highPass} Hz</span>
            </div>
            <p className="text-slate-500 text-xs mb-2">Blocks frequencies BELOW this point. Eliminates 60Hz mains hum, handling noise, and stage rumble.</p>
            <div className="relative h-3 bg-slate-700/70 rounded-full overflow-hidden">
              <div
                className="h-full rounded-full bg-rose-500 transition-all duration-100"
                style={{ width: `${((state.highPass - 20) / (500 - 20)) * 100}%`, boxShadow: '0 0 8px #ef444460' }}
              />
            </div>
            <input
              type="range"
              min={20}
              max={500}
              step={5}
              value={state.highPass}
              onChange={e => onUpdate({ highPass: Number(e.target.value) })}
              className="w-full mt-1 cursor-pointer"
              style={{ accentColor: '#ef4444' }}
            />
            <div className="flex justify-between text-xs text-slate-500 font-mono">
              <span>20 Hz</span>
              <span>500 Hz</span>
            </div>
          </div>
        </div>

        {/* Frequency response mini graph */}
        <div className="mt-4 rounded-lg bg-slate-900/60 p-3 h-16 relative overflow-hidden">
          <div className="absolute inset-x-0 top-0 h-px bg-slate-600/30" />
          <svg className="w-full h-full" viewBox="0 0 400 60" preserveAspectRatio="none">
            {/* Grid lines */}
            {[0.25, 0.5, 0.75].map(y => (
              <line key={y} x1="0" y1={y * 60} x2="400" y2={y * 60} stroke="#334155" strokeWidth="0.5" />
            ))}
            {/* Passband shape */}
            <defs>
              <linearGradient id="freqGrad" x1="0" y1="0" x2="1" y2="0">
                <stop offset="0%" stopColor="#ef4444" stopOpacity="0.4" />
                <stop offset="20%" stopColor="#22d3ee" stopOpacity="0.7" />
                <stop offset="80%" stopColor="#22d3ee" stopOpacity="0.7" />
                <stop offset="100%" stopColor="#22c55e" stopOpacity="0.4" />
              </linearGradient>
            </defs>
            {(() => {
              const hpPct = ((state.highPass - 20) / (500 - 20)) * 0.3;
              const lpPct = ((state.lowPass - 1000) / (20000 - 1000)) * 0.7 + 0.3;
              const hpX = hpPct * 400;
              const lpX = lpPct * 400;
              return (
                <path
                  d={`M 0 58 L ${hpX - 20} 58 L ${hpX} 8 L ${lpX} 8 L ${lpX + 20} 58 L 400 58 Z`}
                  fill="url(#freqGrad)"
                  stroke="#22d3ee"
                  strokeWidth="1.5"
                  strokeOpacity="0.6"
                />
              );
            })()}
          </svg>
          <div className="absolute bottom-1 left-3 text-slate-600 text-xs font-mono">20Hz</div>
          <div className="absolute bottom-1 right-3 text-slate-600 text-xs font-mono">20kHz</div>
        </div>
      </div>

      {/* Latency vs Quality */}
      <div className="bg-slate-800/60 border border-slate-700/50 rounded-xl p-4">
        <div className="flex items-center gap-2 mb-3">
          <span className="text-xl">⚖️</span>
          <div>
            <div className="text-white font-semibold text-sm">Latency vs. Quality Mode</div>
            <div className="text-slate-500 text-xs">Buffer size tradeoff selector</div>
          </div>
        </div>

        <div className="grid grid-cols-2 gap-3">
          <button
            onClick={() => onUpdate({ latencyMode: 'low' })}
            className={`rounded-xl p-4 border text-left transition-all duration-200 ${
              state.latencyMode === 'low'
                ? 'border-cyan-500/70 bg-cyan-500/10 shadow-lg shadow-cyan-500/10'
                : 'border-slate-700/50 bg-slate-800/40 hover:border-slate-600'
            }`}
          >
            <div className="text-2xl mb-2">⚡</div>
            <div className="text-white font-semibold text-sm">Ultra-Low Latency</div>
            <div className="text-slate-400 text-xs mt-1">64-sample buffer. Best for live soloing & real-time performance.</div>
            <div className="mt-2 font-mono text-xs">
              <span className="text-cyan-400">~1.5ms RTT</span>
              <span className="text-slate-500 ml-2">/ 64 smp</span>
            </div>
          </button>

          <button
            onClick={() => onUpdate({ latencyMode: 'quality' })}
            className={`rounded-xl p-4 border text-left transition-all duration-200 ${
              state.latencyMode === 'quality'
                ? 'border-violet-500/70 bg-violet-500/10 shadow-lg shadow-violet-500/10'
                : 'border-slate-700/50 bg-slate-800/40 hover:border-slate-600'
            }`}
          >
            <div className="text-2xl mb-2">🎛️</div>
            <div className="text-white font-semibold text-sm">Buffer Stability</div>
            <div className="text-slate-400 text-xs mt-1">512-sample buffer. Best for recording & maximum signal quality.</div>
            <div className="mt-2 font-mono text-xs">
              <span className="text-violet-400">~11ms RTT</span>
              <span className="text-slate-500 ml-2">/ 512 smp</span>
            </div>
          </button>
        </div>

        <div className="mt-3 p-3 rounded-lg bg-slate-900/50 border border-slate-700/30">
          <div className="flex items-center gap-2">
            <div className="w-2 h-2 rounded-full animate-pulse" style={{ backgroundColor: state.latencyMode === 'low' ? '#22d3ee' : '#a78bfa' }} />
            <span className="text-xs text-slate-400">
              Active: <strong className={state.latencyMode === 'low' ? 'text-cyan-400' : 'text-violet-400'}>
                {state.latencyMode === 'low' ? 'Ultra-Low Latency Mode — ESP32 using 64-sample DMA buffer' : 'Quality Mode — ESP32 using 512-sample stable buffer'}
              </strong>
            </span>
          </div>
        </div>
      </div>
    </div>
  );
}
