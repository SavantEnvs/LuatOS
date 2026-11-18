// Fuzz the CoAP message parser (components/network/ercoap, coap_parse_message) through its Lua entry
// point ercoap.parse, which is what LuatOS scripts hand a raw UDP payload to. Datagrams shorter than the 4-byte
// fixed header are not CoAP messages (RFC 7252 section 3), so they are rejected here: coap_parse_message reads
// those four bytes without looking at its length argument. Everything after the header (token length, option
// deltas and lengths) is attacker-controlled and left to the parser, which is what the target is for.
#include <stddef.h>
#include <stdint.h>
#include "lua.h"
#include "lauxlib.h"
#include "luat_base.h"
#include "luat_malloc.h"
#include "lualib.h"

int luaopen_ercoap(lua_State *L);

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < 4) return -1;
    lua_State *L = lua_newstate(luat_heap_alloc, NULL);
    if (L == NULL) return -1;
    luaopen_ercoap(L);
    lua_getfield(L, -1, "parse");
    lua_pushlstring(L, (const char *)data, size);
    int ok = (lua_pcall(L, 1, 1, 0) == LUA_OK) && lua_istable(L, -1);
    lua_close(L);
    return ok ? 0 : -1;
}
