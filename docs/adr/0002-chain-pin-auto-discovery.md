# ADR 0002 — CHAIN Pin Auto-Discovery

**Status:** Accepted (pending V2 PCB revision)  
**Date:** 2026-09-14

## Context

The master board can be placed at either end of the chain.
Slaves need unique I2C addresses and correct octave offsets.
Both depend on physical position in the chain, which is not knowable
from software alone without extra information at boot.

## Decision

Repurpose pogo connector pin 5 from GND to a dedicated CHAIN GPIO
(V2 PCB revision). J1 pin 5 = CHAIN_IN, J2 pin 5 = CHAIN_OUT on each board.

At boot, a Discovery protocol propagates down the chain via CHAIN pulses.
Each slave records its position from the pulse sequence and receives
its I2C address and octave offset from the master over I2C.

Master detects its own end-position by reading both CHAIN_IN pins
before sending grants: the side with no neighbor reads LOW (pull-down).

No compile-time constant encodes master position or slave addresses.

## Alternatives considered

**Compile-time SLAVE_ID define:** One binary per slave, address hardcoded.
Simple to implement. Breaks "any board, any position" goal. Requires
re-flashing if boards are reordered. Rejected.

**I2C address strap (GPIO jumper):** Solder jumper sets address at manufacturing.
One firmware binary. No PCB cost beyond a pad. Does not give the master
positional awareness for octave assignment without a secondary protocol.
Rejected as insufficient for octave correctness.

**DIP switch:** Considered for a future revision to allow `IS_MASTER` to be
set without reflashing. Not implemented in V1/V2. Noted as future work.

## Consequences

- Requires V2 PCB revision (J1/J2 pin 5 net change: GND → GPIO).
  One GND pin remains on each connector — electrically sufficient.
- V1 board can still be used for all other testing; CHAIN feature tests on V2.
- All boards ship identical firmware binaries (except IS_MASTER flag).
- Discovery adds ~50–100ms to boot time. Acceptable.
- Slave count is detected at runtime — no max-slaves constant in firmware
  (address range 0x20–0x26 sets the practical ceiling of 7 slaves).
