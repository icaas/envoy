#include "common/http/filter/lua/wrappers.h"

namespace Envoy {
namespace Http {
namespace Filter {
namespace Lua {

int HeaderMapWrapper::add(lua_State*) { return 0; }

int HeaderMapWrapper::get(lua_State* state) {
  const char* key = luaL_checkstring(state, 2);
  const HeaderEntry* entry = headers_.get(LowerCaseString(key));
  if (entry != nullptr) {
    lua_pushstring(state, entry->value().c_str());
    return 1;
  } else {
    return 0;
  }
}

int HeaderMapWrapper::iterate(lua_State* state) {
  luaL_checktype(state, 2, LUA_TFUNCTION);
  headers_.iterate(
      [](const HeaderEntry& header, void* context) -> void {
        // fixfix
        lua_State* state = static_cast<lua_State*>(context);
        lua_pushvalue(state, -1);
        lua_pushstring(state, header.key().c_str());
        lua_pushstring(state, header.value().c_str());
        lua_pcall(state, 2, 0, 0);
      },
      state);

  return 0;
}

int HeaderMapWrapper::remove(lua_State*) { return 0; }

} // namespace Lua
} // namespace Filter
} // namespace Http
} // namespace Envoy
