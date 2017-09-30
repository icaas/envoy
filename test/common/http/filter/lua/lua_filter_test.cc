#include "common/buffer/buffer_impl.h"
#include "common/http/filter/lua/lua_filter.h"

#include "test/mocks/http/mocks.h"
#include "test/test_common/utility.h"

#include "gmock/gmock.h"

using testing::InSequence;
using testing::Invoke;
using testing::StrEq;
using testing::_;

namespace Envoy {
namespace Http {
namespace Filter {
namespace Lua {

class TestFilter : public Filter {
public:
  using Filter::Filter;

  MOCK_METHOD2(scriptLog, void(int level, const char* message));
};

// fixfix multi-data tests.

class LuaHttpFilterTest : public testing::Test {
public:
  LuaHttpFilterTest() {
    ON_CALL(decoder_callbacks_, addDecodedData(_, _))
        .WillByDefault(Invoke([this](Buffer::Instance& data, bool) {
          if (decoder_callbacks_.buffer_ == nullptr) {
            decoder_callbacks_.buffer_.reset(new Buffer::OwnedImpl());
          }
          decoder_callbacks_.buffer_->move(data);
        }));
  }

  void setup(const std::string& lua_code) {
    config_.reset(new FilterConfig(lua_code));
    filter_.reset(new TestFilter(config_));
    filter_->setDecoderFilterCallbacks(decoder_callbacks_);
    filter_->setEncoderFilterCallbacks(encoder_callbacks_);
  }

  std::shared_ptr<FilterConfig> config_;
  std::unique_ptr<TestFilter> filter_;
  NiceMock<MockStreamDecoderFilterCallbacks> decoder_callbacks_;
  MockStreamEncoderFilterCallbacks encoder_callbacks_;

  const std::string HEADER_ONLY_SCRIPT{R"EOF(
    function envoy_on_request(request_handle)
      request_handle:log(0, request_handle:headers():get(":path"))
    end
  )EOF"};

  const std::string BODY_CHUNK_SCRIPT{R"EOF(
    function envoy_on_request(request_handle)
      request_handle:log(0, request_handle:headers():get(":path"))

      for chunk in request_handle:bodyChunks() do
        request_handle:log(0, chunk:byteSize())
      end

      request_handle:log(0, "done")
    end
  )EOF"};

  const std::string TRAILERS_SCRIPT{R"EOF(
    function envoy_on_request(request_handle)
      request_handle:log(0, request_handle:headers():get(":path"))

      for chunk in request_handle:bodyChunks() do
        request_handle:log(0, chunk:byteSize())
      end

      local trailers = request_handle:trailers()
      if trailers ~= nil then
        request_handle:log(0, trailers:get("foo"))
      else
        request_handle:log(0, "no trailers")
      end
    end
  )EOF"};

  const std::string TRAILERS_NO_BODY_SCRIPT{R"EOF(
    function envoy_on_request(request_handle)
      request_handle:log(0, request_handle:headers():get(":path"))

      if request_handle:trailers() ~= nil then
        request_handle:log(0, request_handle:trailers():get("foo"))
      else
        request_handle:log(0, "no trailers")
      end
    end
  )EOF"};

  const std::string BODY_SCRIPT{R"EOF(
    function envoy_on_request(request_handle)
      request_handle:log(0, request_handle:headers():get(":path"))

      if request_handle:body() ~= nil then
        request_handle:log(0, request_handle:body():byteSize())
      else
        request_handle:log(0, "no body")
      end
    end
  )EOF"};

  const std::string BODY_TRAILERS_SCRIPT{R"EOF(
    function envoy_on_request(request_handle)
      request_handle:log(0, request_handle:headers():get(":path"))

      if request_handle:body() ~= nil then
        request_handle:log(0, request_handle:body():byteSize())
      else
        request_handle:log(0, "no body")
      end

      if request_handle:trailers() ~= nil then
        request_handle:log(0, request_handle:trailers():get("foo"))
      else
        request_handle:log(0, "no trailers")
      end
    end
  )EOF"};

  // fixfix script with headers and trailers, no body.
};

/*const std::string code(R"EOF(
    function envoy_on_request(request_handle)
      if headers ~= nil then
        headers:iterate(
          function(key, value)
            request_handle:log(0, string.format("'%s' '%s'", key, value))
          end
        )
      end

      local headers = request_handle:headers()
      headers:iterate(
        function(key, value)
          request_handle:log(0, string.format("'%s' '%s'", key, value))
        end
      )

      for chunk in request_handle:bodyChunks() do
        request_handle:log(0, chunk:byteSize())
      end

      local trailers = request_handle:trailers()
      if trailers ~= nil then
        trailers:iterate(
          function(key, value)
            request_handle:log(0, string.format("'%s' '%s'", key, value))
          end
        )
      end

      request_handle:log(0, "all done!")
    end
  )EOF");*/

// Buffer::OwnedImpl data("hello");
// filter_->decodeData(data, true);

/*filter_.reset();
config_->gc();

Filter filter2(config_);
filter2.decodeHeaders(request_headers, false);*/

TEST_F(LuaHttpFilterTest, ScriptHeadersOnlyRequestHeadersOnly) {
  InSequence s;
  setup(HEADER_ONLY_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
}

TEST_F(LuaHttpFilterTest, ScriptHeadersOnlyRequestBody) {
  InSequence s;
  setup(HEADER_ONLY_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data, true));
}

TEST_F(LuaHttpFilterTest, ScriptHeadersOnlyRequestBodyTrailers) {
  InSequence s;
  setup(HEADER_ONLY_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data, false));

