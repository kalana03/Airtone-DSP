import os
import sys
from typing import Optional
import serial
import serial.tools.list_ports
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.animation as animation
from collections import deque

# ============================================================
# CONFIGURATION
# ============================================================
PORT = None             # None = auto-detect; or set explicitly e.g. '/dev/ttyUSB0' or 'COM4'
BAUD = 115200
SAMPLE_RATE = 16000    # Must match your firmware exactly
FFT_SIZE = 1024        # Zero-pad bursts to this size for resolution
HISTORY = 4            # Number of bursts to accumulate before FFT
BUFFER_SIZE = 64       # Must match your firmware BUFFER_SIZE

# ============================================================
# DATA PIPELINE
# ============================================================
sample_buffer = deque(maxlen=FFT_SIZE)
line_time = None
line_freq = None
fig = None

def parse_burst(line: str) -> list[int]:
    """
    Parse a Teleplot-format burst line.
    Expected format: '>RawBurst:100,203,150,...'
    Returns list of integer sample values.
    """
    try:
        if not line.startswith('>RawBurst:'):
            return []
        data_part = line.split(':', 1)[1]
        values = [int(x) for x in data_part.split(',')]
        return values
    except (ValueError, IndexError):
        return []

def apply_ac_coupling(samples: np.ndarray) -> np.ndarray:
    """
    Remove DC offset mathematically.
    This is the equivalent of a capacitor in the analog domain.
    Subtracting the mean removes the 0 Hz component entirely.
    """
    return samples - np.mean(samples)

def apply_window(samples: np.ndarray) -> np.ndarray:
    """
    Apply a Hann window to reduce spectral leakage.
    Without this, sharp edges at buffer boundaries create
    fake frequency components across the entire spectrum.
    """
    window = np.hanning(len(samples))
    return samples * window

def compute_fft(samples: np.ndarray, sample_rate: int) -> tuple[np.ndarray, np.ndarray]:
    """
    Compute magnitude spectrum in dB.
    Returns (frequencies_array, magnitudes_in_dB).
    """
    N = len(samples)
    
    # FFT — complex output
    spectrum = np.fft.rfft(samples, n=FFT_SIZE)
    
    # Magnitude (absolute value of complex number)
    magnitude = np.abs(spectrum)
    
    # Normalize by FFT size to make amplitude independent of buffer size
    magnitude = magnitude / N
    
    # Convert to decibels (log scale matches how ears perceive loudness)
    # Add small epsilon to prevent log(0) = -infinity
    magnitude_db = 20 * np.log10(magnitude + 1e-9)
    
    # Frequency axis: rfft gives N/2 + 1 bins
    frequencies = np.fft.rfftfreq(FFT_SIZE, d=1.0/sample_rate)
    
    return frequencies, magnitude_db

def setup_plot():
    """Initialize the matplotlib figure with two subplots."""
    global fig, line_time, line_freq
    
    fig, (ax_time, ax_freq) = plt.subplots(2, 1, figsize=(12, 8))
    fig.patch.set_facecolor('#1a1a2e')
    
    # --- Time Domain Plot ---
    ax_time.set_facecolor('#16213e')
    ax_time.set_title('AirTone — Time Domain (Waveform)', color='white', fontsize=13)
    ax_time.set_xlabel('Sample Index', color='#aaaaaa')
    ax_time.set_ylabel('Amplitude', color='#aaaaaa')
    ax_time.set_ylim(-35000, 35000)  # int16 range with headroom
    ax_time.tick_params(colors='#aaaaaa')
    ax_time.spines['bottom'].set_color('#444444')
    ax_time.spines['left'].set_color('#444444')
    ax_time.grid(True, alpha=0.2, color='#444444')
    line_time, = ax_time.plot([], [], color='#00d4ff', linewidth=1.2, antialiased=True)
    
    # --- Frequency Domain Plot ---
    ax_freq.set_facecolor('#16213e')
    ax_freq.set_title('AirTone — Frequency Domain (Spectrum)', color='white', fontsize=13)
    ax_freq.set_xlabel('Frequency (Hz)', color='#aaaaaa')
    ax_freq.set_ylabel('Magnitude (dB)', color='#aaaaaa')
    ax_freq.set_xlim(20, 8000)   # Human hearing range for guitar
    ax_freq.set_ylim(-80, 0)     # dB range
    ax_freq.set_xscale('log')    # Log scale makes musical intervals equal-spaced
    ax_freq.tick_params(colors='#aaaaaa')
    ax_freq.spines['bottom'].set_color('#444444')
    ax_freq.spines['left'].set_color('#444444')
    ax_freq.grid(True, alpha=0.2, color='#444444', which='both')
    line_freq, = ax_freq.plot([], [], color='#ff6b35', linewidth=1.5, antialiased=True)
    
    # Add guitar note reference lines
    guitar_notes = {
        'E2': 82.4, 'A2': 110, 'D3': 146.8, 
        'G3': 196, 'B3': 246.9, 'E4': 329.6
    }
    for note, freq in guitar_notes.items():
        ax_freq.axvline(x=freq, color='#44ff88', alpha=0.4, linewidth=0.8, linestyle='--')
        ax_freq.text(freq, -5, note, color='#44ff88', fontsize=7, ha='center', alpha=0.7)
    
    plt.tight_layout(pad=2.0)
    return fig, ax_time, ax_freq

