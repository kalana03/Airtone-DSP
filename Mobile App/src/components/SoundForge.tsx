import { useState } from 'react';
import { AppState } from '../types';

const INSTRUMENTS = [
  { id: 'clean-acoustic', name: 'Clean Acoustic', icon: '🎸', desc: 'Crisp, natural finger-picked tone', color: 'from-amber-500 to-orange-500' },
  { id: 'electric-crunch', name: 'Electric Crunch', icon: '⚡', desc: 'Warm mid-gain crunch overdrive', color: 'from-yellow-500 to-red-500' },
  { id: 'octave-bass', name: 'Octave Bass', icon: '🎵', desc: 'Deep sub-octave bass emulation', color: 'from-blue-600 to-indigo-700' },
  { id: 'lofi-synth', name: 'Lo-Fi Synth', icon: '🎹', desc: 'Bitcrushed vintage tape warmth', color: 'from-violet-500 to-purple-700' },
  { id: 'steel-string', name: 'Steel String', icon: '✨', desc: 'Bright piezo pickup simulation', color: 'from-cyan-500 to-teal-600' },
  { id: 'nylon-classical', name: 'Nylon Classical', icon: '🌿', desc: 'Soft classical guitar timbre', color: 'from-green-500 to-emerald-700' },
  { id: 'dirty-blues', name: 'Dirty Blues', icon: '🎷', desc: 'Gritty amp-in-a-box blues tone', color: 'from-red-600 to-rose-800' },
  { id: 'ambient-pad', name: 'Ambient Pad', icon: '🌌', desc: 'Lush reverb-heavy pad texture', color: 'from-indigo-600 to-violet-800' },
];



interface SoundForgeProps {
  state: AppState;
  onUpdate: (updates: Partial<AppState>) => void;
}

