#include "edge_cloud_transport.hpp"

#include "mature_product_profile_registry.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(INFERLAB_HAS_EDGE_CLOUD_CURL) && INFERLAB_HAS_EDGE_CLOUD_CURL
#include <curl/curl.h>
#endif

#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace inferlab {
namespace {

bool starts_with(const std::string& value, const std::string& prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0U, prefix.size(), prefix) == 0;
}

bool ascii_starts_with_case_insensitive(const char* value,
                                        const std::string& prefix) {
  if (value == nullptr) return false;
  for (std::size_t index = 0U; index < prefix.size(); ++index) {
    if (value[index] == '\0') return false;
    const auto left = static_cast<unsigned char>(value[index]);
    const auto right = static_cast<unsigned char>(prefix[index]);
    if (std::tolower(left) != std::tolower(right)) return false;
  }
  return true;
}

bool loopback_http_endpoint(const std::string& endpoint) {
  for (const std::string host : {"http://localhost", "http://127.0.0.1",
                                 "http://[::1]"}) {
    if (!starts_with(endpoint, host)) continue;
    if (endpoint.size() == host.size()) return true;
    const char delimiter = endpoint[host.size()];
    return delimiter == ':' || delimiter == '/' || delimiter == '?' ||
           delimiter == '#';
  }
  return false;
}

bool valid_environment_variable_name(const std::string& value) {
  if (value.empty() || value.size() > kMaximumEdgeCloudIdentifierBytes) {
    return false;
  }
  const auto first = static_cast<unsigned char>(value.front());
  if (!std::isalpha(first) && value.front() != '_') return false;
  return std::all_of(value.begin() + 1, value.end(), [](const char character) {
    const auto byte = static_cast<unsigned char>(character);
    return std::isalnum(byte) || character == '_';
  });
}

bool endpoint_contains_unsafe_byte(const std::string& endpoint) {
  return std::any_of(endpoint.begin(), endpoint.end(), [](const char value) {
    const auto byte = static_cast<unsigned char>(value);
    return byte <= 0x20U || byte == 0x7fU;
  });
}

bool endpoint_has_authority(const std::string& endpoint) {
  const std::size_t scheme_end = endpoint.find("://");
  if (scheme_end == std::string::npos) return false;
  const std::size_t authority_begin = scheme_end + 3U;
  const std::size_t authority_end =
      endpoint.find_first_of("/?", authority_begin);
  const std::size_t length =
      (authority_end == std::string::npos ? endpoint.size() : authority_end) -
      authority_begin;
  return length != 0U;
}

std::string compiled_curl_version() {
#if defined(INFERLAB_HAS_EDGE_CLOUD_CURL) && INFERLAB_HAS_EDGE_CLOUD_CURL
  const curl_version_info_data* info = curl_version_info(CURLVERSION_NOW);
  return info != nullptr && info->version != nullptr ? info->version
                                                     : "linked_unknown";
#else
  return "unlinked";
#endif
}

class UnavailableCloudTransport final : public ICloudInferenceTransport {
 public:
  explicit UnavailableCloudTransport(OpenAiCompatibleTransportConfig config)
      : config_(std::move(config)) {}

  CloudTransportCapabilities capabilities() const override {
    return {"libcurl_openai_compatible",
            compiled_curl_version(),
            config_.product_id,
            "openai_chat_completions_sse",
            false,
#if defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON
            true,
#else
            false,
#endif
            true,
            true,
            true,
            true};
  }

  CloudGenerationResult generate(
      const EdgeCloudRequest&,
      const EdgeCloudChunkCallback&) override {
    CloudGenerationResult result;
    result.status = EdgeRuntimeStatus::kDependencyUnavailable;
    result.error_category = "cloud_transport_dependency_unavailable";
    result.detail =
        "libcurl 8.5+ and nlohmann/json are required for cloud execute";
    result.metrics.provenance = "capability_only_not_executed";
    return result;
  }

  void cancel() noexcept override {}

 private:
  OpenAiCompatibleTransportConfig config_;
};

#if defined(INFERLAB_HAS_EDGE_CLOUD_CURL) && INFERLAB_HAS_EDGE_CLOUD_CURL && \
    defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON

