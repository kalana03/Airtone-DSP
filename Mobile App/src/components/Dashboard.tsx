import { useRef, useEffect } from 'react';
import { useWaveform, useConnectionStrength, usePulse } from '../hooks/useSimulatedData';

interface DashboardProps {
  noiseThreshold: number;
}

function SignalBar({ level, color }: { level: number; color: string }) {
  return (
    <div className="flex items-end gap-[3px] h-6">
      {[0.2, 0.4, 0.6, 0.8, 1.0].map((threshold, i) => (
        <div
          key={i}
          className="w-[5px] rounded-sm transition-all duration-300"
          style={{
            height: `${(i + 1) * 18}%`,
            backgroundColor: level / 100 >= threshold ? color : '#1e293b',
            minHeight: '4px',
          }}
        />
      ))}
    </div>
  );
}

function WaveformCanvas({ samples, isClipping, isTooQuiet }: {
  samples: number[];
  isClipping: boolean;
  isTooQuiet: boolean;
}) {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const W = canvas.width;
    const H = canvas.height;
    ctx.clearRect(0, 0, W, H);

    // Background grid
    ctx.strokeStyle = 'rgba(100,116,139,0.15)';
    ctx.lineWidth = 0.5;
    for (let y = 0; y <= 4; y++) {
      ctx.beginPath();
      ctx.moveTo(0, (y / 4) * H);
      ctx.lineTo(W, (y / 4) * H);
      ctx.stroke();
    }

    // Center line
    ctx.strokeStyle = 'rgba(100,116,139,0.3)';
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(0, H / 2);
    ctx.lineTo(W, H / 2);
    ctx.stroke();

    // Noise threshold band
    const thresholdY = H / 2 * 0.12;
    ctx.fillStyle = 'rgba(234,179,8,0.07)';
    ctx.fillRect(0, H / 2 - thresholdY, W, thresholdY * 2);

    // Clipping zone
    ctx.fillStyle = 'rgba(239,68,68,0.08)';
    ctx.fillRect(0, 0, W, H * 0.07);
    ctx.fillRect(0, H * 0.93, W, H * 0.07);

    // Waveform
    if (samples.length < 2) return;
    const gradient = ctx.createLinearGradient(0, 0, W, 0);
    if (isClipping) {
      gradient.addColorStop(0, '#f97316');
      gradient.addColorStop(1, '#ef4444');
    } else if (isTooQuiet) {
      gradient.addColorStop(0, '#eab308');
      gradient.addColorStop(1, '#f59e0b');
    } else {
      gradient.addColorStop(0, '#22d3ee');
      gradient.addColorStop(1, '#6366f1');
    }

    ctx.beginPath();
    ctx.strokeStyle = gradient;
    ctx.lineWidth = 2;
    ctx.lineJoin = 'round';

    samples.forEach((s, i) => {
      const x = (i / (samples.length - 1)) * W;
      const y = H / 2 - s * (H / 2) * 0.85;
      if (i === 0) ctx.moveTo(x, y);
      else ctx.lineTo(x, y);
    });
    ctx.stroke();

    // Filled area under waveform
    ctx.lineTo(W, H / 2);
    ctx.lineTo(0, H / 2);
    ctx.closePath();
    const fillGrad = ctx.createLinearGradient(0, 0, 0, H);
    if (isClipping) {
      fillGrad.addColorStop(0, 'rgba(239,68,68,0.15)');
      fillGrad.addColorStop(1, 'rgba(239,68,68,0.02)');
    } else {
      fillGrad.addColorStop(0, 'rgba(99,102,241,0.12)');
      fillGrad.addColorStop(1, 'rgba(34,211,238,0.02)');
    }
    ctx.fillStyle = fillGrad;
    ctx.fill();
  }, [samples, isClipping, isTooQuiet]);

  return (
    <canvas
      ref={canvasRef}
      width={640}
      height={120}
      className="w-full h-full rounded-lg"
    />
  );
}

