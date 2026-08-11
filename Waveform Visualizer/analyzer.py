import serial
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation

# --- CONFIGURATION ---
SERIAL_PORT = '/dev/ttyUSB0'  # Change to your ESP32 port
BAUD_RATE = 115200
SAMPLE_RATE = 16000           # Match your I2S sample rate
CHUNK_SIZE = 1024             # Number of samples to collect before drawing

# Initialize serial connection
try:
    ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
except Exception as e:
    print(f"Failed to open port {SERIAL_PORT}: {e}")
    exit()

# Setup matplotlib figure
fig, ax = plt.subplots(figsize=(10, 5))
x_data = np.fft.rfftfreq(CHUNK_SIZE, d=1.0/SAMPLE_RATE)
line, = ax.plot(x_data, np.zeros(len(x_data)), color='blue')

ax.set_title("Live Audio Frequency Spectrum")
ax.set_xlabel("Frequency (Hz)")
ax.set_ylabel("Amplitude")
ax.set_xlim(0, 8000)  # Nyquist limit for 16kHz is 8000Hz
ax.set_ylim(0, 100000) # Adjust this if the wave is too tall or short

audio_buffer = []

def update(frame):
    global audio_buffer
    
    # Read lines from serial until we have enough for a chunk
    while ser.in_waiting:
        try:
            line_bytes = ser.readline().decode('utf-8').strip()
            # Hunt for our specific Teleplot prefix
            if line_bytes.startswith('>WiredAudio:'):
                value = int(line_bytes.split(':')[1])
                audio_buffer.append(value)
        except Exception:
            pass # Ignore corrupted serial lines
            
    # Once we have enough samples, run the FFT
    if len(audio_buffer) >= CHUNK_SIZE:
        # Convert to numpy array and grab the latest chunk
        data = np.array(audio_buffer[-CHUNK_SIZE:])
        audio_buffer = [] # clear buffer
        
        # Apply a Hanning window to prevent frequency bleeding
        windowed_data = data * np.hanning(CHUNK_SIZE)
        
        # Calculate FFT (absolute value to get magnitudes)
        fft_result = np.abs(np.fft.rfft(windowed_data))
        
        # Update the graph line
        line.set_ydata(fft_result)
        
    return line,

# Run the animation loop
ani = FuncAnimation(fig, update, interval=50, blit=True)
plt.show()

ser.close()