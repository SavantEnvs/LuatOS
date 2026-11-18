// Fuzz the LuatOS `pack` library (luat/modules/luat_lib_pack.c) through its Lua API. The first input
// byte is the length of the format string that follows; the remainder is the data. pack.unpack(data, fmt)
// is run, and when it yields values they are packed back with the same format.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "luat_base.h"
#include "luat_malloc.h"
#include "lualib.h"

int luaopen_pack(lua_State *L);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return -1;
    size_t flen = data[0];
    if (flen > size - 1) flen = size - 1;
    const char *fmt = (const char *)data + 1;
    const char *bin = fmt + flen;
    size_t blen = size - 1 - flen;

    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    if (L == NULL) return -1;
    luaopen_pack(L);
    int lib = lua_gettop(L);

    lua_getfield(L, lib, "unpack");
    lua_pushlstring(L, bin, blen);
    lua_pushlstring(L, fmt, flen);
    int ok = 0;
    if (lua_pcall(L, 2, LUA_MULTRET, 0) == LUA_OK) {
        int nres = lua_gettop(L) - lib;   /* next position, then one value per format item */
        ok = nres > 1;
        /* lua_checkstack: the Lua API requires room before pushing an unbounded number of values (pack, fmt, one per result). */
        if (!lua_checkstack(L, nres + 4)) { lua_close(L); return -1; }
        lua_getfield(L, lib, "pack");
        lua_pushlstring(L, fmt, flen);
        for (int i = 2; i <= nres; i++) lua_pushvalue(L, lib + i);
        lua_pcall(L, nres, 1, 0);
    }
    lua_close(L);
    return ok ? 0 : -1;
}
