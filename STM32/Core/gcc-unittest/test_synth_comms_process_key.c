#include "test_synth_comms.h"
#include "voice_manager.h"
#include "synth_comms.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

static const uint16_t velocity_not_marked = 0;
static uint16_t key_velocities[TEST_TOTAL_KEYS];

static uint8_t record_key_velocity(VoiceManager*, uint16_t key_id, uint16_t delta_ms){
    assert(key_id < TEST_TOTAL_KEYS);
    key_velocities[key_id] = delta_ms;
    return 1;
}

static void unmark_key_velocity(VoiceManager*, uint16_t key_id){
    assert(key_id < TEST_TOTAL_KEYS);
    key_velocities[key_id] = velocity_not_marked;
}

static void reset_keys(){
    memset((void*)g_keys, 0, sizeof(g_keys));
    memset((void*)g_baseline, 0, sizeof(g_baseline));
    memset((void*)g_prev_keys, 0, sizeof(g_prev_keys));
    memset((void*)g_key_state, 0, sizeof(g_key_state));
    memset((void*)g_key_velocity, 0, sizeof(g_key_velocity));
    memset((void*)g_last_update, 0, sizeof(g_last_update));
    memset((void*)g_press_start, 0, sizeof(g_press_start));
    memset((void*)g_press_moving, 0, sizeof(g_press_moving));
    
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        key_velocities[i] = velocity_not_marked;
}

void test_process_keys_no_press(){
    printf("\t\ttest_process_keys_no_press()...\n");
  
    VoiceManager vm;
    reset_keys();
    srand(1);
    
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 5000);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
      
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 7000);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
}

void test_process_keys_move_press_and_release_c2(){
    printf("\t\ttest_process_keys_move_press_and_release_c2()...\n");
  
    VoiceManager vm;
    reset_keys();

    const uint8_t octave = 2;
    const uint8_t key_offset_from_octave = 0;
    const uint8_t trigger_unequal = 1;
    const uint16_t baseline_adc_value = 500;
    const uint16_t noise_boundary = 100;

    //Baseline with noise, should not mark velocity
    g_baseline[2][key_offset_from_octave] = baseline_adc_value;
    g_keys[2][key_offset_from_octave] = baseline_adc_value - (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 4000);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);      
    
    g_keys[2][key_offset_from_octave] = baseline_adc_value + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 4050);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);   
      
    g_keys[2][key_offset_from_octave] = baseline_adc_value - (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 4100);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
    
    //Trigger move threshold with noise, should not mark velocity
    g_keys[2][key_offset_from_octave] = baseline_adc_value + MOVE_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 4200);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
      
    g_keys[2][key_offset_from_octave] = baseline_adc_value + MOVE_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 4700);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
      
    g_keys[2][key_offset_from_octave] = baseline_adc_value + MOVE_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 4800);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);

    //Trigger pressing threshold, velocity should be based on first move and first press
    g_keys[2][key_offset_from_octave] = baseline_adc_value + PRESS_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 5000);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++){
        if(i == octave * TEST_KEYS_PER_OCTAVE + key_offset_from_octave)
            assert(key_velocities[i] == map_velocity(5000 - 4200));
        else
            assert(key_velocities[i] == velocity_not_marked);
    }
    
    g_keys[2][key_offset_from_octave] = baseline_adc_value + PRESS_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 5300);    
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++){
        if(i == octave * TEST_KEYS_PER_OCTAVE + key_offset_from_octave)
            assert(key_velocities[i] == map_velocity(5000 - 4200));
        else
            assert(key_velocities[i] == velocity_not_marked);
    }   
    
    g_keys[2][key_offset_from_octave] = baseline_adc_value + PRESS_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 5400);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++){
        if(i == octave * TEST_KEYS_PER_OCTAVE + key_offset_from_octave)
            assert(key_velocities[i] == map_velocity(5000 - 4200));
        else
            assert(key_velocities[i] == velocity_not_marked);
    }

    //Release key, velocity is unmarked
    g_keys[2][key_offset_from_octave] = baseline_adc_value + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 5500);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
    
    g_keys[2][key_offset_from_octave] = baseline_adc_value + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 5600);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
      
    g_keys[2][key_offset_from_octave] = baseline_adc_value + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 5700);
    for(uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
}

