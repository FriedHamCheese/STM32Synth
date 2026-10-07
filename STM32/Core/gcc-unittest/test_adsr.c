#include "adsr.h"
#include "voice_manager.h"
#include "sine_lookup.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#define ADSR_FLOAT_TEST_TOLERANCE 0.001f

static const AdsrParam test_param = {
  .attack_ms = 10,
  .decay_ms = 20,
  .sustain_level = 0.5f,
  .release_ms = 40
};

static uint8_t float_within_tolerance(float val, float target){
  return fabs(val - target) <= ADSR_FLOAT_TEST_TOLERANCE;
}

static void assert_volume(AdsrState *state, uint32_t now_ms, float expected){
  assert(float_within_tolerance(adsr_get_volume(state, &test_param, now_ms), expected));
}

static void test_adsr_full_envelope(){
  printf("\ttest_adsr_full_envelope()...\n");
  AdsrState state;
  adsr_init(&state);
  assert_volume(&state, 0, 0.0f);

  adsr_note_on(&state, &test_param, 100);
  assert_volume(&state, 100, 0.0f);
  assert_volume(&state, 105, 0.5f);   //attack halfway
  assert_volume(&state, 110, 1.0f);   //attack done
  assert_volume(&state, 115, 0.75f);  //decay falls 1.0 per 20ms
  assert_volume(&state, 120, 0.5f);   //reached sustain after 10ms
  assert(state.stage == ADSR_SUSTAIN);
  assert_volume(&state, 5000, 0.5f);

  adsr_note_off(&state, &test_param, 5000);
  assert_volume(&state, 5000, 0.5f);
  assert_volume(&state, 5010, 0.25f); //release falls 1.0 per 40ms
  assert(adsr_is_active(&state));
  assert_volume(&state, 5020, 0.0f);
  assert(!adsr_is_active(&state));
}

static void test_adsr_release_before_sustain(){
  printf("\ttest_adsr_release_before_sustain()...\n");
  AdsrState state;
  adsr_init(&state);

  adsr_note_on(&state, &test_param, 0);
  assert_volume(&state, 5, 0.5f);
  //Released mid-attack, fades out from 0.5 instead of cutting off
  adsr_note_off(&state, &test_param, 5);
  assert_volume(&state, 15, 0.25f);
  assert_volume(&state, 25, 0.0f);
  assert(!adsr_is_active(&state));
}

static void test_adsr_retrigger_during_release(){
  printf("\ttest_adsr_retrigger_during_release()...\n");
  AdsrState state;
  adsr_init(&state);

  adsr_note_on(&state, &test_param, 0);
  assert_volume(&state, 50, 0.5f);
  adsr_note_off(&state, &test_param, 50);
  assert_volume(&state, 60, 0.25f);
  //Attack ramps from 0.25 instead of jumping to 0
  adsr_note_on(&state, &test_param, 60);
  assert_volume(&state, 60, 0.25f);
  assert_volume(&state, 65, 0.75f);
  //Reaches 1.0 at 67.5ms, leftover 2.5ms goes into decay
  assert_volume(&state, 70, 0.875f);
}

static void test_adsr_skips_stages_in_one_call(){
  printf("\ttest_adsr_skips_stages_in_one_call()...\n");
  AdsrState state;
  adsr_init(&state);

  //Sample arrives late, attack and decay already finished
  adsr_note_on(&state, &test_param, 0);
  assert_volume(&state, 1000, 0.5f);
  assert(state.stage == ADSR_SUSTAIN);
}

static void test_adsr_knob_change_no_jump(){
  printf("\ttest_adsr_knob_change_no_jump()...\n");
  AdsrParam param = test_param;
  AdsrState state;
  adsr_init(&state);

  //Attack 10ms -> 100ms halfway through attack, keeps rising from 0.5 slower
  adsr_note_on(&state, &param, 0);
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 5), 0.5f));
  param.attack_ms = 100;
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 5), 0.5f));
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 15), 0.6f));

  //Sustain 0.5 -> 0.7 while held, glides up at decay rate
  param = test_param;
  adsr_note_on(&state, &param, 100);
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 1000), 0.5f));
  param.sustain_level = 0.7f;
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 1000), 0.5f));
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 1002), 0.6f));
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 1010), 0.7f));

  //Release 40ms -> 400ms mid-release, keeps falling from where it was, slower
  adsr_note_off(&state, &param, 2000);
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 2010), 0.45f));
  param.release_ms = 400;
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 2010), 0.45f));
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 2050), 0.35f));

  //Release 400ms -> 4ms, no instant cut, just falls faster
  param.release_ms = 4;
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 2050), 0.35f));
  assert(adsr_is_active(&state));
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 2051), 0.10f));
  assert(float_within_tolerance(adsr_get_volume(&state, &param, 2052), 0.0f));
  assert(!adsr_is_active(&state));
}

