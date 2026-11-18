// Fuzz the TJpgDec JPEG decoder (components/tjpgd, used by the LCD and video player code to show
// attacker-supplied images). The first input byte picks the output scale (0..3); the rest is the JPEG
// stream, fed through an in-memory input callback and decoded into a sink that reads every pixel.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "tjpgd.h"

typedef struct { const uint8_t *p; size_t n, pos; } mem_src_t;

static size_t in_cb(JDEC *jd, uint8_t *buf, size_t len) {
    mem_src_t *s = (mem_src_t *)jd->device;
    size_t left = s->n - s->pos;
    if (len > left) len = left;
    if (buf != NULL) memcpy(buf, s->p + s->pos, len);
    s->pos += len;
    return len;
}

static volatile uint32_t sink;

static int out_cb(JDEC *jd, void *bitmap, JRECT *rect) {
    (void)jd;
    size_t w = (size_t)rect->right - rect->left + 1, h = (size_t)rect->bottom - rect->top + 1;
    const uint8_t *px = (const uint8_t *)bitmap;
    uint32_t sum = 0;
    for (size_t i = 0; i < w * h * 2; i++) sum += px[i];
    sink += sum;
    return 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return -1;
    static uint8_t pool[40960];
    JDEC jd;
    mem_src_t src = { data + 1, size - 1, 0 };
    if (luat_jd_prepare(&jd, in_cb, pool, sizeof(pool), &src) != JDR_OK) return -1;
    return luat_jd_decomp(&jd, out_cb, data[0] & 3) == JDR_OK ? 0 : -1;
}
