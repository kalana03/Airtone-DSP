import { useState, useEffect, useRef } from 'react';

export function useWaveform(noiseThreshold: number) {
  const [samples, setSamples] = useState<number[]>(Array(80).fill(0));
  const frameRef = useRef(0);
  const timeRef = useRef(0);

  useEffect(() => {
    const animate = () => {
      timeRef.current += 0.08;
      setSamples(prev => {
        const newSamples = [...prev.slice(1)];
        const t = timeRef.current;
        const base =
          Math.sin(t * 2.3) * 0.4 +
          Math.sin(t * 5.1) * 0.2 +
          Math.sin(t * 7.7) * 0.1 +
          (Math.random() - 0.5) * 0.15;
        const val = Math.max(-1, Math.min(1, base));
        newSamples.push(val);
        return newSamples;
      });
      frameRef.current = requestAnimationFrame(animate);
    };
    frameRef.current = requestAnimationFrame(animate);
    return () => cancelAnimationFrame(frameRef.current);
  }, []);

  const isClipping = samples.some(s => Math.abs(s) > 0.88);
  const isTooQuiet = samples.every(s => Math.abs(s) < noiseThreshold / 100 + 0.05);

  return { samples, isClipping, isTooQuiet };
}

export function useConnectionStrength() {
  const [rssi, setRssi] = useState(-52);
  const [espNowLink, setEspNowLink] = useState(94);
  const [battery, setBattery] = useState(78);
  const [packetLoss, setPacketLoss] = useState(0.4);

  useEffect(() => {
    const interval = setInterval(() => {
      setRssi(v => Math.max(-90, Math.min(-30, v + (Math.random() - 0.5) * 3)));
      setEspNowLink(v => Math.max(60, Math.min(100, v + (Math.random() - 0.5) * 2)));
      setPacketLoss(v => Math.max(0, Math.min(5, v + (Math.random() - 0.5) * 0.3)));
      setBattery(v => Math.max(0, v - 0.002));
    }, 1200);
    return () => clearInterval(interval);
  }, []);

  const wifiStrength = Math.round(((rssi + 90) / 60) * 100);

  return { rssi: Math.round(rssi), espNowLink: Math.round(espNowLink), battery: Math.round(battery), packetLoss: packetLoss.toFixed(1), wifiStrength };
}

export function usePulse(active: boolean) {
  const [pulse, setPulse] = useState(false);
  useEffect(() => {
    if (!active) return;
    const interval = setInterval(() => setPulse(p => !p), 600);
    return () => clearInterval(interval);
  }, [active]);
  return pulse;
}
