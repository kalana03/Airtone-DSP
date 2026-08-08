export type PageId = 'dashboard' | 'soundforge' | 'dspengine' | 'presets';

export interface Preset {
  id: string;
  name: string;
  tag: 'Bedroom Practice' | 'Live Stage' | 'Heavy Metal Solo' | 'Recording' | 'Custom';
  instrument: string;
  reverb: boolean;
  overdrive: boolean;
  eq: { bass: number; mids: number; treble: number };
  noiseThreshold: number;
  lowPass: number;
  highPass: number;
  latencyMode: 'low' | 'quality';
  createdAt: string;
}

export interface AppState {
  // Sound Forge
  selectedInstrument: string;
  reverb: boolean;
  overdrive: boolean;
  eq: { bass: number; mids: number; treble: number };

  // DSP Engine
  noiseThreshold: number;
  lowPass: number;
  highPass: number;
  latencyMode: 'low' | 'quality';

  // Presets
  presets: Preset[];
}
