// Fuzz libgnss's stream-level NMEA entry (components/minmea/luat_gnss.c): luat_libgnss_parse_data takes the
// raw bytes the GNSS UART delivers, splits them into lines, and files each sentence in the library's
// global state. The state it keeps is reset for every input.
#include <stdint.h>
#include <string.h>
#include "luat_base.h"
#include "luat_uart.h"
#include "minmea.h"

extern luat_libgnss_t gnssctx;

int luat_libgnss_on_rawdata(const char *d, size_t n, int type) { (void)d; (void)n; (void)type; return 0; }
int luat_libgnss_state_onchanged(int state) { (void)state; return 0; }
int luat_uart_read(int id, void *buf, size_t n) { (void)id; (void)buf; (void)n; return 0; }
int luat_uart_write(int id, void *buf, size_t n) { (void)id; (void)buf; (void)n; return 0; }
int luat_rtc_set(struct tm *t) { (void)t; return 0; }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0) return -1;
    luat_libgnss_init(1);
    gnssctx.gsa_offset = 0;
    gnssctx.rtc_auto = 1;
    luat_libgnss_parse_data((const char *)data, size);
    luat_libgnss_parse_data("\n", 1);   /* complete a trailing partial line so no bytes carry over */
    minmea_data_t *kept[] = { gnssctx.rmc, gnssctx.gga, gnssctx.gll, gnssctx.gst, gnssctx.vtg, gnssctx.zda, gnssctx.txt,
                              gnssctx.gsv[0], gnssctx.gsa[0] };
    for (size_t i = 0; i < sizeof(kept) / sizeof(kept[0]); i++)
        if (kept[i] && kept[i]->data[0]) return 0;
    return -1;
}
