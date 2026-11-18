// Known-answer probe: runs real LuatOS code (built at normal flags) on fixed inputs and prints
// `key=value` lines. mayhem/test.sh compares them against golden values, so a program that simply
// exits (or a patch that breaks the parsers) fails the oracle.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "luat_base.h"
#include "luat_malloc.h"
#include "lualib.h"
#include "minmea.h"
#include "luat_sms.h"
#include "luat_httpsrv.h"
#include "luat_uart.h"
#include "cJSON.h"
#include "pb_decode.h"
#include "drv_gpio.pb.h"
#include "tjpgd.h"
#include "fuzz_protobuf_schema.h"
#include "kat_jpeg.h"
#include "iconv.h"

int luaopen_cjson(lua_State *l);
int luaopen_miniz(lua_State *L);
int luaopen_fastlz(lua_State *L);
int luaopen_pack(lua_State *L);
int luaopen_protobuf(lua_State *L);
int luaopen_ercoap(lua_State *L);

extern luat_libgnss_t gnssctx;
int luat_libgnss_on_rawdata(const char *d, size_t n, int type) { (void)d; (void)n; (void)type; return 0; }
int luat_libgnss_state_onchanged(int state) { (void)state; return 0; }
int luat_uart_read(int id, void *buf, size_t n) { (void)id; (void)buf; (void)n; return 0; }
int luat_uart_write(int id, void *buf, size_t n) { (void)id; (void)buf; (void)n; return 0; }
int luat_rtc_set(struct tm *t) { (void)t; return 0; }

static void kat_lua(void) {
    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    luaopen_cjson(L); lua_setglobal(L, "json");
    const char *src =
        "local t = json.decode('{\"a\":[1,2,3],\"b\":\"x\",\"c\":true,\"d\":null}')\n"
        "local e = json.encode({10,20,30})\n"
        "return 6*7, #t.a + t.a[3], t.b .. '!', (t.c and 'yes' or 'no'), e, json.encode((json.decode('[1.5,\"q\"]')))";
    if (luaL_loadstring(L, src) != LUA_OK || lua_pcall(L, 0, 6, 0) != LUA_OK) {
        printf("lua_error=%s\n", lua_tostring(L, -1));
    } else {
        printf("lua_arith=%d\n", (int)lua_tointeger(L, 1));
        printf("cjson_decode=%d\n", (int)lua_tointeger(L, 2));
        printf("cjson_string=%s\n", lua_tostring(L, 3));
        printf("cjson_bool=%s\n", lua_tostring(L, 4));
        printf("cjson_encode=%s\n", lua_tostring(L, 5));
        printf("cjson_roundtrip=%s\n", lua_tostring(L, 6));
    }
    lua_close(L);
}

static void kat_minmea(void) {
    const char *rmc = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A";
    struct minmea_sentence_rmc f;
    printf("nmea_check=%d\n", minmea_check(rmc, true));
    printf("nmea_id=%d\n", (int)minmea_sentence_id(rmc, true) == (int)MINMEA_SENTENCE_RMC);
    if (minmea_parse_rmc(&f, rmc)) {
        printf("nmea_time=%02d:%02d:%02d\n", f.time.hours, f.time.minutes, f.time.seconds);
        printf("nmea_lat=%d/%d\n", (int)f.latitude.value, (int)f.latitude.scale);
        printf("nmea_lon=%d/%d\n", (int)f.longitude.value, (int)f.longitude.scale);
        printf("nmea_speed=%d/%d\n", (int)f.speed.value, (int)f.speed.scale);
        printf("nmea_date=%02d-%02d-%02d\n", f.date.day, f.date.month, f.date.year);
    } else {
        printf("nmea_error=parse\n");
    }
    printf("nmea_badsum=%d\n", minmea_check("$GPRMC,123519,A*00", true));
}

