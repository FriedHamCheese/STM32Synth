#ifndef WAVEFORM_H
#define WAVEFORM_H

#include <stdint.h>

typedef uint16_t wavegen_output_t;

///Speaker center position, wavegen_output_t minus this is the signed amplitude
#define WAVEGEN_OUTPUT_GROUND 175.0f

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
    float midpoint;
} WaveformConfig;

/*
 * Control-rate copy of WaveformConfig with divisions already resolved.
 * Prepare once per sample, then call waveform_get_point_prepared() per voice.
 */
typedef struct
{
    float midpoint;
    float rise;
    float fall;
    float fall_start;
    float inv_midpoint;
    float inv_trough_span;
    float inv_rise;
    float inv_fall;
    uint8_t valid;
} WaveformPrepared;

void waveform_prepare(const WaveformConfig *config, WaveformPrepared *prepared);
wavegen_output_t waveform_get_point_prepared(
    float phase,
    const WaveformConfig *config,
    const WaveformPrepared *prepared
);
wavegen_output_t waveform_get_point(float phase, const WaveformConfig *config);

#endif
