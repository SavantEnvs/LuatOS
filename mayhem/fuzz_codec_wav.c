// Fuzz the WAV reader behind `codec.*` (components/multimedia/wav_codec_port.c): the input is a RIFF/WAVE file;
// codec.info() parses the RIFF, fmt, fact and data chunk headers, codec.data() streams the samples.
#include "codec_harness.h"

int LLVMFuzzerInitialize(int *argc, char ***argv) { (void)argc; (void)argv; codec_fs_init(); return 0; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return codec_run(LUAT_MULTIMEDIA_DATA_TYPE_WAV, data, size);
}