export default function SoundForge({ state, onUpdate }: SoundForgeProps) {
  const [carouselOffset, setCarouselOffset] = useState(0);
  const visibleCount = 4;

  const visibleInstruments = INSTRUMENTS.slice(carouselOffset, carouselOffset + visibleCount);
  const canScrollLeft = carouselOffset > 0;
  const canScrollRight = carouselOffset + visibleCount < INSTRUMENTS.length;

  return (
    <div className="space-y-5">
      <div>
        <h2 className="text-xl font-bold text-white tracking-tight">Sound Forge</h2>
        <p className="text-slate-400 text-sm mt-0.5">Instrument profiles & real-time FX processing</p>
      </div>

      {/* Instrument Carousel */}
      <div className="bg-slate-800/60 border border-slate-700/50 rounded-xl p-4">
        <div className="flex items-center justify-between mb-3">
          <span className="text-slate-300 text-sm font-semibold uppercase tracking-wide">Instrument Profile</span>
          <div className="flex gap-2">
            <button
              onClick={() => setCarouselOffset(o => Math.max(0, o - 1))}
              disabled={!canScrollLeft}
              className="w-7 h-7 rounded-lg bg-slate-700 flex items-center justify-center text-slate-300 hover:bg-slate-600 disabled:opacity-30 disabled:cursor-not-allowed transition-all"
            >
              ‹
            </button>
            <button
              onClick={() => setCarouselOffset(o => Math.min(INSTRUMENTS.length - visibleCount, o + 1))}
              disabled={!canScrollRight}
              className="w-7 h-7 rounded-lg bg-slate-700 flex items-center justify-center text-slate-300 hover:bg-slate-600 disabled:opacity-30 disabled:cursor-not-allowed transition-all"
            >
              ›
            </button>
          </div>
        </div>

        <div className="grid grid-cols-4 gap-3">
          {visibleInstruments.map(inst => {
            const isSelected = state.selectedInstrument === inst.id;
            return (
              <button
                key={inst.id}
                onClick={() => onUpdate({ selectedInstrument: inst.id })}
                className={`relative rounded-xl p-3 text-left transition-all duration-200 border ${
                  isSelected
                    ? 'border-violet-500/70 bg-violet-500/10 shadow-lg shadow-violet-500/10'
                    : 'border-slate-700/50 bg-slate-800/40 hover:border-slate-600 hover:bg-slate-700/40'
                }`}
              >
                {isSelected && (
                  <div className="absolute top-2 right-2 w-2 h-2 rounded-full bg-violet-400 shadow-sm shadow-violet-400" />
                )}
                <div className={`w-10 h-10 rounded-lg bg-gradient-to-br ${inst.color} flex items-center justify-center text-xl mb-2 shadow-md`}>
                  {inst.icon}
                </div>
                <div className="text-white text-xs font-semibold leading-tight">{inst.name}</div>
                <div className="text-slate-500 text-xs mt-1 leading-tight">{inst.desc}</div>
              </button>
            );
          })}
        </div>

        <div className="flex justify-center gap-1.5 mt-3">
          {Array.from({ length: INSTRUMENTS.length - visibleCount + 1 }).map((_, i) => (
            <button
              key={i}
              onClick={() => setCarouselOffset(i)}
              className={`h-1.5 rounded-full transition-all ${i === carouselOffset ? 'w-5 bg-violet-400' : 'w-1.5 bg-slate-600'}`}
            />
          ))}
        </div>
      </div>

      {/* Digital Pedalboard */}
      <div className="bg-slate-800/60 border border-slate-700/50 rounded-xl p-4">
        <span className="text-slate-300 text-sm font-semibold uppercase tracking-wide block mb-3">Digital Pedalboard</span>
        <div className="grid grid-cols-2 gap-3">
          {/* Reverb */}
          <div
            className={`relative rounded-xl p-4 border cursor-pointer transition-all duration-200 ${
              state.reverb
                ? 'border-cyan-500/60 bg-cyan-500/10 shadow-lg shadow-cyan-500/10'
                : 'border-slate-700/50 bg-slate-800/40 hover:border-slate-600'
            }`}
            onClick={() => onUpdate({ reverb: !state.reverb })}
          >
            <div className="flex items-center justify-between mb-2">
              <div className="flex items-center gap-2">
                <span className="text-xl">🌊</span>
                <span className="text-white text-sm font-semibold">Reverb</span>
              </div>
              <div className={`relative w-11 h-6 rounded-full transition-colors duration-200 ${state.reverb ? 'bg-cyan-500' : 'bg-slate-600'}`}>
                <div className={`absolute top-0.5 w-5 h-5 rounded-full bg-white shadow transition-transform duration-200 ${state.reverb ? 'translate-x-5' : 'translate-x-0.5'}`} />
              </div>
            </div>
            <p className="text-slate-400 text-xs">Trailing echo & space simulation</p>
            {state.reverb && <div className="mt-2 text-cyan-400 text-xs font-mono">● ACTIVE — Hall Reverb</div>}
          </div>

          {/* Overdrive */}
          <div
            className={`relative rounded-xl p-4 border cursor-pointer transition-all duration-200 ${
              state.overdrive
                ? 'border-orange-500/60 bg-orange-500/10 shadow-lg shadow-orange-500/10'
                : 'border-slate-700/50 bg-slate-800/40 hover:border-slate-600'
            }`}
            onClick={() => onUpdate({ overdrive: !state.overdrive })}
          >
            <div className="flex items-center justify-between mb-2">
              <div className="flex items-center gap-2">
                <span className="text-xl">🔥</span>
                <span className="text-white text-sm font-semibold">Overdrive</span>
              </div>
              <div className={`relative w-11 h-6 rounded-full transition-colors duration-200 ${state.overdrive ? 'bg-orange-500' : 'bg-slate-600'}`}>
                <div className={`absolute top-0.5 w-5 h-5 rounded-full bg-white shadow transition-transform duration-200 ${state.overdrive ? 'translate-x-5' : 'translate-x-0.5'}`} />
              </div>
            </div>
            <p className="text-slate-400 text-xs">Intentional waveform clipping distortion</p>
            {state.overdrive && <div className="mt-2 text-orange-400 text-xs font-mono">● ACTIVE — Tube Saturation</div>}
          </div>
        </div>
      </div>

      {/* 3-Band EQ */}
      <div className="bg-slate-800/60 border border-slate-700/50 rounded-xl p-4">
        <div className="flex items-center justify-between mb-4">
          <span className="text-slate-300 text-sm font-semibold uppercase tracking-wide">3-Band Equalizer</span>
          <button
            onClick={() => onUpdate({ eq: { bass: 50, mids: 50, treble: 50 } })}
            className="text-xs text-slate-500 hover:text-slate-300 px-2 py-1 rounded bg-slate-700/50 hover:bg-slate-700 transition-colors"
          >
            Reset
          </button>
        </div>
        <div className="space-y-4">
          {[
            { key: 'bass' as const, label: 'Bass', range: '20–300 Hz', color: '#3b82f6', icon: '🔉' },
            { key: 'mids' as const, label: 'Mids', range: '300–4k Hz', color: '#22d3ee', icon: '🔊' },
            { key: 'treble' as const, label: 'Treble', range: '4k–20k Hz', color: '#a78bfa', icon: '✨' },
          ].map(band => {
            const val = state.eq[band.key];
            const db = ((val - 50) / 50 * 12).toFixed(1);
            return (
              <div key={band.key} className="flex items-center gap-4">
                <div className="flex items-center gap-2 w-24 shrink-0">
                  <span className="text-base">{band.icon}</span>
                  <div>
                    <div className="text-slate-200 text-xs font-semibold">{band.label}</div>
                    <div className="text-slate-500 text-xs">{band.range}</div>
                  </div>
                </div>
                <div className="flex-1 relative">
                  <div className="absolute inset-0 flex items-center">
                    <div className="w-full h-px bg-slate-600/60" />
                  </div>
                  <input
                    type="range"
                    min={0}
                    max={100}
                    value={val}
                    onChange={e => onUpdate({ eq: { ...state.eq, [band.key]: Number(e.target.value) } })}
                    className="relative w-full cursor-pointer"
                    style={{ accentColor: band.color }}
                  />
                </div>
                <div className="w-14 text-right">
                  <span className="font-mono text-sm font-bold" style={{ color: band.color }}>
                    {Number(db) >= 0 ? '+' : ''}{db} dB
                  </span>
                </div>
              </div>
            );
          })}
        </div>

        {/* Mini EQ visualizer */}
        <div className="mt-4 flex items-end justify-center gap-1 h-12 bg-slate-900/50 rounded-lg px-3">
          {Array.from({ length: 16 }).map((_, i) => {
            const progress = i / 15;
            let h: number;
            if (progress < 0.33) {
              h = 20 + (state.eq.bass / 100) * 60;
            } else if (progress < 0.66) {
              h = 20 + (state.eq.mids / 100) * 60;
            } else {
              h = 20 + (state.eq.treble / 100) * 60;
            }
            const color = progress < 0.33 ? '#3b82f6' : progress < 0.66 ? '#22d3ee' : '#a78bfa';
            return (
              <div
                key={i}
                className="flex-1 rounded-t transition-all duration-300"
                style={{ height: `${h}%`, backgroundColor: color, opacity: 0.7 }}
              />
            );
          })}
        </div>
      </div>
    </div>
  );
}
