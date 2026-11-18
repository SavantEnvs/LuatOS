// Fuzz the WebP decoder as LuatOS calls it (components/luat_image): luat_webp_decode_sw_default hands the bytes
// to libwebp's WebPDecodeRGBA (RIFF/VP8/VP8L/VP8X parsing, Huffman and transform tables, alpha plane, upsampling)
// and converts the RGBA result to the LCD colour buffer.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "luat_base.h"
#include "luat_mem.h"
#include "luat_image.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    luat_img_conf_t conf;
    luat_img_info_t info;
    memset(&conf, 0, sizeof(conf));
    memset(&info, 0, sizeof(info));
    conf.format = LUAT_IMG_FMT_WEBP;
    luat_webp_decode_sw_default(&conf, (uint8_t *)data, size, &info);
    luat_heap_free(info.data);
    return 0;
}
