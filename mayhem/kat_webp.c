// Known-answer probe for the WebP decoder (luat_webp_decode_sw_default -> libwebp): a 1x1 lossy, a 1x1 lossless and a
// truncated image; the decoded size and first pixel must match what the files encode.
#include <stdio.h>
#include <string.h>
#include "luat_base.h"
#include "luat_mem.h"
#include "luat_image.h"

static void webp_one(const char *name, const uint8_t *d, size_t n) {
    luat_img_conf_t conf; luat_img_info_t info;
    memset(&conf, 0, sizeof conf); memset(&info, 0, sizeof info);
    conf.format = LUAT_IMG_FMT_WEBP;
    int rc = luat_webp_decode_sw_default(&conf, (uint8_t *)d, n, &info);
    printf("webp_%s=%d size=%ux%u first_pixel_rgb565=", name, rc, (unsigned)info.width, (unsigned)info.height);
    if (rc == 0 && info.data) printf("%04X", (unsigned)*(uint16_t *)info.data);
    else printf("none");
    printf("\n");
    if (info.data) luat_heap_free(info.data);
}

void kat_webp(void) {
    static const uint8_t lossless[] = { 0x52,0x49,0x46,0x46,0x1a,0x00,0x00,0x00,0x57,0x45,0x42,0x50,0x56,0x50,0x38,0x4c,0x0d,0x00,0x00,0x00,0x2f,0x00,0x00,0x00,0x10,0x07,0x10,0x11,0x11,0x88,0x88,0xfe,0x07,0x00 };
    static const uint8_t lossy[] = { 0x52,0x49,0x46,0x46,0x22,0x00,0x00,0x00,0x57,0x45,0x42,0x50,0x56,0x50,0x38,0x20,0x16,0x00,0x00,0x00,0x30,0x01,0x00,0x9d,0x01,0x2a,0x01,0x00,0x01,0x00,0x0e,0xc0,0xfe,0x25,0xa4,0x00,0x03,0x70,0x00,0x00,0x00,0x00 };
    webp_one("lossless", lossless, sizeof lossless);
    webp_one("lossy", lossy, sizeof lossy);
    webp_one("truncated", lossy, 20);
    webp_one("garbage", (const uint8_t *)"not a webp file at all", 22);
}