static void kat_sms(void) {
    static uint8_t pdu[] = {0x07,0x91,0x68,0x31,0x08,0x20,0x10,0x50, 0x04,0x0B,0x91,0x68,0x31,0x08,0x10,0x83,0xF0,
                            0x00,0x00, 0x62,0x11,0x12,0x21,0x43,0x65,0x23, 0x05,0xC8,0x32,0x9B,0xFD,0x06};
    static luat_sms_recv_msg_t m;
    int rc = luat_sms_pdu_message_unpack(&m, pdu, (int)sizeof(pdu));
    printf("sms_rc=%d\n", rc);
    printf("sms_text=%s\n", (char *)m.sms_buffer);
    printf("sms_from=%s\n", (char *)m.phone_address);
    printf("sms_time=%02d-%02d-%02d %02d:%02d:%02d\n", m.time.year, m.time.month, m.time.day,
           m.time.hour, m.time.minute, m.time.second);
    printf("sms_truncated_rc=%d\n", luat_sms_pdu_message_unpack(&m, pdu, 12));
}

static int mock_send(struct client_socket_ctx *c, const char *b, size_t n) { (void)c; (void)b; (void)n; return 0; }
static void mock_close(void *h) { (void)h; }
static int mock_dispatch(struct client_socket_ctx *c) { (void)c; return 0; }
static int mock_static(struct client_socket_ctx *c, const char *p, size_t n) { (void)c; (void)p; (void)n; return 0; }
static void mock_cleanup(struct client_socket_ctx *c) { (void)c; }
static const luat_httpsrv_transport_t mock_vt = {
    .name = "mock", .cli_send = mock_send, .cli_close = mock_close, .cli_dispatch_resp = mock_dispatch,
    .cli_serve_static = mock_static, .cli_cleanup = mock_cleanup,
};

static void kat_http(void) {
    const char *req = "POST /api/v1/set?x=1&y=2 HTTP/1.1\r\nHost: dev\r\nContent-Length: 5\r\nX-Token: abc\r\n\r\nhello";
    client_socket_ctx_t *c = calloc(1, sizeof(*c));
    c->vt = &mock_vt;
    luat_httpsrv_parser_init(c);
    size_t half = 17;  /* split inside the URI: it must be accumulated across segments */
    luat_httpsrv_parser_exec(c, req, half);
    luat_httpsrv_parser_exec(c, req + half, strlen(req) - half);
    printf("http_done=%u\n", c->recv_done);
    printf("http_uri=%s\n", c->uri ? c->uri : "(null)");
    printf("http_method=%s\n", http_method_str(c->parser.method));
    printf("http_expect=%zu\n", c->expect_body_size);
    printf("http_body=%.*s\n", (int)c->body_size, c->body ? c->body : "");
    int n = 0;
    for (http_req_header_t *h = c->req_headers; h; h = h->next) n++;
    printf("http_headers=%d\n", n);
    luat_httpsrv_free_common(c);
    free(c);
}

static void kat_miniz(void) {
    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    luaopen_miniz(L); lua_setglobal(L, "miniz");
    const char *src =
        "local z = \"\\x78\\x9c\\xcb\\x48\\xcd\\xc9\\xc9\\x07\\x00\\x06\\x2c\\x02\\x15\"\n"
        "local s = '' for i = 1, 40 do s = s .. 'abc' end\n"
        "local c = miniz.compress(s)\n"
        "return miniz.uncompress(z), miniz.uncompress(c) == s, #c < #s";
    if (luaL_loadstring(L, src) != LUA_OK || lua_pcall(L, 0, 3, 0) != LUA_OK) {
        printf("miniz_error=%s\n", lua_tostring(L, -1));
    } else {
        printf("miniz_inflate=%s\n", lua_tostring(L, 1));
        printf("miniz_roundtrip=%d\n", lua_toboolean(L, 2));
        printf("miniz_shrinks=%d\n", lua_toboolean(L, 3));
    }
    lua_close(L);
}

static const char *hex_of(const char *s, size_t n, char *out) {
    for (size_t i = 0; i < n; i++) sprintf(out + 2 * i, "%02x", (unsigned char)s[i]);
    out[2 * n] = 0;
    return out;
}

