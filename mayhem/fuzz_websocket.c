// Fuzz the WebSocket client's receive path (components/network/websocket): luat_websocket_read_packet
// drains the socket, parses the HTTP 101 handshake reply and then the frame headers, answers pings and
// hands text/binary frames to the callback. A mock network adapter serves the input as the byte stream.
// The first input byte picks the starting state (bit 0 set: handshake already done); the rest is the stream.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "luat_base.h"
#include "luat_network_adapter.h"
#include "luat_mem.h"
#include "luat_websocket.h"
#include "mock_network.h"

int luat_websocket_read_packet(luat_websocket_ctrl_t *websocket_ctrl);   /* not in the header */

extern void *mayhem_last_heap_malloc;   /* mayhem/support.c */

/* The real handler (Lua binding) frees the frame it is handed; the pointer arrives truncated to an int
 * on this 64-bit host, so the mock takes it from the allocation the parser just made. */
int l_luat_websocket_msg_cb(luat_websocket_ctrl_t *w, int arg1, int arg2) {
    (void)w; (void)arg2;
    if (arg1 == WEBSOCKET_MSG_PUBLISH) { free(mayhem_last_heap_malloc); mayhem_last_heap_malloc = NULL; }
    return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return -1;
    luat_websocket_ctrl_t *w = (luat_websocket_ctrl_t *)calloc(1, sizeof(*w));
    if (w == NULL) return -1;
    w->websocket_state = data[0] & 1;
    stream.p = data + 1;
    stream.n = size - 1;
    int rc = luat_websocket_read_packet(w);
    free(w);
    return rc == 0 ? 0 : -1;
}
