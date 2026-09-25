#include "synth_comms.h"
#include "i2c.h"
#include <math.h>

/* Frequency controlled by key events — read by audio ISR in main.c */
volatile float g_active_freq = 0.0f;

/* ═══════════════════════════════════════════════════════════════════════════
   MASTER
   ═══════════════════════════════════════════════════════════════════════════ */
#ifdef IS_MASTER

volatile int16_t  g_keys[MAX_SLAVES][12]       = {0};
volatile int16_t  g_baseline[MAX_SLAVES][12]   = {0};
volatile int16_t  g_prev_keys[MAX_SLAVES][12]  = {0};
volatile KeyState g_key_state[MAX_SLAVES][12]  = {KEY_IDLE};
volatile int16_t  g_key_velocity[MAX_SLAVES][12] = {0};
volatile uint32_t g_last_update[MAX_SLAVES]    = {0};
uint8_t           g_slave_count                = 0;

static KeyFrame s_frame;

/* Map MIDI note (0–127) to frequency in Hz */
static float midi_to_freq(uint8_t note)
{
    return 440.0f * powf(2.0f, ((float)note - 69.0f) / 12.0f);
}

/* Map velocity delta (ADC units / tick) to 0–127 */
static int16_t map_velocity(int16_t delta)
{
    int16_t v = delta < 0 ? -delta : delta;
    if (v > 127) v = 127;
    return v;
}

void boot_calibrate(void)
{
    /* One frame per slave at rest — hall sensors are stable, no averaging needed.
       Absent slaves time out in I2C_TIMEOUT_MS each (worst case 6*2ms = 12ms total). */
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

void process_keys(void)
{
    for (uint8_t s = 0; s < g_slave_count; s++) {
        for (uint8_t k = 0; k < 12; k++) {
            int16_t depth = g_keys[s][k] - g_baseline[s][k];
            int16_t prev  = g_prev_keys[s][k] - g_baseline[s][k];

            if (g_key_state[s][k] == KEY_IDLE && depth > PRESS_THRESHOLD) {
                g_key_velocity[s][k] = map_velocity(depth - prev);
                g_key_state[s][k]    = KEY_PRESSED;
                note_on(s, k, g_key_velocity[s][k]);
            } else if (g_key_state[s][k] == KEY_PRESSED && depth <= PRESS_THRESHOLD) {
                g_key_state[s][k] = KEY_IDLE;
                note_off(s, k);
            }

            g_prev_keys[s][k] = g_keys[s][k];
        }
    }
}

void stale_check(void)
{
    uint32_t now = HAL_GetTick();
    for (uint8_t s = 0; s < g_slave_count; s++) {
        if (now - g_last_update[s] > STALE_TIMEOUT_MS) {
            for (uint8_t k = 0; k < 12; k++) {
                if (g_key_state[s][k] == KEY_PRESSED) {
                    g_key_state[s][k] = KEY_IDLE;
                    note_off(s, k);
                }
                g_keys[s][k] = g_baseline[s][k];
            }
        }
    }
}

/* Synthesis integration — modifies g_active_freq (extern, read by audio ISR) */
void note_on(uint8_t slave, uint8_t key, int16_t velocity)
{
    (void)velocity;
    uint8_t midi = (uint8_t)(MIDI_BASE_NOTE + slave * 12 + key);
    float f = midi_to_freq(midi);
    __disable_irq();
    g_active_freq = f;
    __enable_irq();
}

void note_off(uint8_t slave, uint8_t key)
{
    (void)slave; (void)key;
    __disable_irq();
    g_active_freq = 0.0f;
    __enable_irq();
}

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
    hi2c1.Init.OwnAddress1 = (uint32_t)((SLAVE_BASE_ADDR + SLAVE_ID) << 1);
    if (HAL_I2C_Init(&hi2c1) != HAL_OK)
        Error_Handler();
    HAL_I2C_EnableListen_IT(&hi2c1);
}

void update_keyframe(void)
{
    /* Channels 0–11 → hall keys H1–H12 (mux Y0–Y11).
       ADC reads unsigned 12-bit; store as signed offset from midpoint. */
    for (uint8_t k = 0; k < 12; k++)
        g_keyframe.adc[k] = (int16_t)(mux_read(k) - 2048);
}

/* ── I2C slave callbacks ─────────────────────────────────────────────────── */

void HAL_I2C_AddrCallback(I2C_HandleTypeDef *hi2c,
                           uint8_t TransferDirection,
                           uint16_t AddrMatchCode)
{
    (void)AddrMatchCode;
    if (TransferDirection == I2C_DIRECTION_RECEIVE) {
        /* Master wants to read — snapshot keyframe and start transmit */
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
    /* Re-arm on any error (NAK on last byte is expected) */
    HAL_I2C_EnableListen_IT(hi2c);
}

#endif /* !IS_MASTER */
