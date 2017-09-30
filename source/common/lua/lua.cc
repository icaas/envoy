#include "common/lua/lua.h"

#include "envoy/common/exception.h"

#include "common/common/assert.h"

namespace Envoy {
namespace Lua {

Coroutine::Coroutine(ThreadLocalState& parent)
    : coroutine_state_({lua_newthread(parent.state()), parent.state()}, false) {}

void Coroutine::start(const std::string& start_function, int num_args) {
  ASSERT(state_ == State::NotStarted);

  // fixfix: perf? error checking?
  state_ = State::Yielded;
  lua_getglobal(coroutine_state_.get(), start_function.c_str());
  lua_insert(coroutine_state_.get(), -(num_args + 1));
  resume(num_args);
}

void Coroutine::resume(int num_args) {
  ASSERT(state_ == State::Yielded);
  int rc = lua_resume(coroutine_state_.get(), num_args);

  if (0 == rc) {
    state_ = State::Finished;
    ENVOY_LOG(debug, "coroutine finished");
  } else if (LUA_YIELD == rc) {
    state_ = State::Yielded;
    ENVOY_LOG(debug, "coroutine yielded");
  } else {
    ENVOY_LOG(debug, "coroutine error: {}", lua_tostring(coroutine_state_.get(), -1));
    ASSERT(false); // fixfix
  }
}

ThreadLocalState::ThreadLocalState(const std::string& code) {
  state_ = lua_open();
  luaL_openlibs(state_);

  if (0 != luaL_loadstring(state_, code.c_str()) || 0 != lua_pcall(state_, 0, 0, 0)) {
    // fixfix free state
    throw EnvoyException(fmt::format("script load error: {}", lua_tostring(state_, -1)));
  }
}

ThreadLocalState::~ThreadLocalState() { lua_close(state_); }

CoroutinePtr ThreadLocalState::createCoroutine() { return CoroutinePtr{new Coroutine(*this)}; }

} // namespace Lua
} // namespace Envoy
