#include "test_synth_comms.h"

#include "waveform.h"
#include "voice_manager.h"
#include "volume_oscillation.h"
#include "sine_lookup.h"
#include "audio_out.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

void test_volume_oscillation();
void test_adsr();

int main(void){
  sine_lookup_init();
  const uint8_t run_test_suite = 0;
  
  if(run_test_suite){
    printf("Unit tests using C asserts. All test passes if no assert message appears.\n");
    printf("Testing synth_comms.h...\n");
    test_process_keys();
    test_volume_oscillation();
    //test_oscillator();
    test_adsr();
    return 0;
  }

  //Loop to csv for visualisation
  FILE *csv = fopen("waveform.csv", "w");
  if (csv == NULL) {
    perror("waveform.csv");
    return EXIT_FAILURE;
  }

  WaveformConfig sine_waveform = {
    .rise_pct = 0.250f,
    .fall_pct = 0.50f,
    .rise_shape = WAVE_SINE,
    .fall_shape = WAVE_SINE,
    .max_output = 350.0f,
    .midpoint = 0.50f
  };

  fprintf(csv, "sample_index,output\n");
  float phase = 0.0f;
  for (uint16_t i = 0; i < 12000; i++) {
    const float output = (float)waveform_get_point(phase, &sine_waveform);
    fprintf(csv, "%u,%.8f\n", (unsigned)i, output);
    phase += 100.0/12000.0f;
    if(phase >= 1.0f) phase -= 1.0f;
  }

  if (fclose(csv) != 0) {
    perror("waveform.csv");
    return EXIT_FAILURE;
  }

  printf("Wrote 12000 samples to waveform.csv\n");
  
  return 0;
}
