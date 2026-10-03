#include "sine_lookup.h"
#include <math.h>
#define _USE_MATH_DEFINES

float sine_table[SINE_TABLE_SIZE + 1];
static int sine_table_initialized = 0;

float clamp01(float value)
{
    if (value < 0.0f)
        return 0.0f;

    if (value > 1.0f)
        return 1.0f;

    return value;
}

void sine_lookup_init(void)
{
    if (sine_table_initialized)
        return;

    for (int i = 0; i <= SINE_TABLE_SIZE; i++)
    {
        float x = (float)i / (float)SINE_TABLE_SIZE;
        sine_table[i] = sinf(x * M_PI * 2.0f);
    }

    sine_table_initialized = 1;
}

float sine_lookup(float wave_completion)
{
    float position;
    int index;
    float fraction;

    wave_completion = clamp01(wave_completion);

    position = wave_completion * (float)SINE_TABLE_SIZE;
    index = (int)position;

    if (index >= SINE_TABLE_SIZE)
        return sine_table[SINE_TABLE_SIZE];

    fraction = position - (float)index;

    return sine_table[index] +
           fraction * (sine_table[index + 1] - sine_table[index]);
}