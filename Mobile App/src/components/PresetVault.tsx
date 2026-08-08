import { useState } from 'react';
import { AppState, Preset } from '../types';

const TAG_COLORS: Record<string, string> = {
  'Bedroom Practice': 'bg-blue-500/20 text-blue-300 border-blue-500/30',
  'Live Stage': 'bg-red-500/20 text-red-300 border-red-500/30',
  'Heavy Metal Solo': 'bg-orange-500/20 text-orange-300 border-orange-500/30',
  'Recording': 'bg-violet-500/20 text-violet-300 border-violet-500/30',
  'Custom': 'bg-slate-500/20 text-slate-300 border-slate-500/30',
};

const TAG_ICONS: Record<string, string> = {
  'Bedroom Practice': '🛏️',
  'Live Stage': '🎤',
  'Heavy Metal Solo': '🤘',
  'Recording': '🎙️',
  'Custom': '⚙️',
};

interface PresetVaultProps {
  state: AppState;
  onLoadPreset: (preset: Preset) => void;
  onSavePreset: (preset: Preset) => void;
  onDeletePreset: (id: string) => void;
}

function PresetCard({
  preset,
  onLoad,
  onDelete,
  syncing,
}: {
  preset: Preset;
  onLoad: () => void;
  onDelete: () => void;
  syncing: boolean;
}) {
  return (
    <div className="bg-slate-800/60 border border-slate-700/50 rounded-xl p-4 flex flex-col gap-3 hover:border-slate-600 transition-all">
      <div className="flex items-start justify-between">
        <div className="flex-1 min-w-0">
          <div className="flex items-center gap-2 flex-wrap">
            <span className="text-white font-semibold text-sm truncate">{preset.name}</span>
            <span className={`text-xs px-2 py-0.5 rounded-full border font-medium ${TAG_COLORS[preset.tag]}`}>
              {TAG_ICONS[preset.tag]} {preset.tag}
            </span>
          </div>
          <div className="text-slate-500 text-xs mt-1">{preset.createdAt}</div>
        </div>
        <button
          onClick={onDelete}
          className="text-slate-600 hover:text-red-400 transition-colors p-1 ml-2 shrink-0"
          title="Delete preset"
        >
          <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 7l-.867 12.142A2 2 0 0116.138 21H7.862a2 2 0 01-1.995-1.858L5 7m5 4v6m4-6v6m1-10V4a1 1 0 00-1-1h-4a1 1 0 00-1 1v3M4 7h16" />
          </svg>
        </button>
      </div>

      {/* Settings summary */}
      <div className="grid grid-cols-3 gap-2 text-xs">
        <div className="bg-slate-900/50 rounded-lg px-2 py-1.5">
          <div className="text-slate-500">Instrument</div>
          <div className="text-slate-200 font-mono truncate">{preset.instrument.replace('-', ' ')}</div>
        </div>
        <div className="bg-slate-900/50 rounded-lg px-2 py-1.5">
          <div className="text-slate-500">FX Chain</div>
          <div className="text-slate-200 font-mono">
            {[preset.reverb && 'Rev', preset.overdrive && 'OD'].filter(Boolean).join('+') || 'Dry'}
          </div>
        </div>
        <div className="bg-slate-900/50 rounded-lg px-2 py-1.5">
          <div className="text-slate-500">Mode</div>
          <div className="text-slate-200 font-mono">{preset.latencyMode === 'low' ? '⚡ Low' : '🎛️ Qual'}</div>
        </div>
      </div>

      {/* EQ mini viz */}
      <div className="flex items-end gap-1 h-6 bg-slate-900/40 rounded px-2">
        {[preset.eq.bass, ...Array(3).fill(preset.eq.mids), preset.eq.treble].map((v, i) => (
          <div
            key={i}
            className="flex-1 rounded-t"
            style={{
              height: `${Math.max(10, v)}%`,
              backgroundColor: i < 2 ? '#3b82f6' : i < 3 ? '#22d3ee' : '#a78bfa',
              opacity: 0.7,
            }}
          />
        ))}
      </div>

      {/* Sync button */}
      <button
        onClick={onLoad}
        className={`w-full py-2 rounded-lg text-sm font-semibold transition-all duration-200 flex items-center justify-center gap-2 ${
          syncing
            ? 'bg-violet-500/30 text-violet-300 border border-violet-500/50'
            : 'bg-violet-600 hover:bg-violet-500 text-white shadow-lg shadow-violet-500/20'
        }`}
      >
        {syncing ? (
          <>
            <div className="w-3.5 h-3.5 rounded-full border-2 border-violet-400 border-t-transparent animate-spin" />
            Syncing to ESP32...
          </>
        ) : (
          <>
            <svg className="w-4 h-4" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M13 10V3L4 14h7v7l9-11h-7z" />
            </svg>
            One-Tap Sync
          </>
        )}
      </button>
    </div>
  );
}

