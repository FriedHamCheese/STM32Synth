# How to develop for the STM32F401 Blackpill
## For first time
- Run .ioc file with CubeMX, this generates additional required files for building
## If CubeMX initialised project directory
- Build the project in CubeIDE, no need to run, use the hammer icon
- Use CubeProgrammer with USB mode
- Put the board into DFU mode by holding BOOT, press NRST and release BOOT after
- Refresh CubeProgrammer's device detection
- Open the .elf output from CubeIDE for CubeProgrammer in /Debug
- Download onto the board
- Press NRST to switch back to normal MCU operation

## Notes (for some reason)
- If CubeProgrammer or the operating system cannot recognise the board in DFU mode,
  try unplugging computer from wall power, noise?
