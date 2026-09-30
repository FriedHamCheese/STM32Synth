#include "synth_comms.h"
#include "voice_manager.h"
#include <math.h>

#ifndef PC_UNITTEST
#include "i2c.h"
#endif

/* ═══════════════════════════════════════════════════════════════════════════
   MASTER
   ═══════════════════════════════════════════════════════════════════════════ */
#ifdef IS_MASTER

volatile int16_t  g_keys[MAX_SLAVES][12]          = {0};
volatile int16_t  g_baseline[MAX_SLAVES][12]      = {0};
volatile int16_t  g_prev_keys[MAX_SLAVES][12]     = {0};
volatile KeyState g_key_state[MAX_SLAVES][12]     = {KEY_IDLE};
volatile int16_t  g_key_velocity[MAX_SLAVES][12]  = {0};
volatile uint32_t g_last_update[MAX_SLAVES]       = {0};
volatile uint32_t g_press_start[MAX_SLAVES][12]   = {0};
volatile uint8_t  g_press_moving[MAX_SLAVES][12]  = {0};
uint8_t           g_slave_count                   = 0;

static KeyFrame s_frame;

/* Fast press (10ms) → vel 127; slow press (2500ms+) → vel 1 */
int16_t map_velocity(uint32_t delta_ms)
{
    const uint32_t min_ms = 10;
    const uint32_t max_ms = 2500;
    const int16_t max_velocity = 127;
    const int16_t min_velocity = 1;
    
    if (delta_ms <= min_ms)  return max_velocity;
    if (delta_ms >= max_ms) return min_velocity;
    return (int16_t)(max_velocity - ((delta_ms*max_velocity)/max_ms));
}

#ifndef PC_UNITTEST
void boot_calibrate(void)
{
    for (uint8_t s = 0; s < MAX_SLAVES; s++) {
        if (HAL_I2C_Master_Receive(&hi2c1, (uint16_t)((SLAVE_BASE_ADDR + s) << 1),
                                   (uint8_t *)&s_frame, sizeof(s_frame),
                                   I2C_TIMEOUT_MS) == HAL_OK) {
            for (uint8_t k = 0; k < 12; k++)
                g_baseline[s][k] = s_frame.adc[k];
        }
    }
}

void scan_slaves(void)
{
    g_slave_count = 0;
    for (uint8_t s = 0; s < MAX_SLAVES; s++) {
        uint8_t dummy;
        if (HAL_I2C_Master_Receive(&hi2c1, (uint16_t)((SLAVE_BASE_ADDR + s) << 1),
                                   &dummy, 1, I2C_TIMEOUT_MS) == HAL_OK)
            g_slave_count = s + 1;
    }
}

void poll_slaves(void)
{
    for (uint8_t s = 0; s < g_slave_count; s++) {
        if (HAL_I2C_Master_Receive(&hi2c1, (uint16_t)((SLAVE_BASE_ADDR + s) << 1),
                                   (uint8_t *)&s_frame, sizeof(s_frame),
                                   I2C_TIMEOUT_MS) == HAL_OK) {
            for (uint8_t k = 0; k < 12; k++)
                g_keys[s][k] = s_frame.adc[k];
            g_last_update[s] = HAL_GetTick();
        }
    }
}
#endif

void process_keys(VoiceManager *vm,
                  uint8_t (*note_on)(VoiceManager*, uint16_t, uint16_t),
                  void    (*note_off)(VoiceManager*, uint16_t),
                  uint32_t now_ms
){
    for (uint8_t s = 0; s < MAX_SLAVES; s++) {
        for (uint8_t k = 0; k < 12; k++) {
            int16_t depth = g_keys[s][k] - g_baseline[s][k];

            /* Track first movement for time-based velocity */
            if (!g_press_moving[s][k] && depth > MOVE_THRESHOLD) {
                g_press_moving[s][k] = 1;
                g_press_start[s][k]  = now_ms;
            }

            if (g_key_state[s][k] == KEY_IDLE && depth > PRESS_THRESHOLD) {
                uint32_t delta_ms    = now_ms - g_press_start[s][k];
                uint8_t  vel         = map_velocity(delta_ms);
                g_key_velocity[s][k] = (int16_t)vel;
                g_key_state[s][k]    = KEY_PRESSED;
                note_on(vm, (uint16_t)(s * 12 + k), vel);
            } else if (g_key_state[s][k] == KEY_PRESSED && depth <= PRESS_THRESHOLD) {
                g_key_state[s][k]    = KEY_IDLE;
                g_press_moving[s][k] = 0;
                note_off(vm, (uint16_t)(s * 12 + k));
            }

            g_prev_keys[s][k] = g_keys[s][k];
        }
    }
}

#ifndef PC_UNITTEST
void stale_check(void)
{
    uint32_t now = HAL_GetTick();
    for (uint8_t s = 0; s < g_slave_count; s++) {
        if (now - g_last_update[s] > STALE_TIMEOUT_MS) {
            for (uint8_t k = 0; k < 12; k++) {
                if (g_key_state[s][k] == KEY_PRESSED) {
                    g_key_state[s][k]    = KEY_IDLE;
                    g_press_moving[s][k] = 0;
                    g_keys[s][k]         = g_baseline[s][k];
                }
            }
        }
    }
}
#endif

#endif /* IS_MASTER */

/* ═══════════════════════════════════════════════════════════════════════════
   SLAVE
   ═══════════════════════════════════════════════════════════════════════════ */
#ifndef IS_MASTER

#include "mux_adc.h"

volatile KeyFrame g_keyframe = {0};
static uint8_t s_tx_buf[sizeof(KeyFrame)];

void init_slave(void)
{
    HAL_I2C_DeInit(&hi2c1);
    hi2c1.Init.OwnAddress1 = (uint32_t)((SLAVE_BASE_ADDR + SLAVE_ID) << 1);
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        Error_Handler();
    HAL_I2C_EnableListen_IT(&hi2c1);
}

void update_keyframe(void)
{
    for (uint8_t k = 0; k < 12; k++)
        g_keyframe.adc[k] = (int16_t)(mux_read(k) - 2048);
}

void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c,
                           uint8_t TransferDirection,
                           uint16_t AddrMatchCode)
{
    (void)AddrMatchCode;
    if (TransferDirection == I2C_DIRECTION_RECEIVE) {
        memcpy(s_tx_buf, (const void *)&g_keyframe, sizeof(g_keyframe));
        HAL_I2C_Slave_Transmit_IT(hi2c, s_tx_buf, sizeof(s_tx_buf));
    }
}

void HAL_I2C_SlaveTxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    HAL_I2C_EnableListen_IT(hi2c);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_AF | I2C_FLAG_BERR | I2C_FLAG_ARLO | I2C_FLAG_OVR);
    HAL_I2C_EnableListen_IT(hi2c);
}

#endif /* !IS_MASTER */
