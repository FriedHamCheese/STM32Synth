#ifndef TEST_SYNTH_COMMS_H
#define TEST_SYNTH_COMMS_H

#define TEST_KEYS_PER_OCTAVE 12
#define TEST_TOTAL_KEYS TEST_KEYS_PER_OCTAVE * MAX_SLAVES

void test_process_keys_no_press();
void test_process_keys_move_press_and_release_c2();
void test_process_keys_three_keys_interleaved();

void test_process_keys();

#endif