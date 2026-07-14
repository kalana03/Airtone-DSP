# Week 1

### Wiring the DAC Module to the ESP 32

Although the plan was to initially build the transmitter module, I had to start with the receiver module because I haven't got the ADC Module (PCM 1808) yet. 

Starting to build the receiver, the major challenge I faced is the wiring of the DAC module (PCM 5102) to the ESP-32. I initally wired the primary connectors to the ESP 32 **(VIn, GND, LRCK, DIn, BCK, SCK)** but nothing could be heard. On further research, I found out that there are function jumpers (pins) that needs a configuration to give a desirable output. Among those pins XSMT is a mute pin. The audio is emitted only when it is given a voltage. 

[PCM 1502 resource page](https://www.makerguides.com/playing-audio-with-esp32-and-pcm5102/#Architecture_of_a_PCM5102A_based_Audio_System)

A static frequency was heard through the connected speaker.

Then I used the **_BluetoothA2DPSink_** library on Arduino and modified the code to connect the ESP 32 with my phone, and play the music transmitted via bluetooth. This was to check the quality of the sound emitted after the Digital-to-Analog conversion. The quality and dynamics was relatively good.

<img width="506" height="400" alt="b7391207-2737-4038-b04f-c5c11867c858" src="https://github.com/user-attachments/assets/d0385586-f945-43f3-8bce-9a7f82f2142a" />
<img width="281" height="400" alt="1a4790d5-fb22-4373-98ff-8e769ceddddd" src="https://github.com/user-attachments/assets/9cca1588-d906-453b-9831-66e864af34d3" />