def detect_port() -> str:
    """
    Find the first likely serial port. Checks real USB/ACM devices first,
    then common device names for the current platform.
    Returns the port name, or None if nothing found.
    """
    candidates = (p.device for p in serial.tools.list_ports.comports())
    preferred = [d for d in candidates
                 if 'USB' in d.upper() or 'ACM' in d.upper() or 'COM' in d.upper()]
    if preferred:
        return preferred[0]

    if os.name == 'nt':  # Windows
        fallbacks = [f'COM{i}' for i in range(1, 32)]
    elif sys.platform.startswith('linux'):
        fallbacks = ['/dev/ttyUSB0', '/dev/ttyACM0']
    elif sys.platform == 'darwin':  # macOS
        fallbacks = ['/dev/cu.usbserial', '/dev/cu.usbmodem']
    else:
        fallbacks = []

    for port in fallbacks:
        if os.path.exists(port):
            return port
    return None

def run_analyzer():
    """Main entry point — connect to serial and start live visualization."""
    fig, ax_time, ax_freq = setup_plot()

    port = PORT or detect_port()
    if port is None:
        print("[ERROR] No serial port found. Run Python without the venv "
              "active, or plug in your device (e.g. /dev/ttyUSB0).")
        print("  → Tip: try `ls /dev/ttyUSB* /dev/ttyACM*` to list available ports")
        return

    try:
        ser = serial.Serial(port, BAUD, timeout=0.1)
        print(f"[AirTone Analyzer] Connected to {port} at {BAUD} baud")
        print(f"[AirTone Analyzer] Nyquist limit: {SAMPLE_RATE // 2} Hz — full guitar range visible!")
    except serial.SerialException as e:
        print(f"[ERROR] Cannot open serial port: {e}")
        print(f"  → Check that {port} is correct and not open in another program")
        return

    def animate(_frame):
        """Called by matplotlib ~30 times per second to update the plot."""
        # Drain everything available in the serial buffer right now
        lines_processed = 0
        while ser.in_waiting > 0 and lines_processed < 10:
            try:
                raw = ser.readline().decode('utf-8', errors='ignore').strip()
                samples = parse_burst(raw)
                if samples:
                    sample_buffer.extend(samples)
                lines_processed += 1
            except Exception:
                break
        
        if len(sample_buffer) < BUFFER_SIZE:
            return line_time, line_freq  # Not enough data yet
        
        # Convert buffer to numpy array
        data = np.array(list(sample_buffer), dtype=np.float32)
        
        # Update time domain plot
        line_time.set_data(np.arange(len(data)), data)
        ax_time.set_xlim(0, len(data))
        
        # Process frequency domain
        data_ac = apply_ac_coupling(data)
        data_windowed = apply_window(data_ac)
        freqs, magnitude_db = compute_fft(data_windowed, SAMPLE_RATE)
        
        # Only plot frequencies above 20 Hz (ignore remaining DC residue)
        mask = freqs >= 20
        line_freq.set_data(freqs[mask], magnitude_db[mask])
        
        return line_time, line_freq

    ani = animation.FuncAnimation(
        fig, 
        animate, 
        interval=33,         # ~30 FPS
        blit=True,           # Only redraw changed artists (faster)
        cache_frame_data=False
    )
    
    plt.show()
    ser.close()
    print("[AirTone Analyzer] Disconnected.")

if __name__ == '__main__':
    run_analyzer()