#include "volume_oscillation.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define VOL_OSC_TEST_POINTS 8
#define VOL_OSC_FLOAT_TEST_TOLERANCE 0.01f

static uint8_t float_within_tolerance(float val, float target, float tolerance){
  return fabs(val - target) <= tolerance;
}

static void test_volume_oscillation_strength(float strength, const float expected[]){
  VolumeOscillationParam voscp = {
    .frequency = 1.0f,
    .completion = 0.0f,
    .strength = strength
  };
  const float sample_period = 1.0f/VOL_OSC_TEST_POINTS;

  for (int i = 0; i < VOL_OSC_TEST_POINTS; i++){
    assert(float_within_tolerance(
      get_output_amplitude_multiplier(&voscp), expected[i], VOL_OSC_FLOAT_TEST_TOLERANCE));
    get_oscillation_completion(&voscp, sample_period);
  }
}

static void test_volume_oscillation_no_strength(){
  printf("\ttest_volume_oscillation_no_strength()...\n");
  const float expected[VOL_OSC_TEST_POINTS] = {
    1.00f, 1.00f, 1.00f, 1.00f,
    1.00f, 1.00f, 1.00f, 1.00f
  };
  test_volume_oscillation_strength(0.0f, expected);
}

static void test_volume_oscillation_quarter_strength(){
  printf("\ttest_volume_oscillation_quarter_strength()...\n");
  const float expected[VOL_OSC_TEST_POINTS] = {
    0.87500f, 0.96339f, 1.00000f, 0.96339f,
    0.87500f, 0.78661f, 0.75000f, 0.78661f
  };
  test_volume_oscillation_strength(0.25f, expected);
}

static void test_volume_oscillation_half_strength(){
  printf("\ttest_volume_oscillation_half_strength()...\n");
  const float expected[VOL_OSC_TEST_POINTS] = {
    0.75000f, 0.92678f, 1.00000f, 0.92678f,
    0.75000f, 0.57322f, 0.50000f, 0.57322f
  };
  test_volume_oscillation_strength(0.50f, expected);
}

static void test_volume_oscillation_max_strength(){
  printf("\ttest_volume_oscillation_max_strength()...\n");
  const float expected[VOL_OSC_TEST_POINTS] = {
    0.50000f, 0.85355f, 1.00000f, 0.85355f,
    0.50000f, 0.14645f, 0.00000f, 0.14645f
  };
  test_volume_oscillation_strength(1.0f, expected);
}

static void test_volume_oscillation_beat_sync_divisions(){
  printf("\ttest_volume_oscillation_beat_sync_divisions()...\n");
  /* 120 BPM = 2 beats/s; pot 0..1 walks 1/16 .. 4 cycles per beat */
  assert(float_within_tolerance(
    vosc_beat_sync_hz(120.0f, 0.0f), 0.125f, VOL_OSC_FLOAT_TEST_TOLERANCE));
  assert(float_within_tolerance(
    vosc_beat_sync_hz(120.0f, 0.5f), 1.0f, VOL_OSC_FLOAT_TEST_TOLERANCE));
  assert(float_within_tolerance(
    vosc_beat_sync_hz(120.0f, 1.0f), 8.0f, VOL_OSC_FLOAT_TEST_TOLERANCE));
  assert(vosc_beat_sync_hz(0.0f, 0.5f) == 0.0f);
}

static void test_beat_division_label(){
  printf("\ttest_volume_oscillation_beat_division_label()...\n");
  assert(strcmp(vosc_beat_division_label(120.0f, 1.0f), "1/2") == 0);
  assert(strcmp(vosc_beat_division_label(120.0f, 2.0f), "1") == 0);
  assert(strcmp(vosc_beat_division_label(120.0f, 8.0f), "4") == 0);
  assert(vosc_beat_division_label(0.0f, 1.0f) == NULL);
}

void test_volume_oscillation(){
  printf("test_volume_oscillation()\n");
  test_volume_oscillation_no_strength();
  test_volume_oscillation_quarter_strength();
  test_volume_oscillation_half_strength();
  test_volume_oscillation_max_strength();
  test_volume_oscillation_beat_sync_divisions();
  test_beat_division_label();
}