export default function PresetVault({ state, onLoadPreset, onSavePreset, onDeletePreset }: PresetVaultProps) {
  const [showSaveForm, setShowSaveForm] = useState(false);
  const [newName, setNewName] = useState('');
  const [newTag, setNewTag] = useState<Preset['tag']>('Custom');
  const [syncingId, setSyncingId] = useState<string | null>(null);
  const [filterTag, setFilterTag] = useState<string>('All');

  const handleSave = () => {
    if (!newName.trim()) return;
    const preset: Preset = {
      id: Date.now().toString(),
      name: newName.trim(),
      tag: newTag,
      instrument: state.selectedInstrument,
      reverb: state.reverb,
      overdrive: state.overdrive,
      eq: { ...state.eq },
      noiseThreshold: state.noiseThreshold,
      lowPass: state.lowPass,
      highPass: state.highPass,
      latencyMode: state.latencyMode,
      createdAt: new Date().toLocaleString(),
    };
    onSavePreset(preset);
    setNewName('');
    setShowSaveForm(false);
  };

  const handleLoad = (preset: Preset) => {
    setSyncingId(preset.id);
    setTimeout(() => {
      onLoadPreset(preset);
      setSyncingId(null);
    }, 1800);
  };

  const filteredPresets = filterTag === 'All'
    ? state.presets
    : state.presets.filter(p => p.tag === filterTag);

  return (
    <div className="space-y-5">
      <div className="flex items-start justify-between">
        <div>
          <h2 className="text-xl font-bold text-white tracking-tight">Preset Vault</h2>
          <p className="text-slate-400 text-sm mt-0.5">Save & recall complete ESP32 loadouts</p>
        </div>
        <button
          onClick={() => setShowSaveForm(v => !v)}
          className="flex items-center gap-2 px-4 py-2 rounded-xl bg-violet-600 hover:bg-violet-500 text-white text-sm font-semibold transition-all shadow-lg shadow-violet-500/20"
        >
          <span>{showSaveForm ? '✕' : '+'}</span>
          {showSaveForm ? 'Cancel' : 'Save Current'}
        </button>
      </div>

      {/* Save form */}
      {showSaveForm && (
        <div className="bg-slate-800/80 border border-violet-500/30 rounded-xl p-4 space-y-3">
          <div className="text-violet-300 text-sm font-semibold">💾 Save Current Loadout</div>

          <div>
            <label className="text-slate-400 text-xs mb-1 block">Preset Name</label>
            <input
              type="text"
              value={newName}
              onChange={e => setNewName(e.target.value)}
              placeholder="e.g. My Stage Setup"
              className="w-full bg-slate-900/60 border border-slate-600 rounded-lg px-3 py-2 text-white text-sm placeholder-slate-500 focus:outline-none focus:border-violet-500 transition-colors"
              onKeyDown={e => e.key === 'Enter' && handleSave()}
              autoFocus
            />
          </div>

          <div>
            <label className="text-slate-400 text-xs mb-2 block">Context Tag</label>
            <div className="flex flex-wrap gap-2">
              {(Object.keys(TAG_COLORS) as Preset['tag'][]).map(tag => (
                <button
                  key={tag}
                  onClick={() => setNewTag(tag)}
                  className={`px-3 py-1.5 rounded-full text-xs font-medium border transition-all ${
                    newTag === tag ? TAG_COLORS[tag] : 'bg-slate-700/50 text-slate-400 border-slate-600 hover:border-slate-500'
                  }`}
                >
                  {TAG_ICONS[tag]} {tag}
                </button>
              ))}
            </div>
          </div>

          {/* Current settings preview */}
          <div className="bg-slate-900/50 rounded-lg p-3 grid grid-cols-3 gap-2 text-xs">
            <div><span className="text-slate-500">Instrument: </span><span className="text-slate-200">{state.selectedInstrument}</span></div>
            <div><span className="text-slate-500">Reverb: </span><span className="text-slate-200">{state.reverb ? 'On' : 'Off'}</span></div>
            <div><span className="text-slate-500">Overdrive: </span><span className="text-slate-200">{state.overdrive ? 'On' : 'Off'}</span></div>
            <div><span className="text-slate-500">Gate: </span><span className="text-slate-200">{state.noiseThreshold}%</span></div>
            <div><span className="text-slate-500">LP Filter: </span><span className="text-slate-200">{state.lowPass}Hz</span></div>
            <div><span className="text-slate-500">Mode: </span><span className="text-slate-200">{state.latencyMode}</span></div>
          </div>

          <button
            onClick={handleSave}
            disabled={!newName.trim()}
            className="w-full py-2.5 rounded-xl bg-violet-600 hover:bg-violet-500 disabled:opacity-40 disabled:cursor-not-allowed text-white font-semibold text-sm transition-all"
          >
            Save Preset →
          </button>
        </div>
      )}

      {/* Filter tabs */}
      <div className="flex gap-2 flex-wrap">
        {['All', ...Object.keys(TAG_COLORS)].map(tag => (
          <button
            key={tag}
            onClick={() => setFilterTag(tag)}
            className={`px-3 py-1.5 rounded-full text-xs font-medium border transition-all ${
              filterTag === tag
                ? 'bg-slate-600 text-white border-slate-500'
                : 'bg-slate-800/40 text-slate-400 border-slate-700/50 hover:border-slate-600'
            }`}
          >
            {tag !== 'All' && TAG_ICONS[tag]} {tag} {tag !== 'All' && `(${state.presets.filter(p => p.tag === tag).length})`}
          </button>
        ))}
      </div>

      {/* Presets grid */}
      {filteredPresets.length === 0 ? (
        <div className="text-center py-16 text-slate-500">
          <div className="text-5xl mb-3">📭</div>
          <div className="text-sm font-medium">No presets saved yet</div>
          <div className="text-xs mt-1">Hit "Save Current" to store your first loadout</div>
        </div>
      ) : (
        <div className="grid grid-cols-2 gap-4">
          {filteredPresets.map(preset => (
            <PresetCard
              key={preset.id}
              preset={preset}
              onLoad={() => handleLoad(preset)}
              onDelete={() => onDeletePreset(preset.id)}
              syncing={syncingId === preset.id}
            />
          ))}
        </div>
      )}

      {/* Stats bar */}
      {state.presets.length > 0 && (
        <div className="flex gap-4 text-xs text-slate-500 border-t border-slate-700/50 pt-3">
          <span>📦 {state.presets.length} presets stored</span>
          <span>•</span>
          <span>Last saved: {state.presets[state.presets.length - 1]?.createdAt}</span>
        </div>
      )}
    </div>
  );
}
