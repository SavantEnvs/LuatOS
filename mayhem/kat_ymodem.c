// Known-answer probe for the YMODEM receiver (see kat.c), built in the library's callback mode: one file
// ("a.bin", 5 bytes) is received through a header packet, a data packet and EOT, and a corrupted packet is refused.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "luat_base.h"
#include "luat_ymodem.h"

static char cb_name[32], cb_data[32];
static uint32_t cb_size, cb_len, cb_calls;
static void cb(uint8_t *d, uint32_t n) {
    cb_calls++;
    if (d && n == 0) snprintf(cb_name, sizeof cb_name, "%s", (char *)d);   /* header: file name */
    else if (!d) cb_size = n;                                              /* header: file size */
    else if (n) { memcpy(cb_data + cb_len, d, n); cb_len += n; }          /* file data */
}

static uint16_t crc16(const uint8_t *p, int n) {   /* CRC-16/XMODEM: poly 0x1021, init 0 */
    uint16_t c = 0;
    while (n--) { c ^= (uint16_t)*p++ << 8; for (int i = 0; i < 8; i++) c = (c & 0x8000) ? (c << 1) ^ 0x1021 : c << 1; }
    return c;
}
static int mkpkt(uint8_t *out, uint8_t sn, const uint8_t *payload, int plen) {
    out[0] = 0x01; out[1] = sn; out[2] = (uint8_t)(255 - sn);
    memset(out + 3, 0, 128);
    memcpy(out + 3, payload, plen);
    uint16_t c = crc16(out + 3, 128);
    out[131] = c >> 8; out[132] = c & 0xff;
    return 133;
}

void kat_ymodem(void) {
    uint8_t pkt[133], ack, flag, ok, done;
    void *h = luat_ymodem_create_handler(cb);
    luat_ymodem_receive(h, NULL, 0, &ack, &flag, &ok, &done);
    printf("ymodem_handshake=%c\n", ack);
    int n = mkpkt(pkt, 0, (const uint8_t *)"a.bin\0" "5\0", 8);
    int rc = luat_ymodem_receive(h, pkt, n, &ack, &flag, &ok, &done);
    printf("ymodem_header=%d ack=%u flag=%c name=%s size=%u\n", rc, ack, flag, cb_name, cb_size);
    n = mkpkt(pkt, 1, (const uint8_t *)"hello", 5);   /* the header was packet 0; data packets count from 1 */
    pkt[131] ^= 0xff;   /* corrupt the CRC */
    rc = luat_ymodem_receive(h, pkt, n, &ack, &flag, &ok, &done);
    printf("ymodem_badcrc=%d ack=%u\n", rc, ack);
    n = mkpkt(pkt, 1, (const uint8_t *)"hello", 5);
    rc = luat_ymodem_receive(h, pkt, 40, &ack, &flag, &ok, &done);    /* split across two reads */
    int part_ack = ack;
    rc = luat_ymodem_receive(h, pkt + 40, n - 40, &ack, &flag, &ok, &done);
    printf("ymodem_data=%d part_ack=%d ack=%u file_ok=%u got=%s len=%u\n", rc, part_ack, ack, ok, cb_data, cb_len);
    uint8_t eot = 0x04;
    luat_ymodem_receive(h, &eot, 1, &ack, &flag, &ok, &done);
    printf("ymodem_eot1=%u\n", ack);
    luat_ymodem_receive(h, &eot, 1, &ack, &flag, &ok, &done);
    printf("ymodem_eot2=%u flag=%c\n", ack, flag);
    n = mkpkt(pkt, 0, (const uint8_t *)"", 0);
    rc = luat_ymodem_receive(h, pkt, n, &ack, &flag, &ok, &done);
    printf("ymodem_end=%d ack=%u all_done=%u\n", rc, ack, done);
    luat_ymodem_release(h);
}