static void test_adsr_apply_volume(){
  printf("\ttest_adsr_apply_volume()...\n");
  assert(adsr_apply_volume(350, 1.0f) == 350);
  assert(adsr_apply_volume(350, 0.5f) == 262);
  assert(adsr_apply_volume(0, 0.5f) == 87);
  assert(adsr_apply_volume(175, 0.3f) == 175);
  assert(adsr_apply_volume(350, 0.0f) == 175);
}

static uint32_t fake_ms = 0;
static uint32_t get_fake_ms(void){
  return fake_ms;
}

static void test_adsr_voice_freed_after_release(){
  printf("\ttest_adsr_voice_freed_after_release()...\n");
  WaveformConfig wc = {
    .rise_pct = 0.5f,
    .fall_pct = 0.5f,
    .rise_shape = WAVE_SINE,
    .fall_shape = WAVE_SINE,
    .max_output = 350.0f
  };
  VoiceManager vm;
  fake_ms = 0;
  voice_manager_init(&vm, 48000, 48000, &test_param, get_fake_ms);

  assert(voice_manager_note_on(&vm, 48, 127));
  voice_manager_get_sample(&vm, &wc);
  assert(vm.active_voice_count == 1);

  fake_ms = 100;
  voice_manager_note_off(&vm, 48);
  fake_ms = 110;
  voice_manager_get_sample(&vm, &wc);
  assert(vm.active_voice_count == 1);  //still releasing

  //Pressing the same key again reuses its releasing voice
  assert(voice_manager_note_on(&vm, 48, 127));
  voice_manager_get_sample(&vm, &wc);
  assert(vm.active_voice_count == 1);

  voice_manager_note_off(&vm, 48);
  fake_ms = 200;
  voice_manager_get_sample(&vm, &wc);
  assert(vm.active_voice_count == 0);
  for (int i = 0; i < MAX_VOICES; i++)
    assert(!vm.voices[i].active);
}

static void test_adsr_null_param_is_no_envelope(){
  printf("\ttest_adsr_null_param_is_no_envelope()...\n");
  AdsrState state;
  adsr_init(&state);

  adsr_note_on(&state, 0, 100);
  assert(float_within_tolerance(adsr_get_volume(&state, 0, 100), 1.0f));
  assert(float_within_tolerance(adsr_get_volume(&state, 0, 5000), 1.0f));
  adsr_note_off(&state, 0, 5000);
  assert(float_within_tolerance(adsr_get_volume(&state, 0, 5000), 0.0f));
  assert(!adsr_is_active(&state));

  //Voice manager without ADSR settings still plays notes instead of going silent
  WaveformConfig wc = {
    .rise_pct = 0.5f,
    .fall_pct = 0.5f,
    .rise_shape = WAVE_SINE,
    .fall_shape = WAVE_SINE,
    .max_output = 350.0f
  };
  VoiceManager vm;
  fake_ms = 0;
  voice_manager_init(&vm, 48000, 48000, 0, get_fake_ms);
  assert(voice_manager_note_on(&vm, 48, 127));
  uint8_t heard_sound = 0;
  for (int i = 0; i < 100; i++)
    if (voice_manager_get_sample(&vm, &wc) != (wavegen_output_t)WAVEGEN_OUTPUT_GROUND)
      heard_sound = 1;
  assert(heard_sound);
  voice_manager_note_off(&vm, 48);
  voice_manager_get_sample(&vm, &wc);
  assert(vm.active_voice_count == 0);
}

void test_adsr(){
  printf("test_adsr()\n");
  test_adsr_full_envelope();
  test_adsr_release_before_sustain();
  test_adsr_retrigger_during_release();
  test_adsr_skips_stages_in_one_call();
  test_adsr_knob_change_no_jump();
  test_adsr_apply_volume();
  test_adsr_voice_freed_after_release();
  test_adsr_null_param_is_no_envelope();
}
