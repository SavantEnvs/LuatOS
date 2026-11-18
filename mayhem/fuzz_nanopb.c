// Fuzz the nanopb decoder/encoder (components/nanopb) the way the airlink RPC layer drives it
// (luat_airlink_rpc_nb_dispatch): zero a struct of the message's size, pb_decode the received bytes into
// it, and on success pb_encode it back. The first input byte selects which of the drv_*.pb.h messages.
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "pb_decode.h"
#include "pb_encode.h"
#include "drv_bluetooth.pb.h"
#include "drv_gpio.pb.h"
#include "drv_mobile.pb.h"
#include "drv_pm.pb.h"
#include "drv_sdata.pb.h"
#include "drv_uart.pb.h"
#include "drv_wlan.pb.h"

typedef struct { const pb_msgdesc_t *desc; size_t size; } fuzz_pb_msg_t;
#define FUZZ_PB_MSG(T) { T##_fields, sizeof(T) }

static const fuzz_pb_msg_t msgs[] = {
#include "fuzz_nanopb_msgs.h"
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return -1;
    const fuzz_pb_msg_t *m = &msgs[data[0] % (sizeof(msgs) / sizeof(msgs[0]))];
    void *msg = calloc(1, m->size);
    if (msg == NULL) return -1;
    pb_istream_t in = pb_istream_from_buffer(data + 1, size - 1);
    int ok = pb_decode(&in, m->desc, msg);
    if (ok) {
        size_t n = 0;
        if (pb_get_encoded_size(&n, m->desc, msg)) {
            uint8_t *out = malloc(n ? n : 1);
            if (out != NULL) {
                pb_ostream_t os = pb_ostream_from_buffer(out, n);
                pb_encode(&os, m->desc, msg);
                free(out);
            }
        }
    }
    free(msg);
    return ok ? 0 : -1;
}
