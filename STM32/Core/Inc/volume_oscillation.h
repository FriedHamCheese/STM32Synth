#ifndef VOLUME_OSCILLATOR_H
#define VOLUME_OSCILLATOR_H

/**
  Oscillate volume based on frequency and strength using sine wave and keep peak of wave at max volume.
  Strength dictates how low the volume would go and is half of the wave amplitude (peak-peak = strength).
  The function will keep the peak of the wave to be at 1.0, meaning the wave is elevated up by (1.0-strength).

  Examples:
  - A 1.0 strength with 1hz frequency means: 
      starting with 50% volume, 100% at 250ms, 50% at 500ms, 0% at 750ms and back to start, and repeat
  - A 0.5 stength with 2hz frequency means:
      starting with 75% volume, 100% volume at 125ms, 75% at 250ms, 50% at 375ms, and back to start and repeat
*/
typedef struct {
  //Should be 0.10-10s
  float frequency;
  float completion;
  float strength;
} VolumeOscillationParam;

//Using inverse of sample rate to avoid 14-cycle float division instruction
void get_oscillation_completion(VolumeOscillationParam *voscp, float sample_period);
float get_output_amplitude_multiplier(const VolumeOscillationParam *voscp);

/* Tempo-synced LFO rate: pot picks a note division (1/16 .. 4 cycles per beat),
   returned in Hz for the tapped BPM. 0.0f when bpm is not positive. */
float vosc_beat_sync_hz(float bpm, float pot);

/* Note division nearest to hz at bpm, e.g. "1/8"; NULL when bpm/hz invalid. */
const char *vosc_beat_division_label(float bpm, float hz);

#endif