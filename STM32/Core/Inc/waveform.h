#ifndef WAVEFORM_H
#define WAVEFORM_H

#include <stdint.h>

typedef uint16_t wavegen_output_t;

typedef enum
{
    WAVE_SINE = 0,
    WAVE_TRIANGLE,
    WAVE_SQUARE,
    WAVE_SAWTOOTH,
    WAVE_SHAPE_COUNT    /* number of wavetables */
} WaveShape;

typedef struct
{
    float waveform_completion_ratio;
    float waveform_completion_increment;
} Waveform;

typedef struct
{
    /* Which wavetable the oscillator plays. */
    WaveShape shape;
    float     max_output;

    /* Legacy two-half ("candidate C") shaping fields. They are not used by the
       wavetable oscillator; kept so existing initialisers/pot code still compile. */
    float     rise_pct;
    float     fall_pct;
    WaveShape rise_shape;
    WaveShape fall_shape;
} WaveformConfig;

void waveform_init(void);

/* Play the wavetable selected by config->shape. Returns 0..max_output. */
wavegen_output_t waveform_get_point(float phase, const WaveformConfig *config);

/* Read one of the built-in wavetables directly. phase is wrapped into [0,1). */
wavegen_output_t waveform_get_wavetable_point(WaveShape shape, float phase, float max_output);

/* Convenience wrappers over waveform_get_wavetable_point(). */
wavegen_output_t get_sine_point(float phase, float max_output);
wavegen_output_t get_triangle_point(float phase, float max_output);
wavegen_output_t get_square_point(float phase, float max_output);
wavegen_output_t get_sawtooth_point(float phase, float max_output);

#endif
