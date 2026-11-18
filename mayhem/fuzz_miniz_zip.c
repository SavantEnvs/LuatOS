// Fuzz the ZIP reader behind miniz.unzip() (components/miniz): central-directory parsing and
// entry extraction, set up as luat_lib_miniz.c does (custom read callback, custom allocator),
// with the entry bytes streamed to a counting sink instead of the filesystem.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "miniz.h"

typedef struct { const uint8_t *p; size_t n; } mem_t;

static size_t read_cb(void *opaque, mz_uint64 ofs, void *buf, size_t n) {
    mem_t *m = (mem_t *)opaque;
    if (ofs >= m->n) return 0;
    if (n > m->n - (size_t)ofs) n = m->n - (size_t)ofs;
    memcpy(buf, m->p + ofs, n);
    return n;
}
// Output cap: a 261-byte entry can inflate to ~4 GB, which is a >40 s timeout, not a bug. Past 4 MiB the
// sink reports a short write, so miniz aborts the entry (MZ_ZIP_WRITE_CALLBACK_FAILED) and the next one runs.
#define OUT_CAP (1u << 22)
static size_t sink_cb(void *opaque, mz_uint64 ofs, const void *buf, size_t n) {
    (void)ofs; (void)buf;
    size_t *total = (size_t *)opaque;
    if (*total + n > OUT_CAP) return 0;
    *total += n;
    return n;
}
static void *a_alloc(void *o, size_t items, size_t sz) { (void)o; return calloc(items, sz); }
static void a_free(void *o, void *p) { (void)o; free(p); }
static void *a_realloc(void *o, void *p, size_t items, size_t sz) { (void)o; return realloc(p, items * sz); }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    mem_t m = { data, size };
    mz_zip_archive z;
    memset(&z, 0, sizeof(z));
    z.m_pRead = read_cb;
    z.m_pIO_opaque = &m;
    z.m_archive_size = size;
    z.m_pAlloc = a_alloc;
    z.m_pFree = a_free;
    z.m_pRealloc = a_realloc;
    if (!mz_zip_reader_init(&z, size, 0)) return -1;
    mz_uint n = mz_zip_reader_get_num_files(&z);
    for (mz_uint i = 0; i < n; i++) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&z, i, &st)) continue;
        size_t out = 0;
        mz_zip_reader_extract_to_callback(&z, i, sink_cb, &out, 0);
    }
    mz_zip_reader_end(&z);
    return 0;
}
