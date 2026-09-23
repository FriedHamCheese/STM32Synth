# ADR 0001 — Slave-Side Event Computation

**Status:** Accepted  
**Date:** 2026-09-14

## Context

Each slave board reads 12 hall-effect sensors via an analog mux.
The raw data must become Key Events (key-on/off, velocity, pressure)
before the master's synthesis engine can use it.

The computation can happen on the slave or on the master.

## Decision

Slaves compute. Each slave converts raw ADC readings into Key Events
locally and buffers them for the master to collect on the next poll.

The master receives only Key Events — never raw ADC values.

## Alternatives considered

**Master computes:** Slave sends raw ADC (24 bytes per slave per poll).
Master converts to events centrally. Simpler slave firmware. All tuning in one place.
Ruled out: at 400kHz Fast Mode I2C, 7 slaves × 24 bytes × 200Hz ≈ 324kbps — exceeds
the effective ceiling. Does not scale beyond 4–5 slaves.

## Consequences

- I2C bandwidth is proportional to key activity, not slave count. Scales to any N.
- Slave firmware must implement calibration, threshold detection, velocity, and pressure.
- Bento (slave firmware author) implements to a spec owned by Nuker (this module).
- Tuning parameters (thresholds, velocity curve) must be communicated to slave firmware.
