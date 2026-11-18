// Fuzz the HTTP client's response path (components/network/libhttp): luat_lib_http_callback, the socket-event
// callback luat_http_client_init registers, reads the server's bytes, finds the end of the headers and feeds
// http_parser_execute with the client callbacks (header field/value, Content-Length, chunked framing, body into
// the heap buffer or a zbuff, completion and the terminal result). A mock network adapter serves the input as the
// server's reply. Byte 0: bit 0 = the server closes the socket after the data, bit 1 = body goes to a zbuff
// (as http.request's zbuff option does). Byte 1 splits the rest into two segments, delivered as two socket events.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "luat_base.h"
#include "luat_network_adapter.h"
#include "luat_mem.h"
#include "luat_http.h"
#include "mock_network.h"

int http_set_url(luat_http_ctrl_t *http_ctrl, const char *url, const char *method);   /* not in the header */
int http_close(luat_http_ctrl_t *http_ctrl);

extern int mayhem_msgbus_record;      /* mayhem/support.c */
extern void mayhem_msgbus_drain(void);

static void progress_cb(int status, void *data, uint32_t len, void *user) { (void)status; (void)data; (void)len; (void)user; }

static void fire(uint32_t id) {
    OS_EVENT ev;
    memset(&ev, 0, sizeof(ev));
    ev.ID = id;
    mock_nw_cb(&ev, mock_nw_param);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 3) return -1;
    mayhem_msgbus_record = 1;
    luat_http_ctrl_t *c = (luat_http_ctrl_t *)luat_heap_malloc(sizeof(*c));
    network_ctrl_t *netc = (network_ctrl_t *)calloc(1, sizeof(*netc));
    if (c == NULL || netc == NULL) { free(c); free(netc); return -1; }
    memset(c, 0, sizeof(*c));
    c->netc = netc;
    /* as luat_http_client_create + luat_http_client_init + http.request prepare it */
    if (luat_http_client_init(c, 0) != 0) { free(c); free(netc); return -1; }
    uint32_t idg = c->idg;
    c->http_cb = (void *)progress_cb;
    c->retry_cnt_max = 0;
    c->state = HTTP_STATE_IDLE;
    c->resp_content_len = -1;
    luat_zbuff_t *zb = NULL;
    if (data[0] & 2) {
        zb = (luat_zbuff_t *)calloc(1, sizeof(*zb));
        if (zb != NULL) {
            zb->addr = (uint8_t *)malloc(16);
            zb->len = 16;
            c->zbuff_body = zb;
        }
    }
    if (http_set_url(c, "http://example.com/a", "GET") != 0) { http_close(c); mayhem_msgbus_drain(); free(netc); return -1; }

    const uint8_t *resp = data + 2;
    size_t n = size - 2, cut = n * data[1] / 256;
    fire(EV_NW_RESULT_CONNECT);
    stream.p = resp; stream.n = cut;
    if (luat_http_idg_get(idg)) fire(EV_NW_RESULT_EVENT);
    stream.p = resp + cut; stream.n = n - cut;
    if (luat_http_idg_get(idg)) fire(EV_NW_RESULT_EVENT);
    if ((data[0] & 1) && luat_http_idg_get(idg)) fire(EV_NW_RESULT_CLOSE);
    if (luat_http_idg_get(idg)) http_close(c);
    mayhem_msgbus_drain();   /* the VM thread runs the deferred teardown that frees the session */
    if (zb != NULL) { free(zb->addr); free(zb); }
    free(netc);
    return 0;
}
