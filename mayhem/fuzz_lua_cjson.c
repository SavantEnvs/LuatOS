// Fuzz the LuatOS `json` library (components/lua-cjson) through its Lua API:
// json.decode(<bytes>) and, when that yields a value, json.encode(<value>).
#include <stddef.h>
#include <stdint.h>
#include "lua.h"
#include "lauxlib.h"
#include "luat_base.h"
#include "luat_malloc.h"
#include "lualib.h"

int luaopen_cjson(lua_State *l);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    if (L == NULL) return -1;
    luaopen_cjson(L);
    int top = lua_gettop(L);
    lua_getfield(L, top, "decode");
    lua_pushlstring(L, (const char *)data, size);
    int rc = lua_pcall(L, 1, 1, 0);
    int ok = (rc == LUA_OK) && !lua_isnil(L, -1);
    if (ok) {
        lua_getfield(L, top, "encode");
        lua_pushvalue(L, -2);
        lua_pcall(L, 1, 1, 0);
    }
    lua_close(L);
    return ok ? 0 : -1;
}
