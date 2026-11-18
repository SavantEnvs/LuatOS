// Fuzz the FastLZ decompressor behind fastlz.uncompress (components/fastlz) through its Lua API.
// The first two input bytes are the `maxout` argument (little endian), the rest is the compressed stream.
#include <stddef.h>
#include <stdint.h>
#include "lua.h"
#include "lauxlib.h"
#include "luat_base.h"
#include "luat_malloc.h"
#include "lualib.h"

int luaopen_fastlz(lua_State *L);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 2) return -1;
    lua_Integer maxout = data[0] | (data[1] << 8);
    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    if (L == NULL) return -1;
    luaopen_fastlz(L);
    lua_getfield(L, -1, "uncompress");
    lua_pushlstring(L, (const char *)data + 2, size - 2);
    lua_pushinteger(L, maxout);
    int rc = lua_pcall(L, 2, 1, 0);
    int ok = (rc == LUA_OK) && lua_isstring(L, -1);
    lua_close(L);
    return ok ? 0 : -1;
}