using SteadyClock = std::chrono::steady_clock;

std::size_t checked_token_count(const nlohmann::json& usage,
                                const char* field) {
  if (!usage.contains(field)) return 0U;
  const auto& value = usage.at(field);
  if (!value.is_number_unsigned()) {
    throw std::runtime_error("cloud usage token count has invalid type");
  }
  const auto parsed = value.get<std::uint64_t>();
  if (parsed > std::numeric_limits<std::size_t>::max()) {
    throw std::overflow_error("cloud usage token count exceeds size_t");
  }
  return static_cast<std::size_t>(parsed);
}

void assign_consistent_token_count(
    const nlohmann::json& usage, const char* field,
    std::optional<std::size_t>* destination) {
  if (!usage.contains(field)) return;
  const std::size_t value = checked_token_count(usage, field);
  if (destination->has_value() && **destination != value) {
    throw std::runtime_error(
        "cloud usage repeated with conflicting token counts");
  }
  *destination = value;
}

class CancellationReset {
 public:
  explicit CancellationReset(std::atomic<bool>* cancellation_requested)
      : cancellation_requested_(cancellation_requested) {}
  ~CancellationReset() {
    cancellation_requested_->store(false, std::memory_order_release);
  }

 private:
  std::atomic<bool>* cancellation_requested_;
};

class OpenAiSseParser {
 public:
  OpenAiSseParser(const std::size_t maximum_event_bytes,
                  const std::size_t maximum_response_bytes,
                  const EdgeCloudChunkCallback& on_chunk,
                  std::atomic<bool>* cancellation_requested)
      : maximum_event_bytes_(maximum_event_bytes),
        maximum_response_bytes_(maximum_response_bytes),
        on_chunk_(on_chunk),
        cancellation_requested_(cancellation_requested) {}

  bool consume(const char* data, const std::size_t size) noexcept {
    try {
      if (data == nullptr && size != 0U) {
        throw std::runtime_error("cloud response callback supplied null data");
      }
      if (total_response_bytes_ > maximum_response_bytes_ ||
          size > maximum_response_bytes_ - total_response_bytes_) {
        throw std::runtime_error("cloud response exceeds configured bound");
      }
      total_response_bytes_ += size;
      std::size_t offset = 0U;
      while (offset < size) {
        const char* const begin = data + offset;
        const char* const end = data + size;
        const char* const newline = std::find(begin, end, '\n');
        const std::size_t segment_size =
            static_cast<std::size_t>(newline - begin);
        if (pending_line_.size() > maximum_event_bytes_ ||
            segment_size > maximum_event_bytes_ - pending_line_.size()) {
          throw std::runtime_error("cloud SSE line exceeds configured bound");
        }
        pending_line_.append(begin, segment_size);
        offset += segment_size;
        if (newline == end) break;

        std::string line = std::move(pending_line_);
        pending_line_.clear();
        if (!line.empty() && line.back() == '\r') line.pop_back();
        consume_line(line);
        ++offset;
      }
      if (pending_line_.size() > maximum_event_bytes_) {
        throw std::runtime_error("cloud SSE pending line exceeds bound");
      }
      return true;
    } catch (const std::exception& error) {
      parser_error_ = error.what();
      return false;
    }
  }

  bool finish() noexcept {
    if (!parser_error_.empty()) return false;
    try {
      if (!pending_line_.empty()) {
        std::string line = std::move(pending_line_);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        consume_line(line);
      }
      if (!event_data_.empty()) dispatch_event();
      if (!done_) {
        throw std::runtime_error("cloud SSE stream ended without DONE event");
      }
      return true;
    } catch (const std::exception& error) {
      parser_error_ = error.what();
      return false;
    }
  }

