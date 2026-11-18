// Mock network adapter and rtos timers shared by the harnesses that drive a network client's receive path
// (fuzz_websocket, fuzz_mqtt, fuzz_http_client). Include from exactly one translation unit per harness binary.
// The harness sets `stream` to the bytes the "socket" delivers; network_rx serves them in the sizes asked for.
#ifndef MAYHEM_MOCK_NETWORK_H
#define MAYHEM_MOCK_NETWORK_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "luat_base.h"
#include "luat_network_adapter.h"

static struct { const uint8_t *p; size_t n; } stream;
/* The callback and parameter a client registers with network_init_ctrl: what the network stack calls on socket events. */
static CBFuncEx_t mock_nw_cb;
static void *mock_nw_param;

int network_rx(network_ctrl_t *c, uint8_t *data, uint32_t len, int flags, luat_ip_addr_t *ip, uint16_t *port, uint32_t *rx_len) {
    (void)c; (void)flags; (void)ip; (void)port;
    if (data == NULL) { *rx_len = (uint32_t)stream.n; return 0; }
    size_t n = stream.n < len ? stream.n : len;
    memcpy(data, stream.p, n);
    stream.p += n; stream.n -= n;
    *rx_len = (uint32_t)n;
    return 0;
}
int network_tx(network_ctrl_t *c, const uint8_t *d, uint32_t len, int flags, luat_ip_addr_t *ip, uint16_t port, uint32_t *tx_len, uint32_t to) {
    (void)c; (void)d; (void)flags; (void)ip; (void)port; (void)to;
    *tx_len = len; return 0;
}
void network_force_close_socket(network_ctrl_t *c) { (void)c; }
int network_close(network_ctrl_t *c, uint32_t t) { (void)c; (void)t; return 0; }
void network_release_ctrl(network_ctrl_t *c) { (void)c; }
network_ctrl_t *network_alloc_ctrl(uint8_t a) { (void)a; return NULL; }
void network_init_ctrl(network_ctrl_t *c, HANDLE h, CBFuncEx_t cb, void *p) { (void)c; (void)h; mock_nw_cb = cb; mock_nw_param = p; }
void network_set_base_mode(network_ctrl_t *c, uint8_t a, uint32_t b, uint8_t d, uint32_t e, uint8_t f, uint8_t g) { (void)c; (void)a; (void)b; (void)d; (void)e; (void)f; (void)g; }
void network_connect_ipv6_domain(network_ctrl_t *c, uint8_t o) { (void)c; (void)o; }
int network_set_local_port(network_ctrl_t *c, uint16_t p) { (void)c; (void)p; return 0; }
int network_set_server_cert(network_ctrl_t *c, const unsigned char *d, size_t n) { (void)c; (void)d; (void)n; return 0; }
int network_set_client_cert(network_ctrl_t *c, const unsigned char *a, size_t b, const unsigned char *d, size_t e, const unsigned char *f, size_t g) { (void)c; (void)a; (void)b; (void)d; (void)e; (void)f; (void)g; return 0; }
int network_init_tls(network_ctrl_t *c, int v) { (void)c; (void)v; return 0; }
void network_deinit_tls(network_ctrl_t *c) { (void)c; }
int network_connect(network_ctrl_t *c, const char *d, uint32_t dl, luat_ip_addr_t *ip, uint16_t p, uint32_t t) { (void)c; (void)d; (void)dl; (void)ip; (void)p; (void)t; return 0; }
int network_wait_event(network_ctrl_t *c, OS_EVENT *e, uint32_t t, uint8_t *to) { (void)c; (void)e; (void)t; (void)to; return 0; }
void network_set_ip_invaild(luat_ip_addr_t *ip) { (void)ip; }
uint8_t network_ip_is_vaild(luat_ip_addr_t *ip) { (void)ip; return 0; }
void *luat_create_rtos_timer(void *cb, void *p, void *h) { (void)cb; (void)p; (void)h; return NULL; }
int luat_start_rtos_timer(void *t, uint32_t ms, uint8_t r) { (void)t; (void)ms; (void)r; return 0; }
void luat_stop_rtos_timer(void *t) { (void)t; }
void luat_release_rtos_timer(void *t) { (void)t; }

#endif
