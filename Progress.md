# Week 1

### Wiring the DAC Module to the ESP 32

Although the plan was to initially build the transmitter module, I had to start with the receiver module because I haven't got the ADC Module (PCM 1808) yet. 

Starting to build the receiver, the major challenge I faced is the wiring of the DAC module (PCM 5102) to the ESP-32. I initally wired the primary connectors to the ESP 32 **(VIn, GND, LRCK, DIn, BCK, SCK)** but nothing could be heard. On further research, I found out that there are function jumpers (pins) that needs a configuration to give a desirable output. Among those pins XSMT is a mute pin. The audio is emitted only when it is given a voltage. 

[PCM 1502 resource page](https://www.makerguides.com/playing-audio-with-esp32-and-pcm5102/#Architecture_of_a_PCM5102A_based_Audio_System)

A static frequency was heard through the connected speaker.

Then I used the **_BluetoothA2DPSink_** library on Arduino and modified the code to connect the ESP 32 with my phone, and play the music transmitted via bluetooth. This was to check the quality of the sound emitted after the Digital-to-Analog conversion. The quality and dynamics was relatively good.

<img width="506" height="400" alt="b7391207-2737-4038-b04f-c5c11867c858" src="https://github.com/user-attachments/assets/d0385586-f945-43f3-8bce-9a7f82f2142a" />
<img width="281" height="400" alt="1a4790d5-fb22-4373-98ff-8e769ceddddd" src="https://github.com/user-attachments/assets/9cca1588-d906-453b-9831-66e864af34d3" />

# Week 2

### Getting a signal wave from the peizoelectric transceiver

I needed to test out the input signal waveform of the peizoelectric sensor alone, before integrating it to the ADC module. I checked the signal wave using Teleplot (VS Code extention) which displays the digital values as a analog signal. I wanted to check whether there is a spike in the waveform when I play the instrument desplite it being in the soundhole with other vibrations present. The waveform turned out to be useable.

The peizoelectric sensor had to be connected thorugh a resistor/capacitor bridge to match the scope of the sensor and the ESP input. The sensor captures the signal in a range of -2048 - 2048, and the ADC input in the ESP takes in the range of 0 - 4096. If the gave the raw signal, the processed signal will only contain the positive parts of the wave, and also will not use the full scope. Using the resistor/capacitor bridge, the signal was superpositioned up, half of the scope so that the lowest possible value in the sensor was equal to 0, and the maximum possible was equal to 4096. 

<img width="400" height="300" alt="image" src="https://github.com/user-attachments/assets/abd447ad-0235-4579-a034-4303812b1b86" />
<img width="535" height="300" alt="image" src="https://github.com/user-attachments/assets/35c319f2-9d28-4c2b-be7a-0f5e0365b64d" />

### Resources on signal processing

I was introduced to several important concepts of signal processing, like aliasing, SNR, filters, Z-transform etc, where I was able to get a broader idea of the internal processes of my project and how those affect my output. Also I was introduced to some DSP resoureces I can use in my project.

[Audio Eq Cookbook](https://www.w3.org/TR/audio-eq-cookbook/) - Contains math functions of most used aspects in signal processing. 
MATLAB - to simulate the signal and view the results visually. 

I also found a C++ library which was implemented with DSP functions focused on music - Daisy SP.