  std::size_t delivered_chunks() const noexcept { return delivered_chunks_; }
  bool done() const noexcept { return done_; }
  bool callback_stopped() const noexcept { return callback_stopped_; }
  const std::string& parser_error() const noexcept { return parser_error_; }
  const std::string& finish_reason() const noexcept { return finish_reason_; }
  const std::optional<std::size_t>& reported_prompt_tokens() const noexcept {
    return reported_prompt_tokens_;
  }
  const std::optional<std::size_t>& reported_completion_tokens() const noexcept {
    return reported_completion_tokens_;
  }
  const std::optional<SteadyClock::time_point>& first_chunk_time() const
      noexcept {
    return first_chunk_time_;
  }

 private:
  void consume_line(const std::string& line) {
    if (cancellation_requested_->load(std::memory_order_acquire)) {
      callback_stopped_ = true;
      throw std::runtime_error("cloud request cancelled");
    }
    if (line.empty()) {
      if (!event_data_.empty()) dispatch_event();
      return;
    }
    if (line.front() == ':') return;
    if (!starts_with(line, "data:")) return;
    std::string value = line.substr(5U);
    if (!value.empty() && value.front() == ' ') value.erase(0U, 1U);
    const std::size_t delimiter = event_data_.empty() ? 0U : 1U;
    if (event_data_.size() > maximum_event_bytes_ ||
        delimiter > maximum_event_bytes_ - event_data_.size() ||
        value.size() >
            maximum_event_bytes_ - event_data_.size() - delimiter) {
      throw std::runtime_error("cloud SSE event exceeds configured bound");
    }
    if (delimiter != 0U) event_data_.push_back('\n');
    event_data_ += value;
  }

  void dispatch_event() {
    std::string data = std::move(event_data_);
    event_data_.clear();
    if (data == "[DONE]") {
      done_ = true;
      return;
    }
    if (done_) {
      throw std::runtime_error("cloud SSE data appeared after DONE");
    }
    const auto event = nlohmann::json::parse(data);
    if (!event.is_object()) {
      throw std::runtime_error("cloud SSE event must be a JSON object");
    }
    if (event.contains("error")) {
      throw std::runtime_error("cloud SSE event reported an API error");
    }
    if (event.contains("usage") && !event.at("usage").is_null()) {
      const auto& usage = event.at("usage");
      if (!usage.is_object()) {
        throw std::runtime_error("cloud usage must be an object");
      }
      if (usage.contains("prompt_tokens")) {
        assign_consistent_token_count(usage, "prompt_tokens",
                                      &reported_prompt_tokens_);
      }
      if (usage.contains("completion_tokens")) {
        assign_consistent_token_count(usage, "completion_tokens",
                                      &reported_completion_tokens_);
      }
    }
    if (!event.contains("choices")) return;
    const auto& choices = event.at("choices");
    if (!choices.is_array()) {
      throw std::runtime_error("cloud choices must be an array");
    }
    if (choices.size() > 1U) {
      throw std::runtime_error(
          "cloud stream returned more than the requested single choice");
    }
    for (const auto& choice : choices) {
      if (!choice.is_object()) {
        throw std::runtime_error("cloud choice must be an object");
      }
      if (choice.contains("finish_reason") &&
          !choice.at("finish_reason").is_null()) {
        if (!choice.at("finish_reason").is_string()) {
          throw std::runtime_error("cloud finish_reason must be a string");
        }
        const std::string value = choice.at("finish_reason").get<std::string>();
        if (value.empty() ||
            value.size() > kMaximumEdgeCloudIdentifierBytes) {
          throw std::runtime_error("cloud finish_reason is invalid");
        }
        if (!finish_reason_.empty() && finish_reason_ != value) {
          throw std::runtime_error("cloud stream has conflicting finish reasons");
        }
        finish_reason_ = value;
      }
      if (!choice.contains("delta")) continue;
      const auto& delta = choice.at("delta");
      if (!delta.is_object() || !delta.contains("content") ||
          delta.at("content").is_null()) {
        continue;
      }
      if (!delta.at("content").is_string()) {
        throw std::runtime_error("cloud delta content must be a string");
      }
      const std::string content = delta.at("content").get<std::string>();
      if (content.empty()) continue;
      if (content.size() > kMaximumEdgeCloudStreamChunkBytes) {
        throw std::runtime_error("cloud stream chunk exceeds canonical bound");
      }
      if (cancellation_requested_->load(std::memory_order_acquire)) {
        callback_stopped_ = true;
        throw std::runtime_error("cloud stream callback stopped");
      }
      if (!first_chunk_time_.has_value()) first_chunk_time_ = SteadyClock::now();
      if (delivered_chunks_ == std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error("cloud stream chunk count overflow");
      }
      const std::size_t chunk_index = delivered_chunks_;
      ++delivered_chunks_;
      if (!on_chunk_(chunk_index, content)) {
        callback_stopped_ = true;
        throw std::runtime_error("cloud stream callback stopped");
      }
    }
  }

