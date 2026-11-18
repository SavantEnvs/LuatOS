// Fuzz the MP3 decoder behind `codec.*` (components/multimedia/mp3_decode_port.c, minimp3): the input is an MP3
// file; codec.info() skips the ID3v2 tag and reads the first frame header, codec.data() decodes frame after frame.
#include "codec_harness.h"

int LLVMFuzzerInitialize(int *argc, char ***argv) { (void)argc; (void)argv; codec_fs_init(); return 0; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    return codec_run(LUAT_MULTIMEDIA_DATA_TYPE_MP3, data, size);
}
