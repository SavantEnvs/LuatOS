// Known-answer probe for the codec.* Lua API (wav, mp3, amr): each file is written to the RAM file system,
// opened with codec.create / codec.info, and decoded with codec.data. The audio format reported by codec.info and the
// number and content of the decoded bytes must match what the files encode.
#include <stdio.h>
#include <string.h>
#include "codec_harness.h"

static void codec_one(const char *name, int type, const uint8_t *d, size_t n) {
    FILE *f = luat_fs_fopen(CODEC_FILE, "wb");
    luat_fs_fwrite(d, 1, n, f);
    luat_fs_fclose(f);

    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    luaopen_multimedia_codec(L);
    int lib = lua_gettop(L);
    lua_getfield(L, lib, "create");
    lua_pushinteger(L, type);
    lua_pcall(L, 1, 1, 0);
    int coder = lua_gettop(L);
    lua_getfield(L, lib, "info");
    lua_pushvalue(L, coder);
    lua_pushstring(L, CODEC_FILE);
    lua_pcall(L, 2, 6, 0);
    int ok = lua_toboolean(L, -6);
    printf("codec_%s=info=%d channels=%d rate=%d bits=%d", name, ok, (int)lua_tointeger(L, -4), (int)lua_tointeger(L, -3), (int)lua_tointeger(L, -2));
    lua_settop(L, coder);
    if (ok) {
        luat_zbuff_t *out = (luat_zbuff_t *)lua_newuserdata(L, sizeof(luat_zbuff_t));
        memset(out, 0, sizeof(*out));
        luaL_newmetatable(L, LUAT_ZBUFF_TYPE);
        lua_setmetatable(L, -2);
        out->addr = (uint8_t *)luat_heap_malloc(CODEC_OUT_LEN);
        out->len = CODEC_OUT_LEN;
        int zb = lua_gettop(L);
        size_t total = 0; unsigned long sum = 0; int calls = 0;
        for (;;) {
            lua_getfield(L, lib, "data");
            lua_pushvalue(L, coder);
            lua_pushvalue(L, zb);
            lua_pushinteger(L, 4096);
            if (lua_pcall(L, 3, 1, 0) != LUA_OK || !lua_toboolean(L, -1) || out->used == 0) break;
            lua_pop(L, 1);
            calls++;
            total += out->used;
            for (size_t i = 0; i < out->used; i++) sum += out->addr[i];
        }
        printf(" calls=%d bytes=%zu sum=%lu", calls, total, sum);
        luat_heap_free(out->addr);
    }
    printf("\n");
    lua_close(L);
    luat_fs_remove(CODEC_FILE);
}

void kat_codec(void) {
    codec_fs_init();
    static uint8_t wav[44 + 16000];
    memcpy(wav, "RIFF\xa4\x3e\x00\x00WAVEfmt \x10\x00\x00\x00\x01\x00\x01\x00\x40\x1f\x00\x00\x80\x3e\x00\x00\x02\x00\x10\x00" "data\x80\x3e\x00\x00", 44);
    for (int i = 0; i < 16000; i++) wav[44 + i] = (uint8_t)(i * 7);
    codec_one("wav", LUAT_MULTIMEDIA_DATA_TYPE_WAV, wav, sizeof wav);

    static uint8_t mp3[417 * 3];
    for (int i = 0; i < 3; i++) memcpy(mp3 + 417 * i, "\xff\xfb\x90\x00", 4);
    codec_one("mp3", LUAT_MULTIMEDIA_DATA_TYPE_MP3, mp3, sizeof mp3);

    static uint8_t nb[6 + 3 * 13], wb[9 + 3 * 18];
    memcpy(nb, "#!AMR\n", 6);
    for (int i = 0; i < 3; i++) { nb[6 + 13 * i] = 0x04; memset(nb + 7 + 13 * i, 0x5a, 12); }
    codec_one("amr_nb", LUAT_MULTIMEDIA_DATA_TYPE_AMR_NB, nb, sizeof nb);
    memcpy(wb, "#!AMR-WB\n", 9);
    for (int i = 0; i < 3; i++) { wb[9 + 18 * i] = 0x04; memset(wb + 10 + 18 * i, 0x5a, 17); }
    codec_one("amr_wb", LUAT_MULTIMEDIA_DATA_TYPE_AMR_WB, wb, sizeof wb);
    codec_one("amr_bad_magic", LUAT_MULTIMEDIA_DATA_TYPE_AMR_NB, wb, sizeof wb);
}
