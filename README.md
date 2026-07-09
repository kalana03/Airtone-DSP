# Airtone-DSP
Semester 5 ICE project - A non-invasive, ultra-low-latency digital signal processing (DSP) system that brings the electronic expressiveness of an electric guitar to a standard classical acoustic guitar, completely wirelessly.

##### Module 1 - The Soundhole Transmitter

- A Piezoelectric contact pickup reads the physical wood vibrations of the nylon strings, preventing feedback loops.
- A TL072 Op-Amp circuit provides impedance matching and a slight voltage boost so the analog signal doesn't lose its low-end frequencies.
- A PCM1808 I2S ADC converts the analog wave into a high-fidelity, 24-bit digital stream, completely isolating the audio from the microcontroller's radio noise.
- An ESP32-S3 SuperMini receives the I2S digital audio, utilizes its vector-accelerated processor to apply DSP effects (like distortion), and broadcasts the packets into the air.
- Driven by a lightweight 3.7V 900mAh LiPo battery managed by a TP4056 charging circuit.

##### Module 2 - The Base Station Receiver

- A standard ESP32 continuously listens for incoming ESP-NOW packets from the transmitter.
- 2.4GHz ESP-NOW protocol communication is used for communicating with the transmitter module.
- A PCM5102 I2S DAC takes the digital 1s and 0s and perfectly converts them back into a clean, analog audio wave.
- A standard 3.5mm or 1/4-inch jack feeds the processed sound directly into a stage amplifier or headphones.

##### Mobile App (User Control)

- Digital dashboard used to control the DSP parameters on the fly without interrupting the live performance.
- Bluetooth Low Energy / BLE communication with the transmitter module.

### Project Timeline

##### **Week 1 :** core software architecture and finalize component

##### **Week 2:** Transmitter
- Capturing high-fidelity analog sound from the acoustic guitar.
- amplification circuit
- Connect the PCM1808 ADC to the ESP32-S3, Verifying conversion is successful.

##### **Week 3:** Receiver
- Connect the ESP32 to the PCM5102 DAC
- ESP-NOW Handshake
- Verify the clean, unprocessed audio

##### **Week 4:** Mobile App UI & BLE Integration

##### **Week 5:** Core DSP math functions
- physical push-button and write the debounce logic to toggle the effects on and off manually.
