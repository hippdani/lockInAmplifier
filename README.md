# lockInAmplifier
## Mirocontroller based Lock In Amplifier with USB interface

This project aims to create a Lock In Amplifier that relies on a microcontroller to do the "mixing", which means the microcontroller multiplies the input signal with a sine and cosine signal. The two resulting signals are then digitally low pass filtered and can be either sent via USB or can be converted back to an analog signal using DACs.

The software is split into four parts:
- the Arduino code for the Microcontroller, which is available in a version that runs on the RP2040, specifically the PiPico but many other RP2040 boards will be compatible.
- the (yet to be made) Arduino code for ESP32 boards (specifically WROOM-32??? with the faster 32bit FPU and dual core capabilities)
- The API to interface with the Microcontroller from a PC (Python)
- The GUI that runs on the PC to set the parameters of the Lock In Amplifier and display the measured signal. This serves also as example on how to use the API (Python)
(API may be the wrong word for the module / package / function / skript????? i created, but it does kinda the same thing)

Additionally the hardware will be described, when it is beyond the state of a chaotic breadboard.

**TODOS:**
-Fix the fixed point IIR low pass filter in the RP2040 code or port it to an floating point IIR for the ESP32