  TestHeaderMapImpl request_trailers{{"foo", "bar"}};
  EXPECT_EQ(FilterTrailersStatus::Continue, filter_->decodeTrailers(request_trailers));
}

TEST_F(LuaHttpFilterTest, ScriptBodyChunksRequestHeadersOnly) {
  InSequence s;
  setup(BODY_CHUNK_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("done")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
}

TEST_F(LuaHttpFilterTest, ScriptBodyChunksRequestBody) {
  InSequence s;
  setup(BODY_CHUNK_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("5")));
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("done")));
  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data, true));
}

TEST_F(LuaHttpFilterTest, ScriptBodyChunksRequestBodyTrailers) {
  InSequence s;
  setup(BODY_CHUNK_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("5")));
  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data, false));

  TestHeaderMapImpl request_trailers{{"foo", "bar"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("done")));
  EXPECT_EQ(FilterTrailersStatus::Continue, filter_->decodeTrailers(request_trailers));
}

TEST_F(LuaHttpFilterTest, ScriptTrailersRequestHeadersOnly) {
  InSequence s;
  setup(TRAILERS_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("no trailers")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
}

TEST_F(LuaHttpFilterTest, ScriptTrailersRequestBody) {
  InSequence s;
  setup(TRAILERS_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("5")));
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("no trailers")));
  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data, true));
}

TEST_F(LuaHttpFilterTest, ScriptTrailersRequestBodyTrailers) {
  InSequence s;
  setup(TRAILERS_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("5")));
  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data, false));

  TestHeaderMapImpl request_trailers{{"foo", "bar"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("bar")));
  EXPECT_EQ(FilterTrailersStatus::Continue, filter_->decodeTrailers(request_trailers));
}

TEST_F(LuaHttpFilterTest, ScriptTrailersNoBodyRequestHeadersOnly) {
  InSequence s;
  setup(TRAILERS_NO_BODY_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("no trailers")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
}

TEST_F(LuaHttpFilterTest, ScriptTrailersNoBodyRequestBody) {
  InSequence s;
  setup(TRAILERS_NO_BODY_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("no trailers")));
  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data, true));
}

TEST_F(LuaHttpFilterTest, ScriptTrailersNoBodyRequestBodyTrailers) {
  InSequence s;
  setup(TRAILERS_NO_BODY_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data, false));

  TestHeaderMapImpl request_trailers{{"foo", "bar"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("bar")));
  EXPECT_EQ(FilterTrailersStatus::Continue, filter_->decodeTrailers(request_trailers));
}

TEST_F(LuaHttpFilterTest, ScriptBodyRequestHeadersOnly) {
  InSequence s;
  setup(BODY_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("no body")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
}

TEST_F(LuaHttpFilterTest, ScriptBodyRequestBody) {
  InSequence s;
  setup(BODY_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("5")));
  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data, true));
}

TEST_F(LuaHttpFilterTest, ScriptBodyRequestBodyTwoFrames) {
  InSequence s;
  setup(BODY_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(FilterDataStatus::StopIterationAndBuffer, filter_->decodeData(data, false));
  decoder_callbacks_.addDecodedData(data, false);

  Buffer::OwnedImpl data2("world");
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("10")));
  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data2, true));
}

TEST_F(LuaHttpFilterTest, ScriptBodyRequestBodyTwoFramesTrailers) {
  InSequence s;
  setup(BODY_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(FilterDataStatus::StopIterationAndBuffer, filter_->decodeData(data, false));
  decoder_callbacks_.addDecodedData(data, false);

  Buffer::OwnedImpl data2("world");
  EXPECT_EQ(FilterDataStatus::StopIterationAndBuffer, filter_->decodeData(data2, false));
  decoder_callbacks_.addDecodedData(data2, false);

  TestHeaderMapImpl request_trailers{{"foo", "bar"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("10")));
  EXPECT_EQ(FilterTrailersStatus::Continue, filter_->decodeTrailers(request_trailers));
}

TEST_F(LuaHttpFilterTest, ScriptBodyTrailersRequestHeadersOnly) {
  InSequence s;
  setup(BODY_TRAILERS_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("no body")));
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("no trailers")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, true));
}

TEST_F(LuaHttpFilterTest, ScriptBodyTrailersRequestBody) {
  InSequence s;
  setup(BODY_TRAILERS_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("5")));
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("no trailers")));
  EXPECT_EQ(FilterDataStatus::Continue, filter_->decodeData(data, true));
}

TEST_F(LuaHttpFilterTest, ScriptBodyTrailersRequestBodyTrailers) {
  InSequence s;
  setup(BODY_TRAILERS_SCRIPT);

  TestHeaderMapImpl request_headers{{":path", "/"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("/")));
  EXPECT_EQ(FilterHeadersStatus::Continue, filter_->decodeHeaders(request_headers, false));

  Buffer::OwnedImpl data("hello");
  EXPECT_EQ(FilterDataStatus::StopIterationAndBuffer, filter_->decodeData(data, false));
  decoder_callbacks_.addDecodedData(data, false);

  TestHeaderMapImpl request_trailers{{"foo", "bar"}};
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("5")));
  EXPECT_CALL(*filter_, scriptLog(0, StrEq("bar")));
  EXPECT_EQ(FilterTrailersStatus::Continue, filter_->decodeTrailers(request_trailers));
}

} // namespace Lua
} // namespace Filter
} // namespace Http
} // namespace Envoy
