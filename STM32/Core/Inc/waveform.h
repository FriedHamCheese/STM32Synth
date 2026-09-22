#ifndef WAVEFORM_H
#define WAVEFORM_H

#include <stdint.h>

typedef uint16_t wavegen_output_t;

typedef enum
{
    WAVE_SINE = 0,
    WAVE_TRIANGLE,
    WAVE_SQUARE
} WaveShape;

typedef struct
{
    float waveform_completion_ratio;
    float waveform_completion_increment;
} Waveform;

typedef struct
{
    float rise_pct;
    float fall_pct;
    WaveShape rise_shape;
    WaveShape fall_shape;
    float max_output;
} WaveformConfig;

void waveform_init(void);

wavegen_output_t waveform_get_point(float phase, const WaveformConfig *config);

wavegen_output_t get_sine_point(float phase, float max_output);
wavegen_output_t get_triangle_point(float phase, float max_output);
wavegen_output_t get_square_point(float phase, float max_output);

#endif
