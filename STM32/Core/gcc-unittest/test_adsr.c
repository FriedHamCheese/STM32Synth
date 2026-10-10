#include "adsr.h"
#include "voice_manager.h"
#include "sine_lookup.h"
#include "audio_out.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

//One step of slack for stage changes landing a sample early/late from float rounding
#define ADSR_FLOAT_TEST_TOLERANCE 0.005f
#define SAMPLES_PER_MS 48

//Attack 10ms, decay 20ms, sustain 0.5, release 40ms
static AdsrParam test_param;

static uint8_t float_within_tolerance(float val, float target){
  return fabs(val - target) <= ADSR_FLOAT_TEST_TOLERANCE;
}

//Runs the envelope like the 48kHz ISR does, returns the last volume
static float run_samples(AdsrState *state, const AdsrParam *param, uint32_t samples){
  float volume = state->volume;
  for (uint32_t i = 0; i < samples; i++)
    volume = adsr_get_volume(state, param);
  return volume;
}

static float run_ms(AdsrState *state, const AdsrParam *param, uint32_t ms){
  return run_samples(state, param, ms * SAMPLES_PER_MS);
}

static void test_adsr_full_envelope(){
  printf("\ttest_adsr_full_envelope()...\n");
  AdsrState state;
  adsr_init(&state);
  assert(float_within_tolerance(run_ms(&state, &test_param, 5), 0.0f));

  adsr_note_on(&state);
  assert(float_within_tolerance(run_ms(&state, &test_param, 5), 0.5f));   //attack halfway
  assert(float_within_tolerance(run_ms(&state, &test_param, 5), 1.0f));   //attack done
  assert(float_within_tolerance(run_ms(&state, &test_param, 5), 0.75f));  //decay falls 1.0 per 20ms
  assert(float_within_tolerance(run_ms(&state, &test_param, 5), 0.5f));   //reached sustain after 10ms
  run_samples(&state, &test_param, 2);
  assert(state.stage == ADSR_SUSTAIN);
  assert(float_within_tolerance(run_ms(&state, &test_param, 1000), 0.5f));

  adsr_note_off(&state);
  assert(float_within_tolerance(run_ms(&state, &test_param, 10), 0.25f)); //release falls 1.0 per 40ms
  assert(adsr_is_active(&state));
  assert(float_within_tolerance(run_ms(&state, &test_param, 10), 0.0f));
  run_samples(&state, &test_param, 2);
  assert(!adsr_is_active(&state));
}

static void test_adsr_changes_every_sample(){
  printf("\ttest_adsr_changes_every_sample()...\n");
  AdsrState state;
  adsr_init(&state);

  //No staircase: volume rises on every sample of the attack, not once per ms
  adsr_note_on(&state);
  float previous = 0.0f;
  for (int i = 0; i < 10 * SAMPLES_PER_MS - 2; i++){
    const float volume = adsr_get_volume(&state, &test_param);
    assert(volume > previous);
    previous = volume;
  }
}

static void test_adsr_1ms_attack_ramps(){
  printf("\ttest_adsr_1ms_attack_ramps()...\n");
  AdsrParam param;
  adsr_set_params(&param, 1, 20, 0.5f, 40, AUDIO_SAMPLE_PERIOD);
  AdsrState state;
  adsr_init(&state);

  //Shortest pot setting still ramps over 48 samples instead of jumping 0 -> 1 (click)
  adsr_note_on(&state);
  assert(float_within_tolerance(run_samples(&state, &param, 1), 1.0f / SAMPLES_PER_MS));
  assert(float_within_tolerance(run_samples(&state, &param, 23), 0.5f));
  assert(float_within_tolerance(run_samples(&state, &param, 24), 1.0f));
}

static void test_adsr_release_before_sustain(){
  printf("\ttest_adsr_release_before_sustain()...\n");
  AdsrState state;
  adsr_init(&state);

  adsr_note_on(&state);
  assert(float_within_tolerance(run_ms(&state, &test_param, 5), 0.5f));
  //Released mid-attack, fades out from 0.5 instead of cutting off
  adsr_note_off(&state);
  assert(float_within_tolerance(run_ms(&state, &test_param, 10), 0.25f));
  assert(float_within_tolerance(run_ms(&state, &test_param, 10), 0.0f));
  run_samples(&state, &test_param, 2);
  assert(!adsr_is_active(&state));
}

static void test_adsr_retrigger_during_release(){
  printf("\ttest_adsr_retrigger_during_release()...\n");
  AdsrState state;
  adsr_init(&state);

  adsr_note_on(&state);
  assert(float_within_tolerance(run_ms(&state, &test_param, 50), 0.5f));
  adsr_note_off(&state);
  assert(float_within_tolerance(run_ms(&state, &test_param, 10), 0.25f));
  //Attack ramps from 0.25 instead of jumping to 0
  adsr_note_on(&state);
  assert(float_within_tolerance(run_samples(&state, &test_param, 1), 0.25f));
  assert(float_within_tolerance(run_ms(&state, &test_param, 5), 0.75f));
  //Reaches 1.0 after 2.5ms more, then 2.5ms of decay
  assert(float_within_tolerance(run_ms(&state, &test_param, 5), 0.875f));
}

