# STM32Synth - Synthesizer/Keyboard Firmware for STM32F4RCT6
## Features
### Audio Synthesis
- [x] Support playback of 9 concurrent voices with 10-20ms input delay and a real-time screen showing audio parameters
  - Runs on a single master module 84MHz STM32F4RCT6
  - 3 user-code interrupt levels
    - TIM4: 12khz sampling interrupt, 9 voices put the CPU at 40% load, calculates a single sample every 83us
    - TIM3: 10ms non-blocking runtime interrupt, handles I2C polling, potentiometer and button reading, and autoplay updating
    - int main: blocking SPI screen transfers
- [x] True analog audio output via 3.5mm jack
  - [x] converting PWM into analog voltage via RC circuit, 50% duty cycle is 0V, 100% is +1.65V, 0 is -1.65V, etc...
  - [x] unity-gain amplifier to not cause voltage divider between output load and RC circuit
- [x] Customizable waveform rise/fall shapes (sine/triangle/square) and percentage, and waveform midpoint (PWM)
- [x] Per-key Attack-Delay-Sustain-Release calculation
- [x] Volume oscillation for tremolo effect on mixed sample, adjustable frequency and depth
- [x] Metronome BPM counter and tapper with volume oscillation integration
- [x] Autoplay feature, programmable keypress cycles to play songs without input
- [x] 320x240 screen showing parameters of audio synthesis
### Physical
- [x] Support for hot-swapping slave (octave modules) together
  - requires the master module (the one with screen) for audio calculation and playback
  - Inter-module communication done through I2C
  - [x] Can add up to 3 slaves in any combinations
    - [x] each with DIP switch to switch octaves on the fly
- [x] Velocity-based keypress detection, affecting loudness of each key
- [x] Runs on single 5V USB-C power on master 

<img width="1258" height="947" alt="image" src="https://github.com/user-attachments/assets/ca5b9a67-c4c9-47d8-ab78-c7bb3189943d" />
<img width="1005" height="756" alt="image" src="https://github.com/user-attachments/assets/297d00a7-3518-469a-a391-465e13b8fdbd" />
<img width="1008" height="756" alt="IMG_1105" src="https://github.com/user-attachments/assets/92f8fac1-66b8-4984-aaac-7d9f2a61bea1" />
<img width="1008" height="756" alt="IMG_1108" src="https://github.com/user-attachments/assets/723845f9-a482-4422-9102-3a5ad73f2499" />
<img width="1008" height="756" alt="IMG_1112" src="https://github.com/user-attachments/assets/d4fee62d-8b2b-4949-858b-ceeae97582f9" />

## Legacy
### How to develop for the STM32F401 Blackpill
### For first time
- Run .ioc file with CubeMX, this generates additional required files for building
### If CubeMX initialised project directory
- Build the project in CubeIDE, no need to run, use the hammer icon
- Use CubeProgrammer with USB mode
- Put the board into DFU mode by holding BOOT, press NRST and release BOOT after
- Refresh CubeProgrammer's device detection
- Open the .elf output from CubeIDE for CubeProgrammer in /Debug
- Download onto the board
- Press NRST to switch back to normal MCU operation

### Notes (for some reason)
- If CubeProgrammer or the operating system cannot recognise the board in DFU mode,
  try unplugging computer from wall power, noise?