  std::size_t maximum_event_bytes_;
  std::size_t maximum_response_bytes_;
  EdgeCloudChunkCallback on_chunk_;
  std::atomic<bool>* cancellation_requested_;
  std::string pending_line_;
  std::string event_data_;
  std::string parser_error_;
  std::string finish_reason_;
  std::size_t total_response_bytes_ = 0U;
  std::size_t delivered_chunks_ = 0U;
  bool done_ = false;
  bool callback_stopped_ = false;
  std::optional<std::size_t> reported_prompt_tokens_;
  std::optional<std::size_t> reported_completion_tokens_;
  std::optional<SteadyClock::time_point> first_chunk_time_;
};

std::size_t curl_write_callback(char* data, const std::size_t size,
                                const std::size_t count, void* user_data) {
  auto* parser = static_cast<OpenAiSseParser*>(user_data);
  if (size != 0U && count > std::numeric_limits<std::size_t>::max() / size) {
    return 0U;
  }
  const std::size_t bytes = size * count;
  return parser->consume(data, bytes) ? bytes : 0U;
}

int curl_progress_callback(void* user_data, curl_off_t, curl_off_t,
                           curl_off_t, curl_off_t) {
  const auto* cancellation_requested =
      static_cast<const std::atomic<bool>*>(user_data);
  return cancellation_requested->load(std::memory_order_acquire) ? 1 : 0;
}

class CurlHandle {
 public:
  CurlHandle() : handle_(curl_easy_init()) {}
  ~CurlHandle() {
    if (handle_ != nullptr) curl_easy_cleanup(handle_);
  }
  CURL* get() const noexcept { return handle_; }

 private:
  CURL* handle_;
};

class CurlHeaders {
 public:
  ~CurlHeaders() {
    if (headers_ != nullptr) curl_slist_free_all(headers_);
  }
  void append(const std::string& value) {
    curl_slist* updated = curl_slist_append(headers_, value.c_str());
    if (updated == nullptr) {
      throw std::bad_alloc();
    }
    headers_ = updated;
  }
  curl_slist* get() const noexcept { return headers_; }

 private:
  curl_slist* headers_ = nullptr;
};

class CurlGlobalState {
 public:
  CurlGlobalState()
      : initialized_(curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK) {}
  ~CurlGlobalState() {
    if (initialized_) curl_global_cleanup();
  }
  bool initialized() const noexcept { return initialized_; }

 private:
  bool initialized_;
};

bool curl_globally_initialized() {
  static const CurlGlobalState state;
  return state.initialized();
}

double positive_milliseconds(const SteadyClock::duration duration) {
  return std::max(
      0.001,
      std::chrono::duration<double, std::milli>(duration).count());
}

