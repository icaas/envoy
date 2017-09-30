#include "common/http/filter/lua/lua_filter.h"

#include "common/buffer/buffer_impl.h"
#include "common/common/assert.h"

namespace Envoy {
namespace Http {
namespace Filter {
namespace Lua {

StreamHandleWrapper::StreamHandleWrapper(Envoy::Lua::CoroutinePtr&& coroutine, HeaderMap& headers,
                                         bool end_stream, FilterCallbacks& callbacks)
    : coroutine_(std::move(coroutine)), headers_(headers), end_stream_(end_stream),
      callbacks_(callbacks) {}

void StreamHandleWrapper::start() {
  // We are on the top of the stack. fixfix don't look up string.
  coroutine_->start("envoy_on_request", 1);
}

FilterDataStatus StreamHandleWrapper::onData(Buffer::Instance& data, bool end_stream) {
  ASSERT(!end_stream_);
  end_stream_ = end_stream;
  if (coroutine_->state() == Envoy::Lua::Coroutine::State::Finished) {
    return FilterDataStatus::Continue;
  }

  if (state_ == State::WaitForBodyChunk) {
    ENVOY_LOG(debug, "resuming for next body chunk");
    Envoy::Lua::LuaDeathRef<Envoy::Lua::BufferWrapper> wrapper(
        Envoy::Lua::BufferWrapper::create(coroutine_->luaState(), data), true);
    state_ = State::Running;
    coroutine_->resume(1);
  } else if (state_ == State::WaitForBody && end_stream_) {
    // fixfix conn manager
    callbacks_.addData(data);

    ENVOY_LOG(debug, "resuming body due to end stream");
    state_ = State::Running;
    coroutine_->resume(luaBody(coroutine_->luaState()));
  } else if (state_ == State::WaitForBody && !end_stream_) {
    ENVOY_LOG(debug, "buffering body");
    return FilterDataStatus::StopIterationAndBuffer;
  } else if (state_ == State::WaitForTrailers && end_stream_) {
    ENVOY_LOG(debug, "resuming nil trailers due to end stream");
    state_ = State::Running;
    coroutine_->resume(0);
  }

  return FilterDataStatus::Continue;
}

void StreamHandleWrapper::onTrailers(HeaderMap& trailers) {
  ASSERT(!end_stream_);
  end_stream_ = true;
  trailers_ = &trailers;

  if (state_ == State::WaitForBodyChunk) {
    ENVOY_LOG(debug, "resuming nil body chunk due to trailers");
    state_ = State::Running;
    coroutine_->resume(0);
  } else if (state_ == State::WaitForBody) {
    ENVOY_LOG(debug, "resuming body due to trailers");
    state_ = State::Running;
    coroutine_->resume(luaBody(coroutine_->luaState()));
  }

  if (coroutine_->state() == Envoy::Lua::Coroutine::State::Finished) {
    return;
  }

  if (state_ == State::WaitForTrailers) {
    // Mimic a call to trailers which will push the trailers onto the stack and then resume.
    state_ = State::Running;
    coroutine_->resume(luaTrailers(coroutine_->luaState()));
  }
}

int StreamHandleWrapper::luaHeaders(lua_State* state) {
  // fixfix don't allow modification after headers are continued.
  if (headers_wrapper_.get() != nullptr) {
    headers_wrapper_.pushStack();
  } else {
    headers_wrapper_.reset(HeaderMapWrapper::create(state, headers_), true);
  }
  return 1;
}

int StreamHandleWrapper::luaBody(lua_State* state) {
  // fixfix fail if body was not buffered.
  if (!(state_ == State::Running)) {
    ASSERT(false); // fixfix
  }

  if (end_stream_) {
    if (callbacks_.bufferedBody() == nullptr) {
      ENVOY_LOG(debug, "end stream. no body");
      return 0;
    } else {
      if (body_wrapper_.get() != nullptr) {
        body_wrapper_.pushStack();
      } else {
        body_wrapper_.reset(Envoy::Lua::BufferWrapper::create(state, *callbacks_.bufferedBody()),
                            true);
      }
      return 1;
    }
  } else {
    ENVOY_LOG(debug, "yielding for full body");
    state_ = State::WaitForBody;
    return lua_yield(state, 0);
  }
}

int StreamHandleWrapper::luaBodyChunks(lua_State* state) {
  if (state_ != State::Running) {
    ASSERT(false); // fixfix
  }

  // fixfix
  lua_pushcclosure(state, static_luaBodyIterator, 1);
  return 1;
}

int StreamHandleWrapper::luaBodyIterator(lua_State* state) {
  if (!(state_ == State::Running)) {
    ASSERT(false); // fixfix
  }

  if (end_stream_) {
    ENVOY_LOG(debug, "body complete. no more body chunks");
    return 0;
  } else {
    ENVOY_LOG(debug, "yielding for next body chunk");
    state_ = State::WaitForBodyChunk;
    return lua_yield(state, 0);
  }
}

int StreamHandleWrapper::luaTrailers(lua_State* state) {
  if (!(state_ == State::Running)) {
    ASSERT(false); // fixfix
  }

  if (end_stream_ && trailers_ == nullptr) {
    ENVOY_LOG(debug, "end stream. no trailers");
    return 0;
  } else if (trailers_ != nullptr) {
    if (trailers_wrapper_.get() != nullptr) {
      trailers_wrapper_.pushStack();
    } else {
      trailers_wrapper_.reset(HeaderMapWrapper::create(state, *trailers_), true);
    }
    return 1;
  } else {
    ENVOY_LOG(debug, "yielding for trailers");
    state_ = State::WaitForTrailers;
    return lua_yield(state, 0);
  }
}

int StreamHandleWrapper::luaLog(lua_State* state) {
  int level = luaL_checkint(state, 2);
  const char* message = luaL_checkstring(state, 3);
  callbacks_.logger().scriptLog(level, message);
  return 0;
}

/*void StreamHandleWrapper::scriptLog(int level, const char* message) {
  // fixfix levels
  switch (level) {
  default: {
    ENVOY_LOG(debug, "script log: {}", message);
    break;
  }
  }
}*/

FilterConfig::FilterConfig(const std::string& lua_code) : lua_state_(lua_code) {
  Envoy::Lua::BufferWrapper::registerType(lua_state_.state());
  HeaderMapWrapper::registerType(lua_state_.state());
  StreamHandleWrapper::registerType(lua_state_.state());
}

FilterHeadersStatus Filter::decodeHeaders(HeaderMap& headers, bool end_stream) {
  Envoy::Lua::CoroutinePtr coroutine = config_->createCoroutine();
  request_stream_wrapper_.reset(StreamHandleWrapper::create(coroutine->luaState(),
                                                            std::move(coroutine), headers,
                                                            end_stream, decoder_callbacks_),
                                true);
  request_stream_wrapper_.get()->start();

  // fixfix mark unusable when script is yielded
  // fixfix mark dead when script is finished
  // fixfix error handling
  // fixfix unexpected yield

  return FilterHeadersStatus::Continue;
}

FilterDataStatus Filter::decodeData(Buffer::Instance& data, bool end_stream) {
  return request_stream_wrapper_.get()->onData(data, end_stream);
}

FilterTrailersStatus Filter::decodeTrailers(HeaderMap& trailers) {
  request_stream_wrapper_.get()->onTrailers(trailers);
  return FilterTrailersStatus::Continue;
}

} // namespace Lua
} // namespace Filter
} // namespace Http
} // namespace Envoy
