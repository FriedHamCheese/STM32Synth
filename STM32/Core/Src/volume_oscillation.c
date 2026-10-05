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