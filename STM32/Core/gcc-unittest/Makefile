CFLAGS := -Wall -Wextra -I../Inc -DPC_UNITTEST -DIS_MASTER

SOURCEFILES := ../Src/voice_manager.c ../Src/waveform.c ../Src/synth_comms.c ../Src/sine_lookup.c ../Src/volume_oscillation.c ../Src/adsr.c
TEST_SOURCEFILES := $(wildcard ./*.c)

ALL_SOURCEFILES := $(SOURCEFILES) $(TEST_SOURCEFILES)

test_main: $(ALL_SOURCEFILES)
	$(CC) $(CFLAGS) $^ -o $@
