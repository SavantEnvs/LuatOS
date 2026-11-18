// Fuzz the MQTT client's receive path (components/network/libemqtt): luat_mqtt_read_packet drains the socket
// into the packet buffer, frames MQTT packets by their remaining-length field and dispatches CONNACK, PUBLISH
// (topic/payload lengths, QoS acks), SUBACK, PINGRESP and the rest. A mock network adapter serves the input as
// the broker's byte stream. The first input byte picks the starting state (bit 0 set: CONNACK already
// received, so PUBLISH and friends are accepted); the rest is the stream.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "luat_base.h"
#include "luat_network_adapter.h"
#include "luat_mem.h"
#include "libemqtt.h"
#include "luat_mqtt.h"
#include "luat_websocket.h"
#include "mock_network.h"

int luat_mqtt_read_packet(luat_mqtt_ctrl_t *mqtt_ctrl);   /* not in the header */

extern void *mayhem_last_heap_malloc;   /* mayhem/support.c */

int32_t luatos_mqtt_callback(lua_State *L, void *ptr) { (void)L; (void)ptr; return 0; }   /* Lua binding, queued via the message bus */

int l_luat_websocket_msg_cb(luat_websocket_ctrl_t *w, int a, int b) { (void)w; (void)a; (void)b; return 0; }   /* MQTT-over-WebSocket mode is not driven */

/* The firmware's Lua handler frees the PUBLISH message it is handed; the pointer arrives truncated to an int
 * on this 64-bit host, so the callback takes it from the allocation the parser just made. luat_mqtt_msg_cb
 * calls the app callback twice for one PUBLISH: first to ask whether the app handles it itself (0: no, parse
 * it), then with the parsed message. */
static int publish_asked;
static int app_cb(luat_mqtt_ctrl_t *c, uint16_t event) {
    (void)c;
    if (event != MQTT_MSG_PUBLISH) return 0;
    if (!publish_asked) { publish_asked = 1; return 0; }
    publish_asked = 0;
    free(mayhem_last_heap_malloc); mayhem_last_heap_malloc = NULL;
    return 1;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return -1;
    luat_mqtt_ctrl_t *m = (luat_mqtt_ctrl_t *)calloc(1, sizeof(*m));
    network_ctrl_t *netc = (network_ctrl_t *)calloc(1, sizeof(*netc));
    if (m == NULL || netc == NULL) { free(m); free(netc); return -1; }
    m->netc = netc;
    m->rxbuff_size = MQTT_RECV_BUF_LEN_MAX + 4;
    m->keepalive = 240;
    m->app_cb = (void *)app_cb;
    m->broker.socket_info = m;
    m->broker.send = luat_mqtt_send_packet;
    m->mqtt_packet_buffer = (uint8_t *)luat_heap_malloc(m->rxbuff_size + 4);   /* as luat_mqtt_connect allocates it */
    if (m->mqtt_packet_buffer == NULL) { free(m); free(netc); return -1; }
    memset(m->mqtt_packet_buffer, 0, m->rxbuff_size + 4);
    m->mqtt_state = (data[0] & 1) ? MQTT_STATE_READY : MQTT_STATE_MQTT;
    publish_asked = 0;
    stream.p = data + 1;
    stream.n = size - 1;
    int rc = luat_mqtt_read_packet(m);
    free(m->mqtt_packet_buffer);
    free(m);
    free(netc);
    return rc == 0 ? 0 : -1;
}
