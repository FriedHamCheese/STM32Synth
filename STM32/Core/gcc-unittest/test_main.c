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
void test_adsr();
void test_waveform();

static void write_waveform_csv(void)
{
  FILE *csv = fopen("waveform.csv", "w");
  if (csv == NULL) {
    perror("waveform.csv");
    exit(EXIT_FAILURE);
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
    exit(EXIT_FAILURE);
  }

  printf("Wrote 12000 samples to waveform.csv\n");
}

int main(int argc, char **argv)
{
  sine_lookup_init();

  if (argc > 1 && strcmp(argv[1], "waveform") == 0) {
    write_waveform_csv();
    return EXIT_SUCCESS;
  }

  if (argc > 1 && strcmp(argv[1], "comms") == 0) {
    printf("Testing synth_comms.h...\n");
    test_process_keys();
    return EXIT_SUCCESS;
  }

  printf("Unit tests using C asserts. All test passes if no assert message appears.\n");
  test_volume_oscillation();
  test_waveform();
  test_adsr();
  return EXIT_SUCCESS;
}
