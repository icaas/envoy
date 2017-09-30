#include "common/lua/wrappers.h"

namespace Envoy {
namespace Lua {

int BufferWrapper::byteSize(lua_State* state) {
  // fixfix
  lua_pushnumber(state, data_.length());
  return 1;
}

} // namespace Lua
} // namespace Envoy
