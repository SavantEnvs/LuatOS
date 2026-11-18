// Fuzz the LuatOS Lua VM front end: the text lexer/parser/code generator
// (luaL_loadbufferx, mode "t"). Nothing is executed and no host library is
// registered, so the guest cannot reach the host. Precompiled chunks are left out:
// Lua's bytecode loader is documented as unsafe for crafted input (no verifier) and
// crashes in LoadConstants on the first mutated chunk.
#include <stddef.h>
#include <stdint.h>
#include "lua.h"
#include "lauxlib.h"
#include "luat_base.h"
#include "luat_malloc.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    if (L == NULL) return -1;
    int rc = luaL_loadbufferx(L, (const char *)data, size, "=fuzz", "t");
    if (rc != LUA_OK) {
        lua_close(L);
        return -1;
    }
    lua_close(L);
    return 0;
}
