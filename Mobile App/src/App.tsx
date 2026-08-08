import { useState } from 'react';
import { PageId, AppState, Preset } from './types';
import Dashboard from './components/Dashboard';
import SoundForge from './components/SoundForge';
import DSPEngine from './components/DSPEngine';
import PresetVault from './components/PresetVault';

const DEFAULT_PRESETS: Preset[] = [
  {
    id: '1',
    name: 'Bedroom Practice',
    tag: 'Bedroom Practice',
    instrument: 'clean-acoustic',
    reverb: true,
    overdrive: false,
    eq: { bass: 55, mids: 50, treble: 45 },
    noiseThreshold: 30,
    lowPass: 12000,
    highPass: 80,
    latencyMode: 'quality',
    createdAt: '6/20/2025, 9:14 AM',
  },
  {
    id: '2',
    name: 'Heavy Metal Solo',
    tag: 'Heavy Metal Solo',
    instrument: 'electric-crunch',
    reverb: false,
    overdrive: true,
    eq: { bass: 70, mids: 35, treble: 80 },
    noiseThreshold: 55,
    lowPass: 8000,
    highPass: 120,
    latencyMode: 'low',
    createdAt: '6/21/2025, 3:42 PM',
  },
  {
    id: '3',
    name: 'Stage Warm',
    tag: 'Live Stage',
    instrument: 'steel-string',
    reverb: true,
    overdrive: false,
    eq: { bass: 45, mids: 60, treble: 65 },
    noiseThreshold: 40,
    lowPass: 15000,
    highPass: 100,
    latencyMode: 'low',
    createdAt: '6/22/2025, 7:01 PM',
  },
];

const INITIAL_STATE: AppState = {
  selectedInstrument: 'clean-acoustic',
  reverb: false,
  overdrive: false,
  eq: { bass: 50, mids: 50, treble: 50 },
  noiseThreshold: 25,
  lowPass: 14000,
  highPass: 80,
  latencyMode: 'low',
  presets: DEFAULT_PRESETS,
};

const NAV_ITEMS: { id: PageId; label: string; icon: string; shortLabel: string }[] = [
  { id: 'dashboard', label: 'Live Dashboard', icon: '📡', shortLabel: 'Dashboard' },
  { id: 'soundforge', label: 'Sound Forge', icon: '🎸', shortLabel: 'Forge' },
  { id: 'dspengine', label: 'DSP Engine', icon: '🎛️', shortLabel: 'DSP' },
  { id: 'presets', label: 'Preset Vault', icon: '💾', shortLabel: 'Presets' },
];

