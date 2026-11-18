// Shared driver for the codec.* harnesses (fuzz_codec_mp3, _amr, _wav, _speex): the input is an audio file
// that a script opens with the Lua API of components/multimedia/luat_lib_multimedia_codec.c,
// codec.create(type) -> codec.info(coder, path) -> codec.data(coder, zbuff) until it returns no more data.
// The file lives on the firmware's own RAM file system (/ram/), so nothing touches the host disk.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "luat_base.h"
#include "luat_malloc.h"
#include "luat_fs.h"
#include "luat_zbuff.h"
#include "luat_multimedia_codec.h"

int luaopen_multimedia_codec(lua_State *L);
extern const struct luat_vfs_filesystem vfs_fs_ram;

#define CODEC_FILE "/ram/in"
#define CODEC_OUT_LEN 16384

static void codec_fs_init(void) {
    luat_vfs_init(NULL);
    luat_vfs_reg(&vfs_fs_ram);
    luat_fs_conf_t conf = {.busname = "", .type = "ram", .filesystem = "ram", .mount_point = "/ram/"};
    luat_fs_mount(&conf);
}

static int codec_run(int type, const uint8_t *data, size_t size) {
    FILE *f = luat_fs_fopen(CODEC_FILE, "wb");
    if (f == NULL) return -1;
    if (size > 0) luat_fs_fwrite(data, 1, size, f);
    luat_fs_fclose(f);

    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    if (L == NULL) { luat_fs_remove(CODEC_FILE); return -1; }
    luaopen_multimedia_codec(L);
    int lib = lua_gettop(L);

    int ok = 0;
    lua_getfield(L, lib, "create");
    lua_pushinteger(L, type);
    if (lua_pcall(L, 1, 1, 0) == LUA_OK && lua_isuserdata(L, -1)) {
        int coder = lua_gettop(L);
        lua_getfield(L, lib, "info");
        lua_pushvalue(L, coder);
        lua_pushstring(L, CODEC_FILE);
        if (lua_pcall(L, 2, 6, 0) == LUA_OK && lua_toboolean(L, -6)) {
            lua_settop(L, coder);
            luat_zbuff_t *out = (luat_zbuff_t *)lua_newuserdata(L, sizeof(luat_zbuff_t));
            memset(out, 0, sizeof(*out));
            luaL_newmetatable(L, LUAT_ZBUFF_TYPE);
            lua_setmetatable(L, -2);
            out->addr = (uint8_t *)luat_heap_malloc(CODEC_OUT_LEN);
            out->len = CODEC_OUT_LEN;
            int zb = lua_gettop(L);
            if (out->addr != NULL) {
                ok = 1;
                for (;;) {
                    lua_getfield(L, lib, "data");
                    lua_pushvalue(L, coder);
                    lua_pushvalue(L, zb);
                    lua_pushinteger(L, 4096);
                    if (lua_pcall(L, 3, 1, 0) != LUA_OK || !lua_toboolean(L, -1) || out->used == 0) break;
                    lua_pop(L, 1);
                }
                luat_heap_free(out->addr);
            }
        }
        lua_settop(L, coder);
        lua_getfield(L, lib, "release");
        lua_pushvalue(L, coder);
        lua_pcall(L, 1, 0, 0);
    }
    lua_close(L);
    luat_fs_remove(CODEC_FILE);
    return ok ? 0 : -1;
}
