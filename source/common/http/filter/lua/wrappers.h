#pragma once

#include "envoy/http/header_map.h"

#include "common/lua/lua.h"

namespace Envoy {
namespace Http {
namespace Filter {
namespace Lua {

/**
 * fixfix
 */
class HeaderMapWrapper : public Envoy::Lua::BaseLuaObject<HeaderMapWrapper> {
public:
  HeaderMapWrapper(HeaderMap& headers) : headers_(headers) {}

  static ExportedFunctions exportedFunctions() {
    return {{"add", static_add},
            {"get", static_get},
            {"iterate", static_iterate},
            {"remove", static_remove}};
  }

private:
  /**
   *
   */
  DECLARE_LUA_FUNCTION(HeaderMapWrapper, add);

  /**
   *
   */
  DECLARE_LUA_FUNCTION(HeaderMapWrapper, get);

  /**
   *
   */
  DECLARE_LUA_FUNCTION(HeaderMapWrapper, iterate);

  /**
   *
   */
  DECLARE_LUA_FUNCTION(HeaderMapWrapper, remove);

  HeaderMap& headers_;
};

} // namespace Lua
} // namespace Filter
} // namespace Http
} // namespace Envoy
