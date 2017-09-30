#pragma once

#include <memory>
#include <string>
#include <vector>

#include "common/common/assert.h"
#include "common/common/logger.h"

#include "luajit-2.0/lua.hpp"

namespace Envoy {
namespace Lua {

/**
 * fixfix
 */
#define DECLARE_LUA_FUNCTION_EX(Class, Name, Index)                                                \
  static int static_##Name(lua_State* state) {                                                     \
    Class* object = static_cast<Class*>(luaL_checkudata(state, Index, typeid(Class).name()));      \
    object->checkDead(state);                                                                      \
    return object->Name(state);                                                                    \
  }                                                                                                \
  int Name(lua_State* state);

/**
 * fixfix
 */
#define DECLARE_LUA_FUNCTION(Class, Name) DECLARE_LUA_FUNCTION_EX(Class, Name, 1)

/**
 * fixfix
 */
#define DECLARE_LUA_CLOSURE(Class, Name) DECLARE_LUA_FUNCTION_EX(Class, Name, lua_upvalueindex(1))

/**
 * fixfix
 */
template <class T> class BaseLuaObject : protected Logger::Loggable<Logger::Id::lua> {
public:
  typedef std::vector<std::pair<const char*, lua_CFunction>> ExportedFunctions;

  virtual ~BaseLuaObject() {}

  /**
   * fixfix
   */
  template <typename... ConstructorArgs>
  static std::pair<T*, lua_State*> create(lua_State* state, ConstructorArgs&&... args) {
    // fixfix error checking
    void* mem = lua_newuserdata(state, sizeof(T));
    luaL_getmetatable(state, typeid(T).name());
    ASSERT(lua_istable(state, -1));
    lua_setmetatable(state, -2);

    // fixfix
    ENVOY_LOG(trace, "creating {} at {}", typeid(T).name(), mem);
    return {new (mem) T(std::forward<ConstructorArgs>(args)...), state};
  }

  /**
   * fixfix
   */
  static void registerType(lua_State* state) {
    std::vector<luaL_Reg> to_register;

    // fixfix
    ExportedFunctions functions = T::exportedFunctions();
    for (auto function : functions) {
      to_register.push_back({function.first, function.second});
    }

    // fixfix
    to_register.push_back(
        {"__gc", [](lua_State* state) {
           T* object = static_cast<T*>(luaL_checkudata(state, 1, typeid(T).name()));
           ENVOY_LOG(trace, "destroying {} at {}", typeid(T).name(), static_cast<void*>(object));
           object->~T();
           return 0;
         }});

    to_register.push_back({nullptr, nullptr});

    // fixfix
    ENVOY_LOG(info, "registering new type: {}", typeid(T).name());
    int rc = luaL_newmetatable(state, typeid(T).name());
    ASSERT(rc == 1);

    lua_pushvalue(state, -1);
    lua_setfield(state, -2, "__index");
    luaL_register(state, nullptr, to_register.data());
  }

  /**
   * fixfix
   */
  int checkDead(lua_State* state) {
    if (dead_) {
      return luaL_error(state, "object used outside of proper scope");
    }
    return 0;
  }

  /**
   * fixfix
   */
  void markDead() {
    ASSERT(!dead_);
    dead_ = true;
    ENVOY_LOG(trace, "marking dead {} at {}", typeid(T).name(), static_cast<void*>(this));
    onMarkDead();
  }

protected:
  virtual void onMarkDead() {}

private:
  bool dead_{};
};

class ThreadLocalState;

/**
 * fixfix
 */
template <typename T> class LuaRef {
public:
  LuaRef() {}

  LuaRef(const std::pair<T*, lua_State*>& object, bool leave_on_stack) {
    reset(object, leave_on_stack);
  }

  ~LuaRef() { unref(); }

  T* get() { return object_.first; }

  void reset(const std::pair<T*, lua_State*>& object, bool leave_on_stack) {
    unref();

    if (leave_on_stack) {
      lua_pushvalue(object.second, -1);
    }

    object_ = object;
    ref_ = luaL_ref(object_.second, LUA_REGISTRYINDEX);
    ASSERT(ref_ != LUA_REFNIL);
  }

  void pushStack() {
    ASSERT(object_.first);
    lua_rawgeti(object_.second, LUA_REGISTRYINDEX, ref_);
  }

protected:
  void unref() {
    if (object_.second != nullptr) {
      luaL_unref(object_.second, LUA_REGISTRYINDEX, ref_);
    }
  }

  std::pair<T*, lua_State*> object_{};
  int ref_{LUA_NOREF};
};

/**
 * fixfix
 */
template <typename T> class LuaDeathRef : public LuaRef<T> {
public:
  using LuaRef<T>::LuaRef;

  ~LuaDeathRef() {
    if (this->object_.first) {
      this->object_.first->markDead();
    }
  }
};

/**
 * fixfix
 */
class Coroutine : Logger::Loggable<Logger::Id::lua> {
public:
  enum class State { NotStarted, Yielded, Finished };

  Coroutine(ThreadLocalState& parent);

  void start(const std::string& start_function, int num_args);
  lua_State* luaState() { return coroutine_state_.get(); }
  State state() { return state_; }
  void resume(int num_args);

private:
  LuaRef<lua_State> coroutine_state_;
  State state_{State::NotStarted};
};

typedef std::unique_ptr<Coroutine> CoroutinePtr;

/**
 * fixfix
 */
class ThreadLocalState {
public:
  ThreadLocalState(const std::string& code);
  ~ThreadLocalState();

  CoroutinePtr createCoroutine();
  void gc() { lua_gc(state_, LUA_GCCOLLECT, 0); }
  lua_State* state() { return state_; }

private:
  lua_State* state_;
};

} // namespace Lua
} // namespace Envoy
