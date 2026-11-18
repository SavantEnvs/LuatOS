// Known-answer probe for the MQTT client receive path (see kat.c): a CONNACK then a QoS-1 PUBLISH arrive in
// one read; the PUBLISH message handed to the application callback must carry the topic and payload.
#include <stdio.h>
#include <string.h>
#include "luat_base.h"
#include "luat_network_adapter.h"
#include "luat_mem.h"
#include "libemqtt.h"
#include "luat_mqtt.h"
#include "luat_websocket.h"
#include "luat_http.h"
#include "mock_network.h"

int luat_mqtt_read_packet(luat_mqtt_ctrl_t *mqtt_ctrl);
extern void *mayhem_last_heap_malloc;

int32_t luatos_mqtt_callback(lua_State *L, void *ptr) { (void)L; (void)ptr; return 0; }
int l_luat_websocket_msg_cb(luat_websocket_ctrl_t *w, int a, int b) { (void)w; (void)a; (void)b; return 0; }

static int got_connack, got_publish, publish_asked;
static char got_topic[32], got_payload[32];
static int app_cb(luat_mqtt_ctrl_t *c, uint16_t ev) {
    (void)c;
    if (ev == MQTT_MSG_CONNACK) got_connack = 1;
    if (ev != MQTT_MSG_PUBLISH) return 0;
    if (!publish_asked) { publish_asked = 1; return 0; }
    luat_mqtt_msg_t *m = (luat_mqtt_msg_t *)mayhem_last_heap_malloc;
    snprintf(got_topic, sizeof got_topic, "%.*s", m->topic_len, (char *)m->data);
    snprintf(got_payload, sizeof got_payload, "%.*s", (int)m->payload_len, (char *)m->data + m->topic_len);
    printf("mqtt_msg_id=%u\n", m->message_id);
    free(m); mayhem_last_heap_malloc = NULL;
    got_publish = 1;
    return 1;
}

void kat_mqtt(void) {
    static const uint8_t wire[] = {
        0x20, 0x02, 0x00, 0x00,                                              /* CONNACK, accepted */
        0x32, 0x0f, 0x00, 0x03, 'a', '/', 'b', 0x12, 0x34, 'h', 'e', 'l', 'l', 'o', '!', '!', '!',   /* PUBLISH QoS1 id 0x1234 */
    };
    luat_mqtt_ctrl_t *m = calloc(1, sizeof(*m));
    m->netc = calloc(1, sizeof(network_ctrl_t));
    m->rxbuff_size = MQTT_RECV_BUF_LEN_MAX + 4;
    m->app_cb = (void *)app_cb;
    m->broker.socket_info = m;
    m->broker.send = luat_mqtt_send_packet;
    m->mqtt_packet_buffer = calloc(1, m->rxbuff_size + 4);
    m->mqtt_state = MQTT_STATE_MQTT;
    stream.p = wire; stream.n = sizeof(wire);
    int rc = luat_mqtt_read_packet(m);
    printf("mqtt_read=%d connack=%d state=%d publish=%d topic=%s payload=%s\n", rc, got_connack, (int)m->mqtt_state, got_publish, got_topic, got_payload);
    static const uint8_t bad[] = { 0x32, 0x00 };   /* PUBLISH with no topic */
    stream.p = bad; stream.n = sizeof(bad);
    publish_asked = 0;
    rc = luat_mqtt_read_packet(m);
    printf("mqtt_bad_publish=%d state=%d\n", rc, (int)m->mqtt_state);
}

/* HTTP client response path: the same drive as fuzz_http_client, with the terminal message read through the hook. */
int http_set_url(luat_http_ctrl_t *http_ctrl, const char *url, const char *method);
int http_close(luat_http_ctrl_t *http_ctrl);
extern int mayhem_msgbus_record;
extern void mayhem_msgbus_drain(void);
extern void (*mayhem_http_event_hook)(luat_http_msg_t *msg);

static struct { int terminals, event, status; uint32_t body_len, headers_len; char body[64]; int has_type; } hres;
static void http_hook(luat_http_msg_t *m) {
    if (m->event == HTTP_CALLBACK) return;   /* progress notifications */
    hres.terminals++;
    hres.event = m->event; hres.status = m->status_code; hres.body_len = m->body_len; hres.headers_len = m->headers_len;
    snprintf(hres.body, sizeof hres.body, "%.*s", m->body ? (int)(m->body_len < 63 ? m->body_len : 63) : 0, m->body ? m->body : "");
    for (uint32_t i = 0; m->headers && i + 12 <= m->headers_len; i++)
        if (memcmp(m->headers + i, "Content-Type", 12) == 0) hres.has_type = 1;
}
static void http_progress(int status, void *data, uint32_t len, void *user) { (void)status; (void)data; (void)len; (void)user; }

static void http_one(const char *name, const char *resp, int close_after) {
    memset(&hres, 0, sizeof hres);
    mayhem_msgbus_record = 1;
    mayhem_http_event_hook = http_hook;
    luat_http_ctrl_t *c = calloc(1, sizeof(*c));
    network_ctrl_t *netc = calloc(1, sizeof(*netc));
    c->netc = netc;
    luat_http_client_init(c, 0);
    uint32_t idg = c->idg;
    c->http_cb = (void *)http_progress;
    c->retry_cnt_max = 0;
    c->state = HTTP_STATE_IDLE;
    c->resp_content_len = -1;
    http_set_url(c, "http://example.com/a", "GET");
    OS_EVENT ev;
    memset(&ev, 0, sizeof ev);
    ev.ID = EV_NW_RESULT_CONNECT; mock_nw_cb(&ev, mock_nw_param);
    stream.p = (const uint8_t *)resp; stream.n = strlen(resp);
    ev.ID = EV_NW_RESULT_EVENT;
    if (luat_http_idg_get(idg)) mock_nw_cb(&ev, mock_nw_param);
    if (close_after && luat_http_idg_get(idg)) { ev.ID = EV_NW_RESULT_CLOSE; mock_nw_cb(&ev, mock_nw_param); }
    if (luat_http_idg_get(idg)) http_close(c);
    mayhem_msgbus_drain();
    mayhem_http_event_hook = NULL; mayhem_msgbus_record = 0;
    free(netc);
    printf("http_client_%s=%d event=%d status=%d body_len=%u body=%s content_type=%d\n", name, hres.terminals, hres.event, hres.status,
           hres.body_len, hres.body, hres.has_type);
}

void kat_http_client(void) {
    http_one("length", "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 5\r\n\r\nhello", 0);
    http_one("chunked", "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nTransfer-Encoding: chunked\r\n\r\n5\r\nhello\r\n6\r\n world\r\n0\r\n\r\n", 0);
    http_one("notfound", "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length: 4\r\n\r\ngone", 0);
}