static void kat_libs(void) {
    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    luaL_requiref(L, "_G", luaopen_base, 1); lua_pop(L, 1);
    luaL_requiref(L, "string", luaopen_string, 1); lua_pop(L, 1);
    luaopen_fastlz(L); lua_setglobal(L, "fastlz");
    luaopen_pack(L); lua_setglobal(L, "pack");
    luaopen_protobuf(L); lua_setglobal(L, "protobuf");
    luaopen_ercoap(L); lua_setglobal(L, "ercoap");
    lua_pushlstring(L, (const char *)fuzz_schema_pb, sizeof(fuzz_schema_pb)); lua_setglobal(L, "schema");
    const char *src =
        "local function hex(s) return (s:gsub('.', function(c) return string.format('%02x', c:byte()) end)) end\n"
        "local s = string.rep('abc', 50)\n"
        "local c = fastlz.compress(s, 1)\n"
        "local r = {}\n"
        "r[1] = tostring(c ~= nil and #c < #s and fastlz.uncompress(c, 300) == s)\n"
        "r[2] = tostring(fastlz.uncompress('\\x04hello', 100))\n"
        "r[3] = hex(pack.pack('>HI', 258, 65536))\n"
        "local n, a, b = pack.unpack('\\x01\\x02\\x00\\x01\\x00\\x00', '>HI')\n"
        "r[4] = n .. ',' .. a .. ',' .. b\n"
        "protobuf.load(schema)\n"
        "local e = protobuf.encode('Fuzz', {i32 = 150, str = 'hi'})\n"
        "r[5] = #e\n"
        "local d = protobuf.decode('Fuzz', e)\n"
        "r[6] = d.i32 .. ',' .. d.str\n"
        "local p = ercoap.parse('\\x40\\x01\\x12\\x34\\xb1\\x61\\xff\\x68\\x69')\n"
        "r[7] = p.type .. ',' .. p.code .. ',' .. p.msgid .. ',' .. p.payload\n"
        "return r[1], r[2], r[3], r[4], r[5], r[6], r[7]";
    if (luaL_loadstring(L, src) != LUA_OK || lua_pcall(L, 0, 7, 0) != LUA_OK) {
        printf("libs_error=%s\n", lua_tostring(L, -1));
    } else {
        printf("fastlz_roundtrip=%s\n", lua_tostring(L, 1));
        printf("fastlz_literal=%s\n", lua_tostring(L, 2));
        printf("pack_pack=%s\n", lua_tostring(L, 3));
        printf("pack_unpack=%s\n", lua_tostring(L, 4));
        printf("pb_encode_len=%s\n", lua_tostring(L, 5));
        printf("pb_decode=%s\n", lua_tostring(L, 6));
        printf("coap_parse=%s\n", lua_tostring(L, 7));
    }
    lua_close(L);
}

static void kat_gnss(void) {
    luat_libgnss_init(1);
    const char *rmc = "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n";
    luat_libgnss_parse_data(rmc, strlen(rmc));
    printf("gnss_rmc=%.*s\n", gnssctx.rmc ? (int)strcspn(gnssctx.rmc->data, "\r\n") : 6, gnssctx.rmc ? gnssctx.rmc->data : "(none)");
    luat_libgnss_parse_data("$GPRMC,1", 8);
    luat_libgnss_parse_data("23519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\n", 62);
    printf("gnss_split=%d\n", gnssctx.rmc && strstr(gnssctx.rmc->data, "123519") != NULL);
}

static void kat_cjson(void) {
    cJSON *j = cJSON_Parse("{ \"a\" : [1, 2.5, \"x\"], \"b\": {\"c\": true, \"d\": null} }");
    char *p = j ? cJSON_PrintUnformatted(j) : NULL;
    printf("cjson_print=%s\n", p ? p : "(null)");
    free(p);
    cJSON_Delete(j);
    printf("cjson_reject=%d\n", cJSON_Parse("{\"a\":") == NULL);
}

