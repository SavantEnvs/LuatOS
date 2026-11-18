// Fuzz the SMS-DELIVER / STATUS-REPORT PDU decoder (components/sms), the code
// that parses messages received from the cellular network.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "luat_sms.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0 || size > 0xFFFF) return -1;
    uint8_t *pdu = (uint8_t *)malloc(size);
    luat_sms_recv_msg_t *msg = (luat_sms_recv_msg_t *)calloc(1, sizeof(*msg));
    if (pdu == NULL || msg == NULL) { free(pdu); free(msg); return -1; }
    memcpy(pdu, data, size);
    int rc = luat_sms_pdu_message_unpack(msg, pdu, (int)size);
    if (rc == 0) {
        // The Lua binding treats the decoded text as a C string.
        volatile size_t n = strlen((const char *)msg->sms_buffer);
        (void)n;
    }
    free(pdu);
    free(msg);
    return rc == 0 ? 0 : -1;
}
