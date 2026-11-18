// Fuzz cJSON (components/cjson, built into the PC firmware and used for config/web/agent JSON), which is
// a different parser from lua-cjson. The first input byte is the length of a first document; the rest is a
// second one; each is copied into a NUL-terminated string, as the firmware callers do, and parsed with
// cJSON_Parse, printed and duplicated. When both parse, JSON patches and merge patches are generated
// between them and applied back (cJSON_Utils).
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "cJSON_Utils.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return -1;
    size_t alen = data[0];
    if (alen > size - 1) alen = size - 1;
    size_t blen = size - 1 - alen;

    /* Callers (luat_pcconf_pc.c, luat_web_*.c, luat_agent_ctl.c) hand cJSON_Parse a NUL-terminated string. */
    char *a_txt = malloc(alen + 1), *b_txt = malloc(blen + 1);
    if (a_txt == NULL || b_txt == NULL) { free(a_txt); free(b_txt); return -1; }
    memcpy(a_txt, data + 1, alen); a_txt[alen] = '\0';
    memcpy(b_txt, data + 1 + alen, blen); b_txt[blen] = '\0';
    cJSON *a = cJSON_Parse(a_txt);
    cJSON *b = cJSON_Parse(b_txt);
    free(a_txt);
    free(b_txt);
    cJSON *docs[2] = { a, b };
    for (int i = 0; i < 2; i++) {
        if (docs[i] == NULL) continue;
        char *p = cJSON_Print(docs[i]);
        free(p);
        p = cJSON_PrintUnformatted(docs[i]);
        free(p);
        cJSON *d = cJSON_Duplicate(docs[i], 1);
        if (d != NULL) {
            cJSONUtils_SortObject(d);
            cJSON_Compare(docs[i], d, 0);
            cJSON_Delete(d);
        }
    }
    if (a != NULL && b != NULL) {
        cJSON *patches = cJSONUtils_GeneratePatches(a, b);
        if (patches != NULL) {
            cJSON *target = cJSON_Duplicate(a, 1);
            if (target != NULL) {
                cJSONUtils_ApplyPatches(target, patches);
                cJSON_Delete(target);
            }
            cJSON_Delete(patches);
        }
        cJSON *mp = cJSONUtils_GenerateMergePatch(a, b);
        if (mp != NULL) {
            cJSON *target = cJSON_Duplicate(a, 1);
            if (target != NULL) {
                target = cJSONUtils_MergePatch(target, mp);
                cJSON_Delete(target);
            }
            cJSON_Delete(mp);
        }
    }
    cJSON_Delete(a);
    cJSON_Delete(b);
    return (a != NULL || b != NULL) ? 0 : -1;
}
