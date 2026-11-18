// Fuzz the YMODEM receiver (components/ymodem): luat_ymodem_receive takes the bytes a peer sends over the
// serial link, assembles SOH/STX packets, checks the header, sequence number and CRC16, parses the file name
// and length out of the first packet and hands the file data on. Built in the library's callback mode
// (no __LUATOS__), where the received data goes to a callback instead of the filesystem, so nothing is written.
// The input is a sequence of reads, each a 2-byte little-endian length then that many bytes (as UART reads
// arrive; a packet may span reads and one call handles one packet). A full session is in the seeds; the CRC16
// gate is not worked around here.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "luat_base.h"
#include "luat_ymodem.h"

static uint64_t sink_sum;
static void sink(uint8_t *d, uint32_t n) {
    for (uint32_t i = 0; d && i < n; i++) sink_sum += d[i];   /* reads every byte handed over */
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    void *h = luat_ymodem_create_handler(sink);
    if (h == NULL) return -1;
    int reads = 0;
    while (size >= 3) {
        size_t n = data[0] | ((size_t)data[1] << 8);
        data += 2; size -= 2;
        if (n > size) n = size;
        if (n > 0) {   /* the library's contract: a read is at least one byte (NULL/0 is only the handshake poll) */
            uint8_t *chunk = (uint8_t *)malloc(n);   /* exact-size heap copy so reads past the chunk are caught */
            if (chunk == NULL) break;
            memcpy(chunk, data, n);
            uint8_t ack, flag, file_ok, all_done;
            luat_ymodem_receive(h, chunk, (uint32_t)n, &ack, &flag, &file_ok, &all_done);
            free(chunk);
            reads++;
        }
        data += n; size -= n;
    }
    luat_ymodem_release(h);
    return reads ? 0 : -1;
}
