// Fuzz the LuatOS `iconv` library (components/iconv) through its Lua API: iconv.open(to, from) then
// iconv.iconv(cd, str), which converts an arbitrary byte string between GB2312, UTF-8, UCS-2 and UCS-2BE
// through the table-driven and variable-length decoders. The low three bits of the first input byte pick one
// of the eight conversions the library supports; the rest is the string to convert.
#include <stddef.h>
#include <stdint.h>
#include "lua.h"
#include "lauxlib.h"
#include "luat_base.h"
#include "luat_malloc.h"
#include "lualib.h"

int luaopen_iconv(lua_State *L);

/* The (to, from) pairs of iconv_map in components/iconv/iconv.c. */
static const char *const pairs[8][2] = {
    {"gb2312", "ucs2"}, {"gb2312", "ucs2be"}, {"ucs2", "gb2312"}, {"ucs2be", "gb2312"},
    {"ucs2", "utf8"}, {"ucs2be", "utf8"}, {"utf8", "ucs2"}, {"utf8", "ucs2be"},
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 1) return -1;
    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    if (L == NULL) return -1;
    luaopen_iconv(L);
    int lib = lua_gettop(L);

    lua_getfield(L, lib, "open");
    lua_pushstring(L, pairs[data[0] & 7][0]);
    lua_pushstring(L, pairs[data[0] & 7][1]);
    int ok = 0;
    if (lua_pcall(L, 2, 1, 0) == LUA_OK && lua_isuserdata(L, -1)) {
        int cd = lua_gettop(L);
        lua_getfield(L, lib, "iconv");
        lua_pushvalue(L, cd);
        lua_pushlstring(L, (const char *)data + 1, size - 1);
        ok = lua_pcall(L, 2, 1, 0) == LUA_OK;
    }
    lua_close(L);
    return ok ? 0 : -1;
}
