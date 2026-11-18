// Host-environment stand-ins so the LuatOS components fuzzed here link without
// the full simulator (RTOS, message bus, hardware timers). No target logic lives here.
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include "luat_base.h"
#include "luat_msgbus.h"
#include "luat_mcu.h"
#include "luat_rtos_legacy.h"

int cmdline_argc = 0;
char **cmdline_argv = NULL;
int32_t luatos_pc_climode = 0;

/* The message bus is a queue the VM thread drains. Messages are dropped unless a harness sets mayhem_msgbus_record
 * (the HTTP client defers its teardown through the bus); it then runs the handlers with mayhem_msgbus_drain(),
 * which empties the queue, so no input sees another's messages. */
#define MAYHEM_MSGQ 8
int mayhem_msgbus_record;
static rtos_msg_t msgq[MAYHEM_MSGQ];
static int msgq_n;
uint32_t luat_msgbus_put(rtos_msg_t *msg, size_t timeout) {
    (void)timeout;
    if (!mayhem_msgbus_record) return 0;
    if (msgq_n >= MAYHEM_MSGQ) return 1;
    msgq[msgq_n++] = *msg;
    return 0;
}
void mayhem_msgbus_drain(void) {
    for (int i = 0; i < msgq_n; i++) if (msgq[i].handler) msgq[i].handler(NULL, msgq[i].ptr);
    msgq_n = 0;
}
#include "luat_rtos.h"
int luat_rtos_mutex_create(luat_rtos_mutex_t *m) { *m = (void *)1; return 0; }
int luat_rtos_mutex_lock(luat_rtos_mutex_t m, uint32_t t) { (void)m; (void)t; return 0; }
int luat_rtos_mutex_unlock(luat_rtos_mutex_t m) { (void)m; return 0; }
void *luat_mutex_create(void) { return NULL; }
LUAT_RET luat_mutex_lock(void *m) { (void)m; return 0; }
LUAT_RET luat_mutex_unlock(void *m) { (void)m; return 0; }
long luat_mcu_ticks(void) { return 0; }
uint32_t luat_mcu_hz(void) { return 1000; }
uint64_t luat_mcu_tick64_ms(void) { return 0; }
uint64_t luat_mcu_tick64(void) { return 0; }
int luat_crypto_trng(char *buff, size_t len) { for (size_t i = 0; i < len; i++) buff[i] = (char)(i * 31 + 7); return 0; }
int luat_rtc_timezone(int *tz) { if (tz) *tz = 32; return 0; }

// Heap on libc malloc: the stock PC port carves allocations from one bget pool,
// which hides heap overflows from AddressSanitizer.
#include "luat_malloc.h"

#define HEAP_CAP ((size_t)64 << 20) /* devices have a small heap: refuse huge requests like they do */
static void *cap_malloc(size_t n) { return n > HEAP_CAP ? NULL : malloc(n); }
static void *cap_realloc(void *p, size_t n) { return n > HEAP_CAP ? NULL : realloc(p, n); }
static void *cap_calloc(size_t a, size_t b) { return (b && a > HEAP_CAP / b) ? NULL : calloc(a, b); }
void luat_heap_init(void) {}
void *mayhem_last_heap_malloc; /* lets a mock free what 64-bit code hands over as a truncated int */
void *luat_heap_malloc(size_t len) { return mayhem_last_heap_malloc = cap_malloc(len); }
void luat_heap_free(void *p) { free(p); }
void *luat_heap_realloc(void *p, size_t len) { return cap_realloc(p, len); }
void *luat_heap_calloc(size_t n, size_t sz) { return cap_calloc(n, sz); }
void *luat_heap_zalloc(size_t sz) { return cap_calloc(1, sz); }
void *luat_heap_memalign(size_t al, size_t sz) { (void)al; return cap_malloc(sz); }
void *luat_heap_alloc(void *ud, void *ptr, size_t osize, size_t nsize) {
    (void)ud; (void)osize;
    if (nsize == 0) { free(ptr); return NULL; }
    return cap_realloc(ptr, nsize);
}
void luat_meminfo_luavm(size_t *t, size_t *u, size_t *m) { *t = *u = *m = 0; }
void luat_meminfo_sys(size_t *t, size_t *u, size_t *m) { *t = *u = *m = 0; }
#include "luat_mem.h"
void luat_meminfo_opt_sys(LUAT_HEAP_TYPE_E type, size_t *t, size_t *u, size_t *m) { (void)type; *t = *u = *m = 0; }
void luat_heap_opt_init(LUAT_HEAP_TYPE_E t) { (void)t; }
void *luat_heap_opt_malloc(LUAT_HEAP_TYPE_E t, size_t n) { (void)t; return cap_malloc(n); }
void luat_heap_opt_free(LUAT_HEAP_TYPE_E t, void *p) { (void)t; free(p); }
void *luat_heap_opt_realloc(LUAT_HEAP_TYPE_E t, void *p, size_t n) { (void)t; return cap_realloc(p, n); }
void *luat_heap_opt_calloc(LUAT_HEAP_TYPE_E t, size_t n, size_t s) { (void)t; return cap_calloc(n, s); }
void *luat_heap_opt_zalloc(LUAT_HEAP_TYPE_E t, size_t s) { (void)t; return cap_calloc(1, s); }
void *luat_heap_opt_memalign(LUAT_HEAP_TYPE_E t, size_t a, size_t s) { (void)t; (void)a; return cap_malloc(s); }

/* pgfs batch API is only reached by miniz.unzip() onto a pgfs mount, which the harnesses never mount. */
#include "luat_pgfs.h"
int luat_pgfs_begin_batch(uint32_t *out_batch_id) { if (out_batch_id) *out_batch_id = 0; return -1; }
int luat_pgfs_commit_batch(uint32_t batch_id) { (void)batch_id; return -1; }
int luat_pgfs_abort_batch(uint32_t batch_id) { (void)batch_id; return -1; }

/* The simulator's logger defaults to DEBUG and appends every line to pclogs/*.log; mute it so fuzzing doesn't flood disk/stdout. */
#include "luat_log.h"
__attribute__((constructor)) static void mayhem_quiet_log(void) { luat_log_set_level(LUAT_LOG_CLOSE); }

/* The HTTP client reports socket events to the network-driver layer, which is not built here. */
#include "luat_network_adapter.h"
#include "luat_netdrv.h"
#include "luat_netdrv_event.h"
void luat_netdrv_fire_socket_event_netctrl(uint32_t event_id, network_ctrl_t *ctrl, uint8_t proto) { (void)event_id; (void)ctrl; (void)proto; }

/* The codec encoders (never driven by the codec.* harnesses) grow the output zbuff through this; same body as luat_lib_zbuff.c. */
#include "luat_zbuff.h"
int __zbuff_resize(luat_zbuff_t *buff, uint32_t new_size) {
    void *p = cap_realloc(buff->addr, new_size ? new_size : 1);
    if (p == NULL) return -1;
    buff->addr = p;
    buff->len = new_size;
    buff->used = (buff->len > buff->used) ? buff->used : buff->len;
    return 0;
}
