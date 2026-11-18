// Fuzz the Speex decoder behind `codec.*` (components/multimedia/speex_codec_port.c, libspeex): the low two bits
// of the first input byte pick codec.SPEEX_NB, _WB or _UWB, the rest is the stream codec.data() reads, frames
// of a 2-byte big-endian length followed by the Speex bits.
#include "codec_harness.h"

int LLVMFuzzerInitialize(int *argc, char ***argv) { (void)argc; (void)argv; codec_fs_init(); return 0; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return -1;
    static const int types[3] = {LUAT_MULTIMEDIA_DATA_TYPE_SPEEX_NB, LUAT_MULTIMEDIA_DATA_TYPE_SPEEX_WB, LUAT_MULTIMEDIA_DATA_TYPE_SPEEX_UWB};
    return codec_run(types[(data[0] & 3) % 3], data + 1, size - 1);
}