export default function Dashboard({ noiseThreshold }: DashboardProps) {
  const { samples, isClipping, isTooQuiet } = useWaveform(noiseThreshold);
  const { rssi, espNowLink, battery, packetLoss, wifiStrength } = useConnectionStrength();
  const pulse = usePulse(true);

  const batteryColor = battery > 60 ? '#22c55e' : battery > 30 ? '#eab308' : '#ef4444';
  const signalColor = wifiStrength > 60 ? '#22d3ee' : wifiStrength > 30 ? '#eab308' : '#ef4444';

  const waveformStatus = isClipping
    ? { label: 'CLIPPING', color: 'text-red-400', bg: 'bg-red-500/10 border-red-500/30' }
    : isTooQuiet
    ? { label: 'TOO QUIET', color: 'text-yellow-400', bg: 'bg-yellow-500/10 border-yellow-500/30' }
    : { label: 'NOMINAL', color: 'text-cyan-400', bg: 'bg-cyan-500/10 border-cyan-500/30' };

  return (
    <div className="space-y-4">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-xl font-bold text-white tracking-tight">Mission Control</h2>
          <p className="text-slate-400 text-sm mt-0.5">Live hardware telemetry dashboard</p>
        </div>
        <div className="flex items-center gap-2">
          <div
            className="w-2.5 h-2.5 rounded-full bg-cyan-400"
            style={{ boxShadow: pulse ? '0 0 8px 3px rgba(34,211,238,0.6)' : 'none', transition: 'box-shadow 0.3s' }}
          />
          <span className="text-cyan-400 text-xs font-mono font-semibold">LIVE</span>
        </div>
      </div>

      {/* Connection HUD */}
      <div className="grid grid-cols-3 gap-3">
        {/* Wi-Fi */}
        <div className="bg-slate-800/60 border border-slate-700/50 rounded-xl p-4 flex flex-col gap-3">
          <div className="flex items-center justify-between">
            <span className="text-slate-400 text-xs font-medium uppercase tracking-wider">Wi-Fi Link</span>
            <svg className="w-4 h-4 text-slate-500" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M8.111 16.404a5.5 5.5 0 017.778 0M12 20h.01m-7.08-7.071c3.904-3.905 10.236-3.905 14.141 0M1.394 9.393c5.857-5.857 15.355-5.857 21.213 0" />
            </svg>
          </div>
          <div className="flex items-end gap-3">
            <SignalBar level={wifiStrength} color={signalColor} />
            <div>
              <div className="text-white font-bold text-lg leading-none">{wifiStrength}%</div>
              <div className="text-slate-500 text-xs font-mono">{rssi} dBm</div>
            </div>
          </div>
          <div className="w-full bg-slate-700/50 rounded-full h-1.5">
            <div
              className="h-1.5 rounded-full transition-all duration-700"
              style={{ width: `${wifiStrength}%`, backgroundColor: signalColor }}
            />
          </div>
        </div>

        {/* ESP-NOW */}
        <div className="bg-slate-800/60 border border-slate-700/50 rounded-xl p-4 flex flex-col gap-3">
          <div className="flex items-center justify-between">
            <span className="text-slate-400 text-xs font-medium uppercase tracking-wider">ESP-NOW</span>
            <svg className="w-4 h-4 text-slate-500" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M13 10V3L4 14h7v7l9-11h-7z" />
            </svg>
          </div>
          <div className="flex items-end gap-3">
            <SignalBar level={espNowLink} color="#a78bfa" />
            <div>
              <div className="text-white font-bold text-lg leading-none">{espNowLink}%</div>
              <div className="text-slate-500 text-xs font-mono">{packetLoss}% loss</div>
            </div>
          </div>
          <div className="w-full bg-slate-700/50 rounded-full h-1.5">
            <div
              className="h-1.5 rounded-full transition-all duration-700 bg-violet-400"
              style={{ width: `${espNowLink}%` }}
            />
          </div>
        </div>

        {/* Battery */}
        <div className="bg-slate-800/60 border border-slate-700/50 rounded-xl p-4 flex flex-col gap-3">
          <div className="flex items-center justify-between">
            <span className="text-slate-400 text-xs font-medium uppercase tracking-wider">Battery</span>
            <svg className="w-4 h-4 text-slate-500" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M17 8h1a2 2 0 012 2v4a2 2 0 01-2 2h-1M3 8h14v9H3z" />
            </svg>
          </div>
          <div className="flex items-center gap-3">
            {/* Battery icon */}
            <div className="relative flex items-center">
              <div className="w-10 h-5 rounded border-2 border-slate-500 relative overflow-hidden">
                <div
                  className="h-full rounded-sm transition-all duration-1000"
                  style={{ width: `${battery}%`, backgroundColor: batteryColor }}
                />
              </div>
              <div className="w-1 h-2.5 rounded-r bg-slate-500 ml-0.5" />
            </div>
            <div>
              <div className="text-white font-bold text-lg leading-none">{battery}%</div>
              <div className="text-slate-500 text-xs font-mono">TX Unit</div>
            </div>
          </div>
          <div className="w-full bg-slate-700/50 rounded-full h-1.5">
            <div
              className="h-1.5 rounded-full transition-all duration-1000"
              style={{ width: `${battery}%`, backgroundColor: batteryColor }}
            />
          </div>
        </div>
      </div>

      {/* Waveform Monitor */}
      <div className={`bg-slate-800/60 border rounded-xl p-4 ${waveformStatus.bg}`}>
        <div className="flex items-center justify-between mb-3">
          <div className="flex items-center gap-2">
            <svg className="w-4 h-4 text-slate-400" fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M9 19V6l12-3v13M9 19c0 1.105-1.343 2-3 2s-3-.895-3-2 1.343-2 3-2 3 .895 3 2zm12-3c0 1.105-1.343 2-3 2s-3-.895-3-2 1.343-2 3-2 3 .895 3 2zM9 10l12-3" />
            </svg>
            <span className="text-slate-300 text-sm font-semibold">Live Waveform Monitor</span>
          </div>
          <div className={`px-2.5 py-0.5 rounded-full text-xs font-bold font-mono ${waveformStatus.color} border ${waveformStatus.bg}`}>
            ⬤ {waveformStatus.label}
          </div>
        </div>
        <div className="h-28 w-full">
          <WaveformCanvas samples={samples} isClipping={isClipping} isTooQuiet={isTooQuiet} />
        </div>
        <div className="flex justify-between mt-2 text-xs text-slate-500 font-mono">
          <span>+CLIP</span>
          <span>Center (0 dB)</span>
          <span>-CLIP</span>
        </div>
      </div>

      {/* Telemetry strip */}
      <div className="grid grid-cols-4 gap-3">
        {[
          { label: 'Latency', value: '4.2 ms', icon: '⚡' },
          { label: 'Sample Rate', value: '44.1 kHz', icon: '📡' },
          { label: 'Buffer Size', value: '256 smp', icon: '💾' },
          { label: 'TX Uptime', value: '2h 14m', icon: '🕐' },
        ].map(item => (
          <div key={item.label} className="bg-slate-800/40 border border-slate-700/40 rounded-xl p-3 text-center">
            <div className="text-xl mb-1">{item.icon}</div>
            <div className="text-white font-mono font-bold text-sm">{item.value}</div>
            <div className="text-slate-500 text-xs mt-0.5">{item.label}</div>
          </div>
        ))}
      </div>
    </div>
  );
}
