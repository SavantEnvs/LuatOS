// Fuzz the NMEA 0183 sentence parser used by libgnss (components/minmea).
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "minmea.h"

// The conversions luat_lib_libgnss.c applies to parsed fields (push_gnss_value): tofloat for every value,
// tocoord/tocoord2 for coordinates.
static volatile float sink_f;
static volatile uint32_t sink_u;
static void use_value(const struct minmea_float *f) { sink_f = minmea_tofloat(f); }
static void use_coord(const struct minmea_float *f) { sink_f = minmea_tocoord(f); sink_u = minmea_tocoord2(f); }

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size == 0 || size > 255) return -1;
    char *line = (char *)malloc(size + 1);
    if (line == NULL) return -1;
    memcpy(line, data, size);
    line[size] = '\0';

    int ret = -1;
    char talker[3];
    minmea_talker_id(talker, line);
    minmea_check(line, false);
    switch (minmea_sentence_id(line, false)) {
    case MINMEA_SENTENCE_RMC: {
        struct minmea_sentence_rmc f;
        if (minmea_parse_rmc(&f, line)) {
            struct tm tm;
            minmea_getdatetime(&tm, &f.date, &f.time);
            use_coord(&f.latitude); use_coord(&f.longitude);
            use_value(&f.speed); use_value(&f.course); use_value(&f.variation);
            ret = 0;
        }
        break; }
    case MINMEA_SENTENCE_GGA: { struct minmea_sentence_gga f; if (minmea_parse_gga(&f, line)) { use_coord(&f.latitude); use_coord(&f.longitude); use_value(&f.hdop); use_value(&f.altitude); use_value(&f.height); use_value(&f.dgps_age); ret = 0; } break; }
    case MINMEA_SENTENCE_GSA: { struct minmea_sentence_gsa f; if (minmea_parse_gsa(&f, line)) { use_value(&f.pdop); use_value(&f.hdop); use_value(&f.vdop); ret = 0; } break; }
    case MINMEA_SENTENCE_GLL: { struct minmea_sentence_gll f; if (minmea_parse_gll(&f, line)) { use_coord(&f.latitude); use_coord(&f.longitude); ret = 0; } break; }
    case MINMEA_SENTENCE_GST: { struct minmea_sentence_gst f; if (minmea_parse_gst(&f, line)) ret = 0; break; }
    case MINMEA_SENTENCE_GSV: { struct minmea_sentence_gsv f; if (minmea_parse_gsv(&f, line)) ret = 0; break; }
    case MINMEA_SENTENCE_VTG: { struct minmea_sentence_vtg f; if (minmea_parse_vtg(&f, line)) { use_value(&f.true_track_degrees); use_value(&f.magnetic_track_degrees); use_value(&f.speed_knots); use_value(&f.speed_kph); ret = 0; } break; }
    case MINMEA_SENTENCE_ZDA: { struct minmea_sentence_zda f; if (minmea_parse_zda(&f, line)) ret = 0; break; }
    default: break;
    }
    free(line);
    return ret;
}
