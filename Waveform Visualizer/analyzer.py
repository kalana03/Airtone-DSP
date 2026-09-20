import serial
import matplotlib.pyplot as plt

# 🚨 Ensure this matches your Linux port!
SERIAL_PORT = '/dev/ttyUSB0' 
BAUD_RATE = 115200

raw_wave = []
processed_wave = []
dsp_exec_time = 0

print(f"🎧 Listening on {SERIAL_PORT}... Play your guitar!")

try:
    with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
        while True:
            line = ser.readline().decode('utf-8', errors='ignore').strip()
            
            if line == "START_PLOT":
                print("📸 Snapshot received! Generating graph...")
                raw_wave.clear()
                processed_wave.clear()
                
                # ⏱️ Read the execution time first
                time_line = ser.readline().decode('utf-8', errors='ignore').strip()
                if time_line.startswith("TIME:"):
                    try:
                        dsp_exec_time = int(time_line.split(":")[1])
                    except ValueError:
                        dsp_exec_time = 0
                
                # Read the 160 audio samples
                for _ in range(160):
                    data = ser.readline().decode('utf-8', errors='ignore').strip()
                    if data == "END_PLOT":
                        break
                    try:
                        raw, processed = map(float, data.split(','))
                        raw_wave.append(raw)
                        processed_wave.append(processed)
                    except ValueError:
                        pass
                
                break # Exit loop to plot the data

except serial.SerialException:
    print("❌ Could not open serial port. Is the PlatformIO Serial Monitor closed?")
    exit()

# 🧮 Calculate actual time in milliseconds for the X-axis
# At 16000 Hz, each sample is 1/16000 seconds (0.0625 milliseconds)
time_axis_ms = [i * (1000.0 / 16000.0) for i in range(len(raw_wave))]

# 📊 Generate the Publication-Ready Plot
plt.figure(figsize=(10, 5))

# Make the blue line thicker
plt.plot(time_axis_ms, raw_wave, label='Original Waveform (Raw Input)', color='blue', alpha=0.8, linewidth=4)

# Make the red line dashed so we can see through it (The "X-Ray" fix)
plt.plot(time_axis_ms, processed_wave, label='Modified Waveform (DSP Output)', color='red', alpha=1.0, linewidth=2, linestyle='--')

# Add the execution time dynamically to the title!
plt.title(f'Real-Time DSP Signal Processing (Execution Time: {dsp_exec_time} μs)', fontsize=14, fontweight='bold')
plt.xlabel('Time (milliseconds)', fontsize=12)
plt.ylabel('Amplitude', fontsize=12)
plt.axhline(0, color='black', linewidth=0.8, linestyle='--')

# Set the X-axis limits perfectly to the 10ms frame
plt.xlim(0, 10) 

plt.legend(loc='upper right')
plt.grid(True, linestyle=':', alpha=0.6)
plt.tight_layout()

plt.show()