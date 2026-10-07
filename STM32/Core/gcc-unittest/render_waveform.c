#include "waveform.h"
#include "voice_manager.h"
#include "sine_lookup.h"
#include "volume_oscillation.h"
#include "audio_out.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Offline audio callback equivalent. No GPIO, timer timing, or ISR concurrency. */
int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s output-directory\n", argv[0]);
        return EXIT_FAILURE;
    }
    sine_lookup_init();
    for (int shape = 0; shape < 2; shape++) {
        for (int mode = 0; mode < 3; mode++) {
            char path[1024];
            const char *names[] = {"off", "firmware_lfo", "demo_lfo"};
            snprintf(path, sizeof(path), "%s/%s_%s.csv", argv[1],
                     shape == 0 ? "sine" : "triangle", names[mode]);
            FILE *out = fopen(path, "w");
            if (!out) { perror(path); return EXIT_FAILURE; }
            WaveformConfig cfg = {.rise_pct=.5f, .fall_pct=.5f,
                .rise_shape=shape == 0 ? WAVE_SINE : WAVE_TRIANGLE,
                .fall_shape=shape == 0 ? WAVE_SINE : WAVE_TRIANGLE, .max_output=350.0f};
            VolumeOscillationParam lfo = {.frequency=mode == 2 ? 10.0f : .5f,
                .completion=0, .strength=mode == 0 ? 0.0f : .25f};
            VoiceManager vm;
            voice_manager_init(&vm, AUDIO_SAMPLE_RATE_HZ, AUDIO_SAMPLE_RATE_HZ);
            const unsigned keys[] = {36, 40, 43, 60, 64};
            fprintf(out, "sample_index,time_s,mixed_pwm,volume_multiplier,output_pwm,active_voices\n");
            unsigned low=350, high=0, rails=0;
            float min_volume=1, max_volume=0;
            for (unsigned i=0; i<18*48000; i++) {
                /* main.c's modulo-five test: starts at 2 s, adds five notes, repeats. */
                if (i >= 2*48000 && i % (2*48000) == 0)
                    voice_manager_note_on(&vm, keys[(i/(2*48000)-1)%5], 127);
                unsigned mixed = voice_manager_get_sample(&vm, &cfg);
                float volume = get_output_amplitude_multiplier(&lfo);
                float value = ((float)mixed-175.0f)*volume+175.0f;
                assert(value >= 0 && value <= 350);
                unsigned pwm = (wavegen_output_t)value;
                if (pwm<low) low=pwm;
                if (pwm>high) high=pwm;
                if (pwm==0 || pwm==350) rails++;
                if (volume<min_volume) min_volume=volume;
                if (volume>max_volume) max_volume=volume;
                fprintf(out, "%u,%.9f,%u,%.9f,%u,%u\n", i, i/48000.0,
                    mixed, volume, pwm, vm.active_voice_count);
                get_oscillation_completion(&lfo, AUDIO_SAMPLE_PERIOD);
            }
            if (fclose(out)) { perror(path); return EXIT_FAILURE; }
            printf("%s: PWM [%u,%u], rail samples=%u, volume=[%.6f,%.6f]\n",
                path,low,high,rails,min_volume,max_volume);
            /* Stress the nine-voice limit and check full-polyphony output bounds. */
            voice_manager_init(&vm,48000,48000);
            for (unsigned k=36;k<45;k++) assert(voice_manager_note_on(&vm,k,127));
            assert(!voice_manager_note_on(&vm,45,127));
            for (unsigned i=0;i<48000;i++) assert(voice_manager_get_sample(&vm,&cfg)<=350);
        }
    }
    return EXIT_SUCCESS;
}