static void kat_nanopb(void) {
    static const uint8_t wire[] = {0x08, 0x05, 0x10, 0x01, 0x28, 0x02, 0x38, 0x14};
    drv_gpio_GpioSetupRequest m = drv_gpio_GpioSetupRequest_init_zero;
    pb_istream_t in = pb_istream_from_buffer(wire, sizeof(wire));
    int ok = pb_decode(&in, drv_gpio_GpioSetupRequest_fields, &m);
    printf("nanopb_decode=%d pin=%u mode=%d irq=%d debounce=%u\n", ok, (unsigned)m.pin, (int)m.mode, (int)m.irq_type, (unsigned)m.debounce_ms);
    pb_istream_t bad = pb_istream_from_buffer(wire, 3);
    printf("nanopb_truncated=%d\n", pb_decode(&bad, drv_gpio_GpioSetupRequest_fields, &m));
}

typedef struct { const unsigned char *p; size_t n, pos; } kat_src_t;
static size_t kat_in(JDEC *jd, uint8_t *buf, size_t len) {
    kat_src_t *s = (kat_src_t *)jd->device;
    if (len > s->n - s->pos) len = s->n - s->pos;
    if (buf) memcpy(buf, s->p + s->pos, len);
    s->pos += len;
    return len;
}
static unsigned long kat_px, kat_sum;
static int kat_out(JDEC *jd, void *bmp, JRECT *r) {
    (void)jd;
    size_t n = ((size_t)r->right - r->left + 1) * ((size_t)r->bottom - r->top + 1);
    kat_px += (unsigned long)n;
    const unsigned char *b = (const unsigned char *)bmp;
    for (size_t i = 0; i < n * 2; i++) kat_sum += b[i];
    return 1;
}
static void kat_tjpgd(void) {
    static uint8_t pool[40960];
    JDEC jd;
    kat_src_t src = { kat_jpeg, sizeof(kat_jpeg), 0 };
    int rc = luat_jd_prepare(&jd, kat_in, pool, sizeof(pool), &src);
    printf("jpeg_prepare=%d size=%ux%u\n", rc, jd.width, jd.height);
    rc = luat_jd_decomp(&jd, kat_out, 0);
    printf("jpeg_decomp=%d pixels=%lu sum=%lu\n", rc, kat_px, kat_sum);
}

static void kat_iconv(void) {
    static const struct { const char *to, *from; const char *in; size_t n; const char *name; } t[] = {
        { "ucs2",   "utf8",   "\xE4\xBD\xA0\xE5\xA5\xBD", 6, "utf8_to_ucs2" },
        { "ucs2be", "gb2312", "\xC4\xE3\xBA\xC3",           4, "gb2312_to_ucs2be" },
        { "utf8",   "ucs2",   "\x60\x4F\x7D\x59",           4, "ucs2_to_utf8" },
        { "gb2312", "ucs2",   "\x60\x4F\x7D\x59",           4, "ucs2_to_gb2312" },
    };
    for (size_t i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        char out[32] = {0}, *ip = (char *)t[i].in, *op = out;
        size_t il = t[i].n, ol = sizeof(out);
        iconv_t cd = iconv_open(t[i].to, t[i].from);
        size_t rc = iconv_convert(cd, &ip, &il, &op, &ol);
        printf("iconv_%s=%d ", t[i].name, (int)rc);
        (void)op;
        for (char *p = out; p < out + (sizeof(out) - ol); p++) printf("%02X", (unsigned char)*p);
        printf("\n");
    }
    printf("iconv_unknown=%d\n", iconv_open("utf8", "latin1") == (iconv_t)-1);
}

void kat_ymodem(void);
void kat_mqtt(void);
void kat_http_client(void);
void kat_webp(void);
void kat_codec(void);

int main(void) {
    kat_lua();
    kat_minmea();
    kat_sms();
    kat_http();
    kat_miniz();
    kat_libs();
    kat_gnss();
    kat_cjson();
    kat_nanopb();
    kat_tjpgd();
    kat_iconv();
    kat_ymodem();
    kat_mqtt();
    kat_http_client();
    kat_webp();
    kat_codec();
    printf("kat_done=1\n");
    return 0;
}
