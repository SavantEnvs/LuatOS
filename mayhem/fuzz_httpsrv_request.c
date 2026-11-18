// Fuzz the HTTP server's request path (components/network/httpsrv): the
// http_parser callbacks that accumulate URI, headers and body, then the
// static-route resolver and dispatcher that run once a request is complete.
// A mock transport stands in for the socket backend; the bytes arrive in two
// segments to cover fields that span TCP segments.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "luat_httpsrv.h"

static int mock_send(struct client_socket_ctx *c, const char *b, size_t n) { (void)c; (void)b; (void)n; return 0; }
static void mock_close(void *h) { (void)h; }
static int mock_dispatch(struct client_socket_ctx *c) { (void)c; return 0; }
static int mock_static(struct client_socket_ctx *c, const char *p, size_t n) { (void)c; (void)p; (void)n; return 0; }
static void mock_cleanup(struct client_socket_ctx *c) { (void)c; }

static const luat_httpsrv_transport_t mock_vt = {
    .name = "mock",
    .cli_send = mock_send,
    .cli_close = mock_close,
    .cli_dispatch_resp = mock_dispatch,
    .cli_serve_static = mock_static,
    .cli_cleanup = mock_cleanup,
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return -1;
    size_t cut = data[0] % size;
    const char *buf = (const char *)data + 1;
    size_t len = size - 1;
    if (cut > len) cut = len;

    client_socket_ctx_t *client = (client_socket_ctx_t *)calloc(1, sizeof(*client));
    if (client == NULL) return -1;
    client->vt = &mock_vt;
    luat_httpsrv_parser_init(client);

    int ret = -1;
    size_t n = luat_httpsrv_parser_exec(client, buf, cut);
    if (n == cut && client->parser.http_errno == 0) {
        n = luat_httpsrv_parser_exec(client, buf + cut, len - cut);
        if (n == len - cut && client->parser.http_errno == 0 && client->recv_done && client->uri) {
            luat_httpsrv_dispatch_request(client);
            ret = 0;
        }
    }
    luat_httpsrv_free_common(client);
    free(client);
    return ret;
}