class CurlOpenAiCompatibleTransport final
    : public ICloudInferenceTransport {
 public:
  explicit CurlOpenAiCompatibleTransport(
      OpenAiCompatibleTransportConfig config)
      : config_(std::move(config)) {}

  CloudTransportCapabilities capabilities() const override {
    return {"libcurl_openai_compatible",
            compiled_curl_version(),
            config_.product_id,
            "openai_chat_completions_sse",
            true,
            true,
            true,
            true,
            true,
            true};
  }

  CloudGenerationResult generate(
      const EdgeCloudRequest& request,
      const EdgeCloudChunkCallback& on_chunk) override {
    validate_edge_cloud_request(request);
    if (!on_chunk) {
      throw std::invalid_argument("cloud stream callback is required");
    }
    std::unique_lock<std::mutex> generation_lock(generation_mutex_,
                                                  std::try_to_lock);
    if (!generation_lock.owns_lock()) {
      CloudGenerationResult busy;
      busy.status = EdgeRuntimeStatus::kFailed;
      busy.error_category = "cloud_transport_generation_already_active";
      busy.detail = "cloud transport permits one active generation";
      busy.metrics.provenance = "transport_lifecycle_gate";
      return busy;
    }
    CloudGenerationResult result;
    result.metrics.provenance = "libcurl_openai_compatible_execute";
    if (cancellation_requested_.exchange(false,
                                         std::memory_order_acq_rel)) {
      result.status = EdgeRuntimeStatus::kCancelled;
      result.error_category = "cancelled_before_cloud_transport_start";
      result.detail = "cloud request was cancelled before transport start";
      return result;
    }
    CancellationReset reset_cancellation(&cancellation_requested_);
    if (!curl_globally_initialized()) {
      result.status = EdgeRuntimeStatus::kDependencyUnavailable;
      result.error_category = "curl_global_initialization_failed";
      result.detail = "libcurl global initialization failed";
      return result;
    }

    const char* api_key =
        std::getenv(config_.api_key_environment_variable.c_str());
    if (api_key == nullptr || *api_key == '\0') {
      result.status = EdgeRuntimeStatus::kInvalidConfiguration;
      result.error_category = "cloud_api_key_environment_missing";
      result.detail = "configured cloud API key environment variable is empty";
      return result;
    }
    const std::string api_key_value(api_key);
    if (api_key_value.size() > 16384U ||
        api_key_value.find('\r') != std::string::npos ||
        api_key_value.find('\n') != std::string::npos) {
      result.status = EdgeRuntimeStatus::kInvalidConfiguration;
      result.error_category = "cloud_api_key_environment_invalid";
      result.detail = "configured cloud API key value is oversized or unsafe";
      return result;
    }

    const nlohmann::json payload = {
        {"model", request.cloud_model_id},
        {"messages",
         nlohmann::json::array(
             {{{"role", "user"}, {"content", request.generation.prompt}}})},
        {"stream", true},
        {"stream_options", {{"include_usage", true}}},
        {"n", 1},
        {"max_tokens", request.generation.maximum_output_tokens},
        {"seed", request.generation.seed},
        {"temperature", request.generation.temperature},
        {"top_p", request.generation.top_p},
    };
    const std::string payload_text = payload.dump();
    if (payload_text.size() > kMaximumEdgePromptBytes + 65536U) {
      result.status = EdgeRuntimeStatus::kInvalidConfiguration;
      result.error_category = "cloud_request_payload_oversized";
      result.detail = "serialized cloud request exceeds bounded payload size";
      return result;
    }

    CurlHandle handle;
    if (handle.get() == nullptr) {
      result.status = EdgeRuntimeStatus::kDependencyUnavailable;
      result.error_category = "curl_easy_initialization_failed";
      result.detail = "libcurl easy handle initialization failed";
      return result;
    }
    CurlHeaders headers;
    headers.append("Content-Type: application/json");
    headers.append("Accept: text/event-stream");
    headers.append("Authorization: Bearer " + api_key_value);

    OpenAiSseParser parser(config_.maximum_event_bytes,
                           config_.maximum_response_bytes, on_chunk,
                           &cancellation_requested_);
    CURL* curl = handle.get();
    CURLcode setup_status = CURLE_OK;
    const auto apply_option = [&setup_status](const CURLcode status) {
      if (setup_status == CURLE_OK && status != CURLE_OK) {
        setup_status = status;
      }
    };
    apply_option(curl_easy_setopt(curl, CURLOPT_URL,
                                  config_.endpoint_url.c_str()));
    apply_option(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers.get()));
    apply_option(curl_easy_setopt(curl, CURLOPT_POST, 1L));
    apply_option(curl_easy_setopt(curl, CURLOPT_POSTFIELDS,
                                  payload_text.data()));
    apply_option(curl_easy_setopt(
        curl, CURLOPT_POSTFIELDSIZE_LARGE,
        static_cast<curl_off_t>(payload_text.size())));
    apply_option(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
                                  curl_write_callback));
    apply_option(curl_easy_setopt(curl, CURLOPT_WRITEDATA, &parser));
    apply_option(curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
                                  curl_progress_callback));
    apply_option(curl_easy_setopt(curl, CURLOPT_XFERINFODATA,
                                  &cancellation_requested_));
    apply_option(curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L));
    apply_option(curl_easy_setopt(
        curl, CURLOPT_CONNECTTIMEOUT_MS,
        static_cast<long>(config_.connect_timeout_ms)));
    apply_option(curl_easy_setopt(
        curl, CURLOPT_TIMEOUT_MS,
        static_cast<long>(request.generation.maximum_generation_duration_ms)));
    apply_option(curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L));
    apply_option(curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L));
    apply_option(curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 0L));
    apply_option(curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L));
    apply_option(curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L));
    apply_option(curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR,
                                  "http,https"));
    apply_option(curl_easy_setopt(curl, CURLOPT_USERAGENT,
                                  "InferLab-edge-cloud/1"));
    if (setup_status != CURLE_OK) {
      result.status = EdgeRuntimeStatus::kFailed;
      result.error_category = "cloud_transport_option_rejected";
      result.detail = curl_easy_strerror(setup_status);
      return result;
    }

    const auto start = SteadyClock::now();
    const CURLcode code = curl_easy_perform(curl);
    const auto end = SteadyClock::now();
    result.metrics.request_duration_ms = positive_milliseconds(end - start);
    result.delivered_chunks = parser.delivered_chunks();
    result.reported_prompt_tokens = parser.reported_prompt_tokens();
    result.reported_completion_tokens =
        parser.reported_completion_tokens();
    if (parser.first_chunk_time().has_value()) {
      result.metrics.first_chunk_latency_ms =
          positive_milliseconds(*parser.first_chunk_time() - start);
      if (result.delivered_chunks > 1U) {
        const double interval =
            positive_milliseconds(end - *parser.first_chunk_time()) /
            static_cast<double>(result.delivered_chunks - 1U);
        result.metrics.stream_chunk_interval_ms = interval;
      }
    }

    long response_code = 0L;
    const CURLcode info_status =
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    char* content_type = nullptr;
    const CURLcode content_type_status =
        curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
    if (cancellation_requested_.load(std::memory_order_acquire) ||
        parser.callback_stopped()) {
      result.status = EdgeRuntimeStatus::kCancelled;
      result.error_category = "cloud_stream_cancelled";
      result.detail = "cloud stream stopped cooperatively";
      result.finish_reason.clear();
      return result;
    }
    if (!parser.parser_error().empty()) {
      result.status = EdgeRuntimeStatus::kFailed;
      result.error_category = "cloud_sse_contract_failed";
      result.detail = parser.parser_error();
      result.finish_reason.clear();
      return result;
    }
    if (code != CURLE_OK) {
      result.status = EdgeRuntimeStatus::kFailed;
      result.error_category = code == CURLE_OPERATION_TIMEDOUT
                                  ? "cloud_deadline_exceeded"
                                  : "cloud_transport_failed";
      result.detail = curl_easy_strerror(code);
      result.finish_reason.clear();
      return result;
    }
    if (info_status != CURLE_OK) {
      result.status = EdgeRuntimeStatus::kFailed;
      result.error_category = "cloud_http_status_unavailable";
      result.detail = curl_easy_strerror(info_status);
      result.finish_reason.clear();
      return result;
    }
    if (response_code < 200L || response_code >= 300L) {
      result.status = EdgeRuntimeStatus::kFailed;
      result.error_category = "cloud_http_status_rejected";
      result.detail = "cloud endpoint returned a non-success HTTP status";
      result.finish_reason.clear();
      return result;
    }
    if (content_type_status != CURLE_OK || content_type == nullptr ||
        !ascii_starts_with_case_insensitive(content_type,
                                            "text/event-stream")) {
      result.status = EdgeRuntimeStatus::kFailed;
      result.error_category = "cloud_content_type_rejected";
      result.detail =
          "cloud endpoint did not return a text/event-stream response";
      result.finish_reason.clear();
      return result;
    }
    if (!parser.finish()) {
      result.status = EdgeRuntimeStatus::kFailed;
      result.error_category = "cloud_sse_contract_failed";
      result.detail = parser.parser_error();
      result.finish_reason.clear();
      return result;
    }
    result.delivered_chunks = parser.delivered_chunks();
    result.reported_prompt_tokens = parser.reported_prompt_tokens();
    result.reported_completion_tokens =
        parser.reported_completion_tokens();
    result.finish_reason = parser.finish_reason();
    if (parser.first_chunk_time().has_value()) {
      result.metrics.first_chunk_latency_ms =
          positive_milliseconds(*parser.first_chunk_time() - start);
      if (result.delivered_chunks > 1U) {
        result.metrics.stream_chunk_interval_ms =
            positive_milliseconds(end - *parser.first_chunk_time()) /
            static_cast<double>(result.delivered_chunks - 1U);
      }
    }
    if (result.finish_reason.empty()) {
      result.status = EdgeRuntimeStatus::kFailed;
      result.error_category = "cloud_finish_reason_missing";
      result.detail = "cloud SSE stream completed without a finish reason";
      return result;
    }
    result.status = EdgeRuntimeStatus::kReady;
    result.error_category.clear();
    result.detail = "cloud stream completed";
    return result;
  }

  void cancel() noexcept override {
    cancellation_requested_.store(true, std::memory_order_release);
  }

 private:
  OpenAiCompatibleTransportConfig config_;
  std::mutex generation_mutex_;
  std::atomic<bool> cancellation_requested_{false};
};