export default function App() {
  const [activePage, setActivePage] = useState<PageId>('dashboard');
  const [appState, setAppState] = useState<AppState>(INITIAL_STATE);
  const [toastMsg, setToastMsg] = useState<string | null>(null);

  const updateState = (updates: Partial<AppState>) => {
    setAppState(prev => ({ ...prev, ...updates }));
  };

  const showToast = (msg: string) => {
    setToastMsg(msg);
    setTimeout(() => setToastMsg(null), 2800);
  };

  const handleLoadPreset = (preset: Preset) => {
    setAppState(prev => ({
      ...prev,
      selectedInstrument: preset.instrument,
      reverb: preset.reverb,
      overdrive: preset.overdrive,
      eq: { ...preset.eq },
      noiseThreshold: preset.noiseThreshold,
      lowPass: preset.lowPass,
      highPass: preset.highPass,
      latencyMode: preset.latencyMode,
    }));
    showToast(`✅ "${preset.name}" synced to ESP32`);
  };

  const handleSavePreset = (preset: Preset) => {
    setAppState(prev => ({ ...prev, presets: [...prev.presets, preset] }));
    showToast(`💾 "${preset.name}" saved to vault`);
  };

  const handleDeletePreset = (id: string) => {
    setAppState(prev => ({ ...prev, presets: prev.presets.filter(p => p.id !== id) }));
    showToast('🗑️ Preset deleted');
  };

  return (
    <div className="min-h-screen bg-[#0a0f1e] text-white flex flex-col" style={{ fontFamily: "'Inter', system-ui, sans-serif" }}>
      {/* Top App Bar */}
      <header className="sticky top-0 z-30 bg-[#0a0f1e]/90 backdrop-blur-xl border-b border-slate-800/60">
        <div className="max-w-4xl mx-auto px-4 h-14 flex items-center justify-between">
          {/* Logo */}
          <div className="flex items-center gap-3">
            <div className="w-8 h-8 rounded-lg bg-gradient-to-br from-cyan-500 to-violet-600 flex items-center justify-center shadow-lg shadow-violet-500/30">
              <svg className="w-4 h-4 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2.5} d="M9 3H5a2 2 0 00-2 2v4m6-6h10a2 2 0 012 2v4M9 3v18m0 0h10a2 2 0 002-2V9M9 21H5a2 2 0 01-2-2V9m0 0h18" />
              </svg>
            </div>
            <div>
              <div className="text-white font-bold text-sm leading-none tracking-tight">AirForge</div>
              <div className="text-slate-500 text-xs">ESP32 Audio Controller</div>
            </div>
          </div>

          {/* Status pills */}
          <div className="flex items-center gap-2">
            <div className="hidden sm:flex items-center gap-1.5 bg-slate-800/60 border border-slate-700/50 rounded-full px-3 py-1">
              <div className="w-1.5 h-1.5 rounded-full bg-cyan-400 animate-pulse" />
              <span className="text-cyan-400 text-xs font-mono font-medium">ESP32 CONNECTED</span>
            </div>
            <div className="hidden sm:flex items-center gap-1.5 bg-slate-800/60 border border-slate-700/50 rounded-full px-3 py-1">
              <span className="text-xs text-slate-400 font-mono">
                {appState.selectedInstrument.replace('-', ' ').toUpperCase()}
              </span>
            </div>
          </div>
        </div>
      </header>

      {/* Main content */}
      <div className="flex-1 max-w-4xl mx-auto w-full px-4 py-5">
        {activePage === 'dashboard' && (
          <Dashboard noiseThreshold={appState.noiseThreshold} />
        )}
        {activePage === 'soundforge' && (
          <SoundForge state={appState} onUpdate={updateState} />
        )}
        {activePage === 'dspengine' && (
          <DSPEngine state={appState} onUpdate={updateState} />
        )}
        {activePage === 'presets' && (
          <PresetVault
            state={appState}
            onLoadPreset={handleLoadPreset}
            onSavePreset={handleSavePreset}
            onDeletePreset={handleDeletePreset}
          />
        )}
      </div>

      {/* Bottom Navigation */}
      <nav className="sticky bottom-0 z-30 bg-[#0a0f1e]/95 backdrop-blur-xl border-t border-slate-800/60">
        <div className="max-w-4xl mx-auto px-2">
          <div className="flex">
            {NAV_ITEMS.map(item => {
              const isActive = activePage === item.id;
              return (
                <button
                  key={item.id}
                  onClick={() => setActivePage(item.id)}
                  className="flex-1 flex flex-col items-center justify-center py-3 gap-0.5 relative transition-all"
                >
                  {isActive && (
                    <div className="absolute top-0 left-1/2 -translate-x-1/2 w-10 h-0.5 bg-gradient-to-r from-cyan-500 to-violet-500 rounded-full" />
                  )}
                  <span className="text-xl leading-none" style={{ filter: isActive ? 'none' : 'grayscale(0.6) opacity(0.5)' }}>
                    {item.icon}
                  </span>
                  <span
                    className="text-xs font-medium transition-colors"
                    style={{ color: isActive ? '#e2e8f0' : '#475569' }}
                  >
                    {item.shortLabel}
                  </span>
                  {/* Preset count badge */}
                  {item.id === 'presets' && appState.presets.length > 0 && (
                    <div className="absolute top-2 right-1/4 w-4 h-4 rounded-full bg-violet-500 text-white text-xs flex items-center justify-center font-bold leading-none">
                      {appState.presets.length}
                    </div>
                  )}
                </button>
              );
            })}
          </div>
        </div>
      </nav>

      {/* Toast notification */}
      <div
        className={`fixed bottom-20 left-1/2 -translate-x-1/2 z-50 px-5 py-2.5 rounded-full bg-slate-800 border border-slate-600 text-white text-sm font-medium shadow-2xl transition-all duration-300 ${
          toastMsg ? 'opacity-100 translate-y-0' : 'opacity-0 translate-y-4 pointer-events-none'
        }`}
      >
        {toastMsg}
      </div>
    </div>
  );
}
