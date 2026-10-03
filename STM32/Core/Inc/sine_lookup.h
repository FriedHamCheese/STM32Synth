#ifndef SINE_LOOKUP_H
#define SINE_LOOKUP_H

#define SINE_TABLE_SIZE 256

extern float sine_table[SINE_TABLE_SIZE + 1];

float clamp01(float value);
void sine_lookup_init(void);
float sine_lookup(float wave_completion);

#endif