#endif

}  // namespace

void validate_openai_compatible_transport_config(
    const OpenAiCompatibleTransportConfig& config) {
  (void)mature_product_profiles::require_product_profile(config.product_id);
  if (config.endpoint_url.empty() ||
      config.endpoint_url.size() > kMaximumEdgeCloudEndpointBytes ||
      endpoint_contains_unsafe_byte(config.endpoint_url) ||
      !endpoint_has_authority(config.endpoint_url) ||
      config.endpoint_url.find('@') != std::string::npos ||
      config.endpoint_url.find('#') != std::string::npos) {
    throw std::invalid_argument(
        "cloud endpoint is empty, oversized or contains unsafe URL components");
  }
  const bool https = starts_with(config.endpoint_url, "https://");
  const bool loopback_http = loopback_http_endpoint(config.endpoint_url);
  if (!https && !(config.allow_loopback_http && loopback_http)) {
    throw std::invalid_argument(
        "cloud endpoint requires HTTPS; HTTP is limited to explicit loopback development");
  }
  if (!valid_environment_variable_name(
          config.api_key_environment_variable)) {
    throw std::invalid_argument(
        "cloud API key environment variable name is invalid");
  }
  if (config.connect_timeout_ms == 0U ||
      config.connect_timeout_ms > kMaximumEdgeGenerationDurationMs ||
      config.connect_timeout_ms >
          static_cast<std::uint64_t>(std::numeric_limits<long>::max())) {
    throw std::invalid_argument("cloud connect timeout is outside bounds");
  }
  if (config.maximum_event_bytes == 0U ||
      config.maximum_event_bytes > 4U * 1024U * 1024U ||
      config.maximum_response_bytes < config.maximum_event_bytes ||
      config.maximum_response_bytes > 256U * 1024U * 1024U) {
    throw std::invalid_argument("cloud response bounds are invalid");
  }
}

std::unique_ptr<ICloudInferenceTransport>
create_openai_compatible_cloud_transport(
    const OpenAiCompatibleTransportConfig& config) {
  validate_openai_compatible_transport_config(config);
#if defined(INFERLAB_HAS_EDGE_CLOUD_CURL) && INFERLAB_HAS_EDGE_CLOUD_CURL && \
    defined(INFERLAB_HAS_NLOHMANN_JSON) && INFERLAB_HAS_NLOHMANN_JSON
  return std::make_unique<CurlOpenAiCompatibleTransport>(config);
#else
  return std::make_unique<UnavailableCloudTransport>(config);
#endif
}

}  // namespace inferlab
