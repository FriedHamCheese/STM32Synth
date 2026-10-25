#include "volume_oscillation.h"
#include "sine_lookup.h"
#include "math.h"

//Using inverse of sample rate to avoid 14-cycle float division instruction
void get_oscillation_completion(VolumeOscillationParam *voscp, float sample_period){
  const float oscillation_increment = voscp->frequency * sample_period;
  voscp->completion += oscillation_increment;
  if(voscp->completion >= 1.0f) 
	  voscp->completion -= 1.0f;
}

float get_output_amplitude_multiplier(const VolumeOscillationParam *voscp){
  if(voscp->strength == 0.0f) return 1.0f;
  
  const float sin_max_as_1 = 0.5f;
  const float sin_min_as_0 = 1.0f;
  const float multiplier = (sine_lookup(voscp->completion) + sin_min_as_0) * voscp->strength * sin_max_as_1;
  const float max_amplitude_at_1 = 1.0f - voscp->strength;
  
  return multiplier + max_amplitude_at_1;
}

/* Pot 0..1 walks these in order, slowest to fastest. */
static const float VOSC_BEAT_DIVISIONS[] = {
  1.0f/16.0f, 1.0f/8.0f, 1.0f/4.0f, 1.0f/2.0f, 1.0f, 2.0f, 4.0f
};
static const char *const VOSC_BEAT_DIVISION_LABELS[] = {
  "1/16", "1/8", "1/4", "1/2", "1", "2", "4"
};
#define VOSC_BEAT_DIVISION_COUNT \
  (sizeof(VOSC_BEAT_DIVISIONS) / sizeof(VOSC_BEAT_DIVISIONS[0]))

static unsigned beat_division_index(float pot)
{
  if (!(pot > 0.0f)) pot = 0.0f;
  if (pot > 1.0f) pot = 1.0f;

  const unsigned last = (unsigned)VOSC_BEAT_DIVISION_COUNT - 1U;
  unsigned index = (unsigned)(pot * (float)(last + 1U));
  if (index > last) index = last;
  return index;
}

float vosc_beat_sync_hz(float bpm, float pot)
{
  if (!(bpm > 0.0f))
    return 0.0f;

  return bpm / 60.0f * VOSC_BEAT_DIVISIONS[beat_division_index(pot)];
}

const char *vosc_beat_division_label(float bpm, float hz)
{
  if (!(bpm > 0.0f) || !(hz > 0.0f))
    return 0;

  const float cycles_per_beat = hz * 60.0f / bpm;
  unsigned best = 0;
  float best_diff = fabsf(cycles_per_beat - VOSC_BEAT_DIVISIONS[0]);

  for (unsigned i = 1; i < VOSC_BEAT_DIVISION_COUNT; i++) {
    const float diff = fabsf(cycles_per_beat - VOSC_BEAT_DIVISIONS[i]);
    if (diff < best_diff) {
      best_diff = diff;
      best = i;
    }
  }
  return VOSC_BEAT_DIVISION_LABELS[best];
}