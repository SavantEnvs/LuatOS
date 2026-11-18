// Fuzz the LuatOS `protobuf` library (components/serialization/protobuf) through its Lua API,
// with the input used twice: as a wire-format message decoded against a fixed schema that covers
// every field kind (protobuf.decode, then protobuf.encode of the result), and as a compiled
// FileDescriptorSet (protobuf.load).
#include <stddef.h>
#include <stdint.h>
#include "lua.h"
#include "lauxlib.h"
#include "luat_base.h"
#include "luat_malloc.h"
#include "lualib.h"
#include "fuzz_protobuf_schema.h"

int luaopen_protobuf(lua_State *L);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    if (L == NULL) return -1;
    luaopen_protobuf(L);
    int lib = lua_gettop(L);
    int ok = 0;

    lua_getfield(L, lib, "load");
    lua_pushlstring(L, (const char *)fuzz_schema_pb, sizeof(fuzz_schema_pb));
    lua_pcall(L, 1, 2, 0);
    lua_settop(L, lib);

    lua_getfield(L, lib, "decode");
    lua_pushstring(L, "Fuzz");
    lua_pushlstring(L, (const char *)data, size);
    if (lua_pcall(L, 2, 1, 0) == LUA_OK && lua_istable(L, -1)) {
        ok = 1;
        lua_getfield(L, lib, "encode");
        lua_pushstring(L, "Fuzz");
        lua_pushvalue(L, -3);
        lua_pcall(L, 2, 1, 0);
    }
    lua_settop(L, lib);

    lua_getfield(L, lib, "clear");
    lua_pcall(L, 0, 0, 0);
    lua_settop(L, lib);

    lua_getfield(L, lib, "load");
    lua_pushlstring(L, (const char *)data, size);
    if (lua_pcall(L, 1, 2, 0) == LUA_OK && lua_toboolean(L, -2)) ok = 1;
    lua_settop(L, lib);

    lua_getfield(L, lib, "clear");
    lua_pcall(L, 0, 0, 0);
    lua_close(L);
    return ok ? 0 : -1;
}