static void test_adsr_knob_change_no_jump(){
  printf("\ttest_adsr_knob_change_no_jump()...\n");
  AdsrParam param = test_param;
  AdsrState state;
  adsr_init(&state);

  //Attack 10ms -> 100ms halfway through attack, keeps rising from 0.5 slower
  adsr_note_on(&state);
  assert(float_within_tolerance(run_ms(&state, &param, 5), 0.5f));
  adsr_set_params(&param, 100, 20, 0.5f, 40, AUDIO_SAMPLE_PERIOD);
  assert(float_within_tolerance(run_samples(&state, &param, 1), 0.5f));
  assert(float_within_tolerance(run_ms(&state, &param, 10), 0.6f));

  //Sustain 0.5 -> 0.7 while held, glides up at decay rate
  param = test_param;
  assert(float_within_tolerance(run_ms(&state, &param, 1000), 0.5f));
  adsr_set_params(&param, 10, 20, 0.7f, 40, AUDIO_SAMPLE_PERIOD);
  assert(float_within_tolerance(run_samples(&state, &param, 1), 0.5f));
  assert(float_within_tolerance(run_ms(&state, &param, 2), 0.6f));
  assert(float_within_tolerance(run_ms(&state, &param, 10), 0.7f));

  //Release 40ms -> 400ms mid-release, keeps falling from where it was, slower
  adsr_note_off(&state);
  assert(float_within_tolerance(run_ms(&state, &param, 10), 0.45f));
  adsr_set_params(&param, 10, 20, 0.7f, 400, AUDIO_SAMPLE_PERIOD);
  assert(float_within_tolerance(run_samples(&state, &param, 1), 0.45f));
  assert(float_within_tolerance(run_ms(&state, &param, 40), 0.35f));

  //Release 400ms -> 4ms, no instant cut, just falls faster
  adsr_set_params(&param, 10, 20, 0.7f, 4, AUDIO_SAMPLE_PERIOD);
  assert(float_within_tolerance(run_samples(&state, &param, 1), 0.35f - 1.0f / (4 * SAMPLES_PER_MS)));
  assert(adsr_is_active(&state));
  assert(float_within_tolerance(run_samples(&state, &param, SAMPLES_PER_MS - 1), 0.10f));
  assert(float_within_tolerance(run_ms(&state, &param, 1), 0.0f));
  assert(!adsr_is_active(&state));
}

static void test_adsr_apply_volume(){
  printf("\ttest_adsr_apply_volume()...\n");
  assert(float_within_tolerance(adsr_apply_volume(350, 1.0f), 175.0f));
  assert(float_within_tolerance(adsr_apply_volume(350, 0.5f), 87.5f));
  assert(float_within_tolerance(adsr_apply_volume(0, 0.5f), -87.5f));
  assert(float_within_tolerance(adsr_apply_volume(175, 0.3f), 0.0f));
  assert(float_within_tolerance(adsr_apply_volume(350, 0.0f), 0.0f));
  //Small volumes keep their fraction instead of rounding to a whole step
  assert(float_within_tolerance(adsr_apply_volume(350, 0.01f), 1.75f));
}

static const WaveformConfig test_waveform = {
  .rise_pct = 0.5f,
  .fall_pct = 0.5f,
  .rise_shape = WAVE_SINE,
  .fall_shape = WAVE_SINE,
  .max_output = 350.0f
};

static void run_voice_manager_ms(VoiceManager *vm, uint32_t ms){
  for (uint32_t i = 0; i < ms * SAMPLES_PER_MS; i++)
    voice_manager_get_sample(vm, &test_waveform);
}

static void test_adsr_voice_freed_after_release(){
  printf("\ttest_adsr_voice_freed_after_release()...\n");
  VoiceManager vm;
  voice_manager_init(&vm, 48000, 48000, &test_param);

  assert(voice_manager_note_on(&vm, 48, 127));
  run_voice_manager_ms(&vm, 100);
  assert(vm.active_voice_count == 1);

  voice_manager_note_off(&vm, 48);
  run_voice_manager_ms(&vm, 5);
  assert(vm.active_voice_count == 1);  //still releasing

  //Pressing the same key again reuses its releasing voice
  assert(voice_manager_note_on(&vm, 48, 127));
  run_voice_manager_ms(&vm, 1);
  assert(vm.active_voice_count == 1);

  voice_manager_note_off(&vm, 48);
  run_voice_manager_ms(&vm, 100);
  assert(vm.active_voice_count == 0);
  for (int i = 0; i < MAX_VOICES; i++)
    assert(!vm.voices[i].active);
}

static void test_adsr_null_param_is_no_envelope(){
  printf("\ttest_adsr_null_param_is_no_envelope()...\n");
  AdsrState state;
  adsr_init(&state);

  adsr_note_on(&state);
  assert(float_within_tolerance(run_samples(&state, 0, 1), 1.0f));
  assert(float_within_tolerance(run_ms(&state, 0, 100), 1.0f));
  adsr_note_off(&state);
  assert(float_within_tolerance(run_samples(&state, 0, 1), 0.0f));
  assert(!adsr_is_active(&state));

  //Voice manager without ADSR settings still plays notes instead of going silent
  VoiceManager vm;
  voice_manager_init(&vm, 48000, 48000, 0);
  assert(voice_manager_note_on(&vm, 48, 127));
  uint8_t heard_sound = 0;
  for (int i = 0; i < 100; i++)
    if (voice_manager_get_sample(&vm, &test_waveform) != (wavegen_output_t)WAVEGEN_OUTPUT_GROUND)
      heard_sound = 1;
  assert(heard_sound);
  voice_manager_note_off(&vm, 48);
  voice_manager_get_sample(&vm, &test_waveform);
  assert(vm.active_voice_count == 0);
}

void test_adsr(){
  printf("test_adsr()\n");
  adsr_set_params(&test_param, 10, 20, 0.5f, 40, AUDIO_SAMPLE_PERIOD);
  test_adsr_full_envelope();
  test_adsr_changes_every_sample();
  test_adsr_1ms_attack_ramps();
  test_adsr_release_before_sustain();
  test_adsr_retrigger_during_release();
  test_adsr_knob_change_no_jump();
  test_adsr_apply_volume();
  test_adsr_voice_freed_after_release();
  test_adsr_null_param_is_no_envelope();
}
