#include "audio_out.h"
#include "stm32f4xx_hal.h"
#include "tim.h"
#include "volume_oscillation.h"
#include "main.h"

static VoiceManager    *s_vm  = 0;
static WaveformConfig  *s_cfg = 0;
static VolumeOscillationParam *s_vosc = 0;
static float* s_mvol = 0;
static MetronomeParam *s_metro = 0;
static AudioDcBlockState s_dc_block = { 0.0f, 0.0f };

#ifdef WAVEGEN_PROFILE
volatile uint32_t g_audio_isr_cycles_last;
volatile uint32_t g_audio_isr_cycles_min = 0xFFFFFFFFu;
volatile uint32_t g_audio_isr_cycles_max;
volatile uint64_t g_audio_isr_cycles_sum;
volatile uint32_t g_audio_isr_cycles_count;
#endif

/* ponytail: file-scope pointers set once at init; safe because single-core M4 */
void audio_out_init(VoiceManager *vm, WaveformConfig *cfg, VolumeOscillationParam *voscp, float *master_volume, MetronomeParam *metro)
{
    s_vm  = vm;
    s_cfg = cfg;
    s_vosc = voscp;
    s_mvol = master_volume;
    s_metro = metro;

#ifdef WAVEGEN_PROFILE
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
#endif
}

//Function for readability, used only here, O1+ will probably inline this, but just to make it clear
__attribute__((always_inline))
inline void set_audio_output_value(wavegen_output_t left, wavegen_output_t right)
{
    TIM2->CCR1 = right;
    TIM2->CCR2 = left;
}

/* 12kHz audio tick — called directly from TIM4_IRQHandler */
void audio_out_tick(void)
{
#ifdef WAVEGEN_PROFILE
    const uint32_t profile_start = DWT->CYCCNT;
#endif

    if (!s_vm || !s_cfg || !s_vosc || !s_mvol || !s_metro)
        return;

    const float lfo_multiplier = get_output_amplitude_multiplier(s_vosc);
    const float voice_sample = (float)voice_manager_get_sample(s_vm, s_cfg);
    const float click = (float)metronome_get_sample(s_metro);
    const float mixed = audio_mix_sample(
        voice_sample, click, *s_mvol, lfo_multiplier
    );
    const float centered = audio_dc_block(
        &s_dc_block, mixed - WAVEGEN_OUTPUT_GROUND
    );
    float sample = centered + WAVEGEN_OUTPUT_GROUND;

    if (sample < AUDIO_OUTPUT_MIN)
        sample = AUDIO_OUTPUT_MIN;
    if (sample > AUDIO_OUTPUT_MAX)
        sample = AUDIO_OUTPUT_MAX;

    set_audio_output_value((wavegen_output_t)sample, (wavegen_output_t)sample);

    get_oscillation_completion(s_vosc, AUDIO_SAMPLE_PERIOD);

#ifdef WAVEGEN_PROFILE
    const uint32_t cycles = DWT->CYCCNT - profile_start;
    g_audio_isr_cycles_last = cycles;
    if (cycles < g_audio_isr_cycles_min)
        g_audio_isr_cycles_min = cycles;
    if (cycles > g_audio_isr_cycles_max)
        g_audio_isr_cycles_max = cycles;
    g_audio_isr_cycles_sum += cycles;
    g_audio_isr_cycles_count++;
#endif
}

/* TIM3 only: TIM4 is dispatched directly by TIM4_IRQHandler. */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM3)
        main_runtime_iter();
}
