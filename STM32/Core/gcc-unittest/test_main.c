#include "test_synth_comms.h"

#include "waveform.h"
#include "voice_manager.h"
#include "volume_oscillation.h"
#include "sine_lookup.h"
#include "audio_out.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void test_volume_oscillation();

int main(int argc, char **argv){
  sine_lookup_init();
  const uint8_t run_test_suite = argc > 1 && strcmp(argv[1], "--test") == 0;
  
  if(run_test_suite){
    printf("Unit tests using C asserts. All test passes if no assert message appears.\n");
    printf("Testing synth_comms.h...\n");
    test_process_keys();
    test_volume_oscillation();
    return 0;
  }


  //Loop to csv for visualisation
  FILE *csv = fopen("waveform.csv", "w");
  if (csv == NULL) {
    perror("waveform.csv");
    return EXIT_FAILURE;
  }

  fprintf(csv, "sample_index,output\n");
  
  VolumeOscillationParam voscp = {
  .frequency = 10.0f,
  .completion = 0.0f,
  .strength = 0.25f
  };
  
  /*
  WaveformConfig wc = {
    .rise_pct = 0.00f,
    .fall_pct = 0.00f,
    .rise_shape = WAVE_SINE,
    .fall_shape = WAVE_SINE,
    .max_output = 350.0f
  };
  
  VoiceManager vm;
  voice_manager_init(&vm, 48000, 48000);
  voice_manager_note_on(&vm, 0, 255);
  */
  
  for(uint16_t i = 0; i < 48000; i++){
  get_oscillation_completion(&voscp, AUDIO_SAMPLE_PERIOD);
  fprintf(csv, "%u,%.2f\n", (unsigned)i,
    get_output_amplitude_multiplier(&voscp));
  }

  if (fclose(csv) != 0) {
    perror("waveform.csv");
    return EXIT_FAILURE;
  }

  printf("Wrote 48000 samples to waveform.csv\n");
  
  return 0;
}
