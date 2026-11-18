// Stand-ins for the HTTP client (components/network/libhttp). A separate object, so that only the harnesses that
// link luat_http_client.c pull it in. No target logic lives here.
#include <stddef.h>
#include <stdint.h>
#include "luat_base.h"
#include "luat_rtos.h"
#include "luat_network_adapter.h"

/* The HTTP client delivers its results to the Lua binding through the message bus; the consumer there reads the
 * message and frees it. mayhem_http_event_hook lets the known-answer test read the result first. */
#include "luat_http.h"
void (*mayhem_http_event_hook)(luat_http_msg_t *msg);
int luat_http_client_onevent(luat_http_msg_t *msg) {
    if (msg == NULL) return -1;
    if (mayhem_http_event_hook) mayhem_http_event_hook(msg);
    luat_http_msg_free(msg);
    return 0;
}
void luat_http_fail_notify(uint32_t idg, int error_code) { (void)idg; (void)error_code; }
int luat_rtos_timer_stop(luat_rtos_timer_t t) { (void)t; return 0; }
int network_register_get_default(void) { return 0; }
