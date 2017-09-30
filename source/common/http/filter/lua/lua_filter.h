#pragma once

#include "envoy/http/filter.h"

#include "common/http/filter/lua/wrappers.h"
#include "common/lua/wrappers.h"

namespace Envoy {
namespace Http {
namespace Filter {
namespace Lua {

/**
 * fixfix
 */
class ScriptLogger {
public:
  virtual ~ScriptLogger() {}

  /**
   * fixfix
   */
  virtual void scriptLog(int level, const char* message) PURE;
};

/**
 * fixfix
 */
class FilterCallbacks {
public:
  virtual ~FilterCallbacks() {}

  virtual ScriptLogger& logger() PURE;

  virtual void addData(Buffer::Instance& data) PURE;

  virtual const Buffer::Instance* bufferedBody() PURE;
};

/**
 * fixfix
 */
class StreamHandleWrapper : public Envoy::Lua::BaseLuaObject<StreamHandleWrapper> {
public:
  enum class State { Running, WaitForBodyChunk, WaitForBody, WaitForTrailers };

  StreamHandleWrapper(Envoy::Lua::CoroutinePtr&& coroutine, HeaderMap& headers, bool end_stream,
                      FilterCallbacks& callbacks);

  void start();
  FilterDataStatus onData(Buffer::Instance& data, bool end_stream);
  void onTrailers(HeaderMap& trailers);

  static ExportedFunctions exportedFunctions() {
    return {{"headers", static_luaHeaders},
            {"body", static_luaBody},
            {"bodyChunks", static_luaBodyChunks},
            {"trailers", static_luaTrailers},
            {"log", static_luaLog}};
  }

private:
  /**
   *
   */
  DECLARE_LUA_FUNCTION(StreamHandleWrapper, luaHeaders);

  /**
   *
   */
  DECLARE_LUA_FUNCTION(StreamHandleWrapper, luaBody);

  /**
   *
   */
  DECLARE_LUA_FUNCTION(StreamHandleWrapper, luaBodyChunks);

  /**
   *
   */
  DECLARE_LUA_FUNCTION(StreamHandleWrapper, luaTrailers);

  /**
   *
   */
  DECLARE_LUA_FUNCTION(StreamHandleWrapper, luaLog);

  /**
   *
   */
  DECLARE_LUA_CLOSURE(StreamHandleWrapper, luaBodyIterator);

  // Envoy::Lua::BaseLuaObject
  void onMarkDead() override {
    // fixfix wrapper?
    if (headers_wrapper_.get()) {
      headers_wrapper_.get()->markDead();
    }

    if (trailers_wrapper_.get()) {
      trailers_wrapper_.get()->markDead();
    }
  }

  Envoy::Lua::CoroutinePtr coroutine_;
  HeaderMap& headers_;
  bool end_stream_;
  FilterCallbacks& callbacks_;
  HeaderMap* trailers_{};
  Envoy::Lua::LuaRef<HeaderMapWrapper> headers_wrapper_;
  Envoy::Lua::LuaRef<Envoy::Lua::BufferWrapper> body_wrapper_; // fixfix death
  Envoy::Lua::LuaRef<HeaderMapWrapper> trailers_wrapper_;
  State state_{State::Running};
};

/**
 * fixfix
 */
class FilterConfig {
public:
  FilterConfig(const std::string& lua_code);
  Envoy::Lua::CoroutinePtr createCoroutine() { return lua_state_.createCoroutine(); }
  void gc() { return lua_state_.gc(); }

private:
  Envoy::Lua::ThreadLocalState lua_state_;
};

typedef std::shared_ptr<FilterConfig> FilterConfigConstSharedPtr;

/**
 * fixfix
 */
class Filter : public StreamFilter, public ScriptLogger {
public:
  Filter(FilterConfigConstSharedPtr config) : config_(config) {}

  // Http::StreamFilterBase
  void onDestroy() override {}

  // Http::StreamDecoderFilter
  FilterHeadersStatus decodeHeaders(HeaderMap& headers, bool end_stream) override;
  FilterDataStatus decodeData(Buffer::Instance& data, bool end_stream) override;
  FilterTrailersStatus decodeTrailers(HeaderMap&) override;
  void setDecoderFilterCallbacks(StreamDecoderFilterCallbacks& callbacks) override {
    decoder_callbacks_.callbacks_ = &callbacks;
  }

  // Http::StreamEncoderFilter
  FilterHeadersStatus encodeHeaders(HeaderMap&, bool) override {
    return FilterHeadersStatus::Continue;
  }
  FilterDataStatus encodeData(Buffer::Instance&, bool) override {
    return FilterDataStatus::Continue;
  };
  FilterTrailersStatus encodeTrailers(HeaderMap&) override {
    return FilterTrailersStatus::Continue;
  };
  void setEncoderFilterCallbacks(StreamEncoderFilterCallbacks& callbacks) override {
    encoder_callbacks_ = &callbacks;
  };

private:
  struct DecoderCallbacks : public FilterCallbacks {
    DecoderCallbacks(Filter& parent) : parent_(parent) {}

    // FilterCallbacks
    ScriptLogger& logger() override { return parent_; }
    void addData(Buffer::Instance& data) override {
      return callbacks_->addDecodedData(data, false);
    }
    const Buffer::Instance* bufferedBody() override { return callbacks_->decodingBuffer(); }

    Filter& parent_;
    StreamDecoderFilterCallbacks* callbacks_{};
  };

  FilterConfigConstSharedPtr config_;
  DecoderCallbacks decoder_callbacks_{*this};
  StreamEncoderFilterCallbacks* encoder_callbacks_{};
  Envoy::Lua::LuaDeathRef<StreamHandleWrapper> request_stream_wrapper_;
};

} // namespace Lua
} // namespace Filter
} // namespace Http
} // namespace Envoy
