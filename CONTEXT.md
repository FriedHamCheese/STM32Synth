# Context — MCU Synth (Modular Piano)

Glossary for the STM32F4 modular synthesizer project.
No implementation details here — see code and ADRs.

---

## Boards

**Master Board**
The board running the synthesis engine, TFT display, potentiometers, and mode buttons.
Exactly one per chain. Role determined at flash time (`IS_MASTER` compile flag).

**Slave Board**
A board carrying 12 hall-effect keys and an analog mux. Has no display or synthesis logic.
Reports key events to the master when polled. Any number may exist in a chain.

**Chain**
The physical linear sequence of boards connected left-to-right via pogo connectors.
Master occupies one end (left or right). Slaves fill the remaining positions.

**End Board**
A board with no neighbor on one side. The master is always an end board.
Slaves are never end boards in normal operation.

---

## Discovery

**Discovery**
The boot-time protocol by which the master determines its physical end-position
and assigns a unique I2C address and octave offset to each slave.
Runs once at power-on before normal operation begins.

**CHAIN Pin**
A dedicated GPIO on each board's left and right pogo connectors (pin 5).
Used exclusively during Discovery to propagate address grants down the chain.
Not used after Discovery completes.

**Position**
A slave's integer index within the chain, counted from the master outward.
Position 0 is the slave directly adjacent to the master.
Position is assigned during Discovery and determines I2C address and octave offset.

---

## Communication

**Poll Cycle**
One master-initiated I2C read from a single slave. Master rotates through all
discovered slaves on a fixed schedule. Slaves do not initiate communication.

**Key Event**
A discrete message produced by a slave describing a change in one key's state.
Contains: key identity, event type, velocity, and pressure.
Slaves buffer events between poll cycles.

**Event Type**
One of three values a Key Event carries:
- `KEY_ON` — key crossed the press threshold (includes velocity)
- `KEY_OFF` — key returned to rest
- `KEY_PRESSURE` — key remains pressed; pressure value has changed

---

## Key Sensing

**Baseline**
The ADC reading of a hall-effect sensor when its key is fully at rest.
Per-key, because magnet gap varies across keys.
Used to compute delta (displacement from rest).

**Calibration**
The process of measuring and storing per-key Baselines.
Performed at boot if no stored Calibration exists in flash,
or loaded from flash if a previous Calibration was saved.

**Velocity**
The rate of change of a key's ADC delta at the moment KEY_ON is detected.
Represents how fast the key was struck. Expressed 0–127.

**Pressure**
The magnitude of a key's ADC delta while the key remains held.
Represents ongoing force after initial press (aftertouch). Expressed 0–127.

---

## Music

**Octave Assignment**
The MIDI base note given to a slave by the master after Discovery.
Determines which pitches that slave's 12 keys produce.
Leftmost slave in the chain always receives the lowest base note,
regardless of which end the master occupies.

**Base Note**
The MIDI note number of key 0 on the leftmost slave. Default: 48 (C3).
Each subsequent slave's base note is 12 higher than the previous.
