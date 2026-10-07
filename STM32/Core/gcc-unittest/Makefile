CFLAGS := -Wall -Wextra -I../Inc -DPC_UNITTEST -DIS_MASTER

SOURCEFILES := ../Src/voice_manager.c ../Src/waveform.c ../Src/synth_comms.c ../Src/sine_lookup.c ../Src/volume_oscillation.c
TEST_SOURCEFILES := test_main.c test_synth_comms_process_key.c test_volume_oscillation.c test_musical.c

ALL_SOURCEFILES := $(SOURCEFILES) $(TEST_SOURCEFILES)

test_main: $(ALL_SOURCEFILES)
	$(CC) $(CFLAGS) $^ -o $@ -lm

.PHONY: test
test: test_main
	./test_main --test

render_waveform: render_waveform.c $(SOURCEFILES)
	$(CC) $(CFLAGS) $^ -o $@ -lm
