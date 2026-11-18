// Fuzz LuatOS's `miniz` Lua binding (components/miniz/luat_lib_miniz.c): miniz.uncompress() on
// attacker bytes, and a miniz.compress() -> miniz.uncompress() round trip that must return its input.
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "luat_base.h"
#include "luat_malloc.h"

int luaopen_miniz(lua_State *L);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    if (L == NULL) return -1;
    luaopen_miniz(L);
    int mod = lua_gettop(L);
    int ret = -1;

    lua_getfield(L, mod, "uncompress");
    lua_pushlstring(L, (const char *)data, size);
    if (lua_pcall(L, 1, 1, 0) == LUA_OK && lua_isstring(L, -1)) ret = 0;
    lua_settop(L, mod);

    lua_getfield(L, mod, "compress");
    lua_pushlstring(L, (const char *)data, size);
    if (lua_pcall(L, 1, 1, 0) == LUA_OK && lua_isstring(L, -1)) {
        lua_getfield(L, mod, "uncompress");
        lua_pushvalue(L, -2);
        if (lua_pcall(L, 1, 1, 0) != LUA_OK || !lua_isstring(L, -1)) abort();
        size_t n = 0;
        const char *back = lua_tolstring(L, -1, &n);
        if (n != size || (size && memcmp(back, data, size) != 0)) abort();
    }
    lua_close(L);
    return ret;
}