void test_process_keys_three_keys_interleaved(){
    printf("\t\ttest_process_keys_three_keys_interleaved()...\n");

    VoiceManager vm;
    reset_keys();

    const uint16_t baseline_adc_value = 500;
    const uint16_t noise_boundary = 100;
    const uint8_t trigger_unequal = 1;
    const uint8_t key_ids[3] = {3, 47, 95};
    const uint8_t slave_1 = key_ids[0] / TEST_KEYS_PER_OCTAVE;
    const uint8_t key_1 = key_ids[0] % TEST_KEYS_PER_OCTAVE;
    const uint8_t slave_2 = key_ids[1] / TEST_KEYS_PER_OCTAVE;
    const uint8_t key_2 = key_ids[1] % TEST_KEYS_PER_OCTAVE;
    const uint8_t slave_3 = key_ids[2] / TEST_KEYS_PER_OCTAVE;
    const uint8_t key_3 = key_ids[2] % TEST_KEYS_PER_OCTAVE;

    g_baseline[slave_1][key_1] = baseline_adc_value;
    g_baseline[slave_2][key_2] = baseline_adc_value;
    g_baseline[slave_3][key_3] = baseline_adc_value;

    // Baseline stage.
    g_keys[slave_1][key_1] = baseline_adc_value - (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 1000);
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
    
    g_keys[slave_2][key_2] = baseline_adc_value + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 1100);
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
    
    g_keys[slave_3][key_3] = baseline_adc_value - (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 1200);
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);

    // Movement stage.
    g_keys[slave_1][key_1] = baseline_adc_value + MOVE_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 1300);
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
    
    g_keys[slave_2][key_2] = baseline_adc_value + MOVE_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 1400);
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
    
    g_keys[slave_3][key_3] = baseline_adc_value + MOVE_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 1500);
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);

    // Press stage; each velocity uses its own first movement timestamp.
    g_keys[slave_1][key_1] = baseline_adc_value + PRESS_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 1700);
    assert(key_velocities[key_ids[0]] == map_velocity(1700 - 1300));
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++){
        if(i == key_ids[0]) continue;
        assert(key_velocities[i] == velocity_not_marked);        
    }
    
    g_keys[slave_2][key_2] = baseline_adc_value + PRESS_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 1800);
    assert(key_velocities[key_ids[0]] == map_velocity(1700 - 1300));
    assert(key_velocities[key_ids[1]] == map_velocity(1800 - 1400));
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++){
        if((i == key_ids[0]) || (i == key_ids[1])) continue;
        assert(key_velocities[i] == velocity_not_marked);        
    }
    
    g_keys[slave_3][key_3] = baseline_adc_value + PRESS_THRESHOLD + trigger_unequal + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 1900);
    assert(key_velocities[key_ids[0]] == map_velocity(1700 - 1300));
    assert(key_velocities[key_ids[1]] == map_velocity(1800 - 1400));
    assert(key_velocities[key_ids[2]] == map_velocity(1900 - 1500));
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++){
        if((i == key_ids[0]) || (i == key_ids[1]) || (i == key_ids[2])) continue;
        assert(key_velocities[i] == velocity_not_marked);
    }

    // Release stage; released keys clear while later keys remain pressed.
    g_keys[slave_1][key_1] = baseline_adc_value + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 2100);    
    assert(key_velocities[key_ids[1]] == map_velocity(1800 - 1400));
    assert(key_velocities[key_ids[2]] == map_velocity(1900 - 1500));
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++){
        if((i == key_ids[1]) || (i == key_ids[2])) continue;
        assert(key_velocities[i] == velocity_not_marked);        
    }
    
    g_keys[slave_2][key_2] = baseline_adc_value - (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 2200);
    assert(key_velocities[key_ids[2]] == map_velocity(1900 - 1500));
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++){
        if(i == key_ids[2]) continue;
        assert(key_velocities[i] == velocity_not_marked);        
    }    
    
    g_keys[slave_3][key_3] = baseline_adc_value + (rand() % noise_boundary);
    process_keys(&vm, record_key_velocity, unmark_key_velocity, 2300);
    for (uint8_t i = 0; i < TEST_TOTAL_KEYS; i++)
        assert(key_velocities[i] == velocity_not_marked);
}

void test_process_keys(){
    printf("\ttest_process_keys()...\n");
    test_process_keys_no_press();
    test_process_keys_move_press_and_release_c2();
    test_process_keys_three_keys_interleaved();
}