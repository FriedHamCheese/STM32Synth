#ifndef __SYNTH_COMMS_H
#define __SYNTH_COMMS_H

/* Board role:
   Master board → define IS_MASTER in project preprocessor (Project > Properties >
   C/C++ Build > Settings > Preprocessor > Defined symbols: IS_MASTER).
   Slave board  → define SLAVE_ID=<0..5> instead. */

/* #define IS_MASTER */
/* #define SLAVE_ID 0 */

#ifndef SLAVE_ID
#define SLAVE_ID 0
#endif

#ifndef PC_UNITTEST
#include "stm32f4xx_hal.h"
#endif
#include "voice_manager.h"

#include <stdint.h>
#include <string.h>

#define MAX_SLAVES        8
#define SLAVE_BASE_ADDR   0x10   /* 7-bit base; HAL expects (addr << 1) at call site */
#define I2C_TIMEOUT_MS    2
#define PRESS_THRESHOLD   300    /* signed ADC delta from baseline — tune per H3503 */
#define STALE_TIMEOUT_MS  30
#define MIDI_BASE_NOTE    48     /* C3 — key 0 on leftmost slave */

//idfk man
#define MOVE_THRESHOLD PRESS_THRESHOLD/2

typedef enum { KEY_IDLE, KEY_PRESSED } KeyState;

typedef struct {
    int16_t adc[12];             /* signed: raw_adc - 2048 (offset binary → two's comp) */
} KeyFrame;

/* ── Master ─────────────────────────────────────────────────────────────── */
#ifdef IS_MASTER

//ADC value of hall effect sensors
extern volatile int16_t  g_keys[MAX_SLAVES][12];
//ADC value of hall effect sensors set by boot_calibrate()
extern volatile int16_t  g_baseline[MAX_SLAVES][12];
//ADC value of hall effect sensors, set by process_keys() to be g_keys after function finishes
extern volatile int16_t  g_prev_keys[MAX_SLAVES][12];
extern volatile KeyState g_key_state[MAX_SLAVES][12];
//Set by process_keys() using map_velocity()
extern volatile int16_t  g_key_velocity[MAX_SLAVES][12];
//Set by poll_slaves() during retrieving g_keys as HAL_GetTick()
extern volatile uint32_t g_last_update[MAX_SLAVES];
//HAL_GetTick() when a key is considered moving down
extern volatile uint32_t g_press_start[MAX_SLAVES][12];
//1 if a key is considered moving down
extern volatile uint8_t  g_press_moving[MAX_SLAVES][12];
extern uint8_t           g_slave_count;

int16_t map_velocity(uint32_t delta_ms);

void boot_calibrate(void);   /* 200ms blocking cal at boot — call before scan_slaves */
//void scan_slaves(void);      /* probe 0x10..0x15, set g_slave_count */
void poll_slaves(void);      /* read KeyFrame from each slave, update g_keys */
/* threshold detect, note_on/note_off */
void process_keys(
    VoiceManager *vm, 
    uint8_t (*note_on)(VoiceManager*, uint16_t, uint16_t), 
    void    (*note_off)(VoiceManager*, uint16_t),
    uint32_t now_ms
);
//void stale_check(void);      /* zero rows silent >STALE_TIMEOUT_MS */

/* Wire these to the synthesis engine */
void note_on(uint8_t slave, uint8_t key, int16_t velocity);
void note_off(uint8_t slave, uint8_t key);

#endif /* IS_MASTER */

/* ── Slave ───────────────────────────────────────────────────────────────── */
#ifndef IS_MASTER

extern volatile KeyFrame g_keyframe;

void init_slave(void);       /* configure I2C slave address, start listen_IT */
void update_keyframe(void);  /* scan all 12 mux channels into g_keyframe */

#ifndef PC_UNITTEST
/* HAL I2C slave callbacks — implemented in synth_comms.c, called by HAL IT */
void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c,
                           uint8_t TransferDirection,
                           uint16_t AddrMatchCode);
void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c);
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c);
#endif

#endif /* !IS_MASTER */

#endif /* __SYNTH_COMMS_H */
