// Fuzz the AMR decoders behind `codec.*` (components/multimedia/amr_codec_port.c, opencore AMR-NB and AMR-WB):
// the low bit of the first input byte picks codec.AMR or codec.AMR_WB, the rest is the .amr file (magic line,
// then frames led by a mode byte); codec.info() checks the magic, codec.data() decodes the frames.
#include "codec_harness.h"

int LLVMFuzzerInitialize(int *argc, char ***argv) { (void)argc; (void)argv; codec_fs_init(); return 0; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return -1;
    return codec_run((data[0] & 1) ? LUAT_MULTIMEDIA_DATA_TYPE_AMR_WB : LUAT_MULTIMEDIA_DATA_TYPE_AMR_NB, data + 1, size - 1);
}
