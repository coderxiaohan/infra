#include "inferlab/edge_runtime.hpp"

#include "plan_json_utils.hpp"

#include <jni.h>

#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

constexpr std::size_t kMaximumIdentityBytes = 4096U;
constexpr std::size_t kMaximumJniResultBytes = 256U * 1024U;
constexpr std::size_t kMaximumSessions = 1024U;
constexpr jint kAndroidBridgeApiVersion = 1;

struct RuntimeSession {
  explicit RuntimeSession(std::unique_ptr<inferlab::IEdgeRuntime> value)
      : runtime(std::move(value)) {}

  std::mutex lifecycle_mutex;
  std::unique_ptr<inferlab::IEdgeRuntime> runtime;
  std::atomic<bool> closing{false};
};

thread_local const RuntimeSession* g_callback_session = nullptr;

class CallbackSessionScope final {
 public:
  explicit CallbackSessionScope(const RuntimeSession* session)
      : previous_(g_callback_session) {
    g_callback_session = session;
  }

  CallbackSessionScope(const CallbackSessionScope&) = delete;
  CallbackSessionScope& operator=(const CallbackSessionScope&) = delete;

  ~CallbackSessionScope() { g_callback_session = previous_; }

 private:
  const RuntimeSession* previous_ = nullptr;
};

std::mutex g_sessions_mutex;
std::unordered_map<jlong, std::shared_ptr<RuntimeSession>> g_sessions;
jlong g_next_handle = 1;

void throw_java(JNIEnv* env, const char* class_name,
                const std::string& message) noexcept {
  if (env == nullptr || env->ExceptionCheck()) return;
  jclass exception_class = env->FindClass(class_name);
  if (exception_class == nullptr) return;
  const std::string redacted = inferlab_plan::redact_error_message(message);
  env->ThrowNew(exception_class, redacted.c_str());
  env->DeleteLocalRef(exception_class);
}

template <typename Result, typename Callable>
Result jni_call(JNIEnv* env, const Result fallback,
                Callable&& callable) noexcept {
  try {
    return callable();
  } catch (const std::invalid_argument& error) {
    throw_java(env, "java/lang/IllegalArgumentException", error.what());
  } catch (const std::logic_error& error) {
    throw_java(env, "java/lang/IllegalStateException", error.what());
  } catch (const std::exception& error) {
    throw_java(env, "java/lang/RuntimeException", error.what());
  } catch (...) {
    throw_java(env, "java/lang/RuntimeException",
               "unknown native edge runtime failure");
  }
  return fallback;
}

template <typename Callable>
void jni_call_void(JNIEnv* env, Callable&& callable) noexcept {
  try {
    callable();
  } catch (const std::invalid_argument& error) {
    throw_java(env, "java/lang/IllegalArgumentException", error.what());
  } catch (const std::logic_error& error) {
    throw_java(env, "java/lang/IllegalStateException", error.what());
  } catch (const std::exception& error) {
    throw_java(env, "java/lang/RuntimeException", error.what());
  } catch (...) {
    throw_java(env, "java/lang/RuntimeException",
               "unknown native edge runtime failure");
  }
}

std::string byte_array_string(JNIEnv* env, jbyteArray value,
                              const std::size_t maximum_bytes,
                              const char* field, const bool allow_empty,
                              const bool allow_nul = false) {
  if (value == nullptr) {
    throw std::invalid_argument(std::string(field) + " must not be null");
  }
  const jsize length = env->GetArrayLength(value);
  if (length < 0 || static_cast<std::size_t>(length) > maximum_bytes) {
    throw std::invalid_argument(std::string(field) +
                                " exceeds the JNI byte limit");
  }
  if (!allow_empty && length == 0) {
    throw std::invalid_argument(std::string(field) + " must not be empty");
  }
  std::string result(static_cast<std::size_t>(length), '\0');
  if (length > 0) {
    env->GetByteArrayRegion(
        value, 0, length, reinterpret_cast<jbyte*>(result.data()));
    if (env->ExceptionCheck()) {
      throw std::runtime_error(std::string("failed to read ") + field);
    }
  }
  if (!allow_nul && result.find('\0') != std::string::npos) {
    throw std::invalid_argument(std::string(field) +
                                " must not contain a NUL byte");
  }
  return result;
}

std::size_t positive_size(const jlong value, const char* field) {
  if (value <= 0 ||
      static_cast<std::uint64_t>(value) >
          static_cast<std::uint64_t>(
              std::numeric_limits<std::size_t>::max())) {
    throw std::invalid_argument(std::string(field) +
                                " must be a positive native size");
  }
  return static_cast<std::size_t>(value);
}

std::uint64_t non_negative_u64(const jlong value, const char* field) {
  if (value < 0) {
    throw std::invalid_argument(std::string(field) +
                                " must be non-negative");
  }
  return static_cast<std::uint64_t>(value);
}

jbyteArray string_bytes(JNIEnv* env, const std::string& value) {
  if (value.size() > kMaximumJniResultBytes ||
      value.size() > static_cast<std::size_t>(
                         std::numeric_limits<jsize>::max())) {
    throw std::runtime_error("JNI result exceeds the byte-array limit");
  }
  auto* result = env->NewByteArray(static_cast<jsize>(value.size()));
  if (result == nullptr) {
    throw std::runtime_error("failed to allocate JNI result bytes");
  }
  if (!value.empty()) {
    env->SetByteArrayRegion(
        result, 0, static_cast<jsize>(value.size()),
        reinterpret_cast<const jbyte*>(value.data()));
    if (env->ExceptionCheck()) {
      env->DeleteLocalRef(result);
      throw std::runtime_error("failed to write JNI result bytes");
    }
  }
  return result;
}

std::shared_ptr<RuntimeSession> require_session(const jlong handle) {
  if (handle <= 0) {
    throw std::invalid_argument("native edge runtime handle is invalid");
  }
  std::lock_guard<std::mutex> lock(g_sessions_mutex);
  const auto found = g_sessions.find(handle);
  if (found == g_sessions.end()) {
    throw std::invalid_argument(
        "native edge runtime handle is closed or unknown");
  }
  return found->second;
}

jlong register_session(std::unique_ptr<inferlab::IEdgeRuntime> runtime) {
  if (!runtime) {
    throw std::invalid_argument("native edge runtime must not be null");
  }
  std::lock_guard<std::mutex> lock(g_sessions_mutex);
  if (g_sessions.size() >= kMaximumSessions ||
      g_next_handle == std::numeric_limits<jlong>::max()) {
    throw std::runtime_error("native edge runtime session limit reached");
  }
  const jlong handle = g_next_handle++;
  g_sessions.emplace(
      handle, std::make_shared<RuntimeSession>(std::move(runtime)));
  return handle;
}

std::shared_ptr<RuntimeSession> remove_session(const jlong handle) {
  if (handle <= 0) return {};
  std::lock_guard<std::mutex> lock(g_sessions_mutex);
  const auto found = g_sessions.find(handle);
  if (found == g_sessions.end()) return {};
  auto session = found->second;
  g_sessions.erase(found);
  return session;
}

void require_open_session(const RuntimeSession& session) {
  if (session.closing.load(std::memory_order_acquire)) {
    throw std::logic_error("native edge runtime session is closing");
  }
}

void require_not_callback_reentry(const RuntimeSession& session) {
  if (g_callback_session == &session) {
    throw std::logic_error(
        "token callback may only cancel or close its active runtime session");
  }
}

std::string optional_double_json(const std::optional<double>& value) {
  return value.has_value() ? inferlab_plan::fixed6(*value) : "null";
}

std::string optional_u64_json(const std::optional<std::uint64_t>& value) {
  return value.has_value() ? std::to_string(*value) : "null";
}

std::string operation_json(
    const inferlab::EdgeRuntimeOperationResult& result) {
  std::ostringstream out;
  out << "{\n";
  out << "  \"status\": "
      << inferlab_plan::json_string(
             inferlab::edge_runtime_status_name(result.status))
      << ",\n";
  out << "  \"ok\": " << inferlab_plan::json_bool(result.ok()) << ",\n";
  out << "  \"error_category\": "
      << inferlab_plan::json_string(result.error_category) << ",\n";
  out << "  \"detail\": " << inferlab_plan::json_string(result.detail)
      << "\n";
  out << "}\n";
  return out.str();
}

std::string capabilities_json(
    const inferlab::EdgeRuntimeCapabilities& capabilities) {
  std::ostringstream out;
  out << "{\n";
  out << "  \"runtime_id\": "
      << inferlab_plan::json_string(capabilities.runtime_id) << ",\n";
  out << "  \"runtime_version\": "
      << inferlab_plan::json_string(capabilities.runtime_version) << ",\n";
  out << "  \"dependency_linked\": "
      << inferlab_plan::json_bool(capabilities.dependency_linked) << ",\n";
  out << "  \"supports_streaming\": "
      << inferlab_plan::json_bool(capabilities.supports_streaming) << ",\n";
  out << "  \"supports_cancel\": "
      << inferlab_plan::json_bool(capabilities.supports_cancel) << ",\n";
  out << "  \"supports_mmap\": "
      << inferlab_plan::json_bool(capabilities.supports_mmap) << ",\n";
  out << "  \"supports_explicit_thread_count\": "
      << inferlab_plan::json_bool(
             capabilities.supports_explicit_thread_count)
      << ",\n";
  out << "  \"model_formats\": "
      << inferlab_plan::json_string_array(capabilities.model_formats, 2)
      << ",\n";
  out << "  \"backends\": "
      << inferlab_plan::json_string_array(capabilities.backends, 2) << ",\n";
  out << "  \"instruction_sets\": "
      << inferlab_plan::json_string_array(capabilities.instruction_sets, 2)
      << "\n";
  out << "}\n";
  return out.str();
}

std::string generation_json(const inferlab::EdgeGenerationResult& result) {
  std::ostringstream out;
  out << "{\n";
  out << "  \"status\": "
      << inferlab_plan::json_string(
             inferlab::edge_runtime_status_name(result.status))
      << ",\n";
  out << "  \"ok\": " << inferlab_plan::json_bool(result.ok()) << ",\n";
  out << "  \"error_category\": "
      << inferlab_plan::json_string(result.error_category) << ",\n";
  out << "  \"detail\": " << inferlab_plan::json_string(result.detail)
      << ",\n";
  out << "  \"prompt_tokens\": " << result.prompt_tokens << ",\n";
  out << "  \"generated_tokens\": " << result.generated_tokens << ",\n";
  out << "  \"finish_reason\": "
      << inferlab_plan::json_string(result.finish_reason) << ",\n";
  out << "  \"metrics\": {\n";
  out << "    \"load_time_ms\": "
      << optional_double_json(result.metrics.load_time_ms) << ",\n";
  out << "    \"cold_ttft_ms\": "
      << optional_double_json(result.metrics.cold_ttft_ms) << ",\n";
  out << "    \"warm_ttft_ms\": "
      << optional_double_json(result.metrics.warm_ttft_ms) << ",\n";
  out << "    \"tpot_ms\": "
      << optional_double_json(result.metrics.tpot_ms) << ",\n";
  out << "    \"prompt_tokens_per_second\": "
      << optional_double_json(result.metrics.prompt_tokens_per_second)
      << ",\n";
  out << "    \"decode_tokens_per_second\": "
      << optional_double_json(result.metrics.decode_tokens_per_second)
      << ",\n";
  out << "    \"peak_rss_bytes\": "
      << optional_u64_json(result.metrics.peak_rss_bytes) << ",\n";
  out << "    \"peak_pss_bytes\": "
      << optional_u64_json(result.metrics.peak_pss_bytes) << ",\n";
  out << "    \"mapped_model_bytes\": "
      << optional_u64_json(result.metrics.mapped_model_bytes) << ",\n";
  out << "    \"kv_cache_bytes\": "
      << optional_u64_json(result.metrics.kv_cache_bytes) << ",\n";
  out << "    \"runtime_workspace_bytes\": "
      << optional_u64_json(result.metrics.runtime_workspace_bytes) << ",\n";
  out << "    \"provenance\": "
      << inferlab_plan::json_string(result.metrics.provenance) << "\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

class JavaTokenCallback final {
 public:
  JavaTokenCallback(JNIEnv* env, jobject callback) : owner_env_(env) {
    if (callback == nullptr) {
      throw std::invalid_argument("token callback must not be null");
    }
    if (env->GetJavaVM(&vm_) != JNI_OK || vm_ == nullptr) {
      throw std::runtime_error("failed to obtain JavaVM for token callback");
    }
    callback_ = env->NewGlobalRef(callback);
    if (callback_ == nullptr) {
      throw std::runtime_error("failed to retain token callback");
    }
    jclass callback_class = env->GetObjectClass(callback);
    if (callback_class != nullptr) {
      method_ = env->GetMethodID(callback_class, "onToken", "(J[B)Z");
      env->DeleteLocalRef(callback_class);
    }
    if (method_ == nullptr) {
      env->DeleteGlobalRef(callback_);
      callback_ = nullptr;
      throw std::invalid_argument(
          "token callback must implement boolean onToken(long, byte[])");
    }
  }

  JavaTokenCallback(const JavaTokenCallback&) = delete;
  JavaTokenCallback& operator=(const JavaTokenCallback&) = delete;

  ~JavaTokenCallback() {
    if (callback_ != nullptr) owner_env_->DeleteGlobalRef(callback_);
    if (pending_exception_ != nullptr) {
      owner_env_->DeleteGlobalRef(pending_exception_);
    }
  }

  bool invoke(const std::size_t token_index, const std::string& token_text) {
    if (token_index >
        static_cast<std::size_t>(std::numeric_limits<jlong>::max())) {
      transport_failed_.store(true, std::memory_order_release);
      return false;
    }
    JNIEnv* env = nullptr;
    bool attached = false;
    const jint env_status =
        vm_->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (env_status == JNI_EDETACHED) {
      if (vm_->AttachCurrentThread(&env, nullptr) != JNI_OK) {
        transport_failed_.store(true, std::memory_order_release);
        return false;
      }
      attached = true;
    } else if (env_status != JNI_OK || env == nullptr) {
      transport_failed_.store(true, std::memory_order_release);
      return false;
    }

    bool keep_going = false;
    bool callback_invoked = false;
    if (token_text.size() <= inferlab::kMaximumEdgeTokenTextBytes &&
        token_text.size() <= static_cast<std::size_t>(
                                 std::numeric_limits<jsize>::max())) {
      jbyteArray bytes = env->NewByteArray(
          static_cast<jsize>(token_text.size()));
      if (bytes == nullptr) {
        transport_failed_.store(true, std::memory_order_release);
      }
      if (bytes != nullptr && !token_text.empty()) {
        env->SetByteArrayRegion(
            bytes, 0, static_cast<jsize>(token_text.size()),
            reinterpret_cast<const jbyte*>(token_text.data()));
      }
      if (bytes != nullptr && !env->ExceptionCheck()) {
        callback_invoked = true;
        keep_going = env->CallBooleanMethod(
                         callback_, method_, static_cast<jlong>(token_index),
                         bytes) == JNI_TRUE;
      }
      if (!callback_invoked && env->ExceptionCheck()) {
        transport_failed_.store(true, std::memory_order_release);
      }
      if (bytes != nullptr) env->DeleteLocalRef(bytes);
    } else {
      transport_failed_.store(true, std::memory_order_release);
    }
    capture_pending_exception(env);
    if (attached) vm_->DetachCurrentThread();
    return keep_going;
  }

  bool rethrow_if_pending(JNIEnv* env) {
    std::lock_guard<std::mutex> lock(exception_mutex_);
    if (pending_exception_ == nullptr) return false;
    auto* local = static_cast<jthrowable>(env->NewLocalRef(pending_exception_));
    env->DeleteGlobalRef(pending_exception_);
    pending_exception_ = nullptr;
    if (local == nullptr) {
      throw std::runtime_error("failed to relay token callback exception");
    }
    env->Throw(local);
    env->DeleteLocalRef(local);
    return true;
  }

  void throw_if_transport_failed() const {
    if (transport_failed_.load(std::memory_order_acquire)) {
      throw std::runtime_error("token callback JNI transport failed");
    }
  }

 private:
  void capture_pending_exception(JNIEnv* env) {
    if (!env->ExceptionCheck()) return;
    jthrowable local = env->ExceptionOccurred();
    env->ExceptionClear();
    std::lock_guard<std::mutex> lock(exception_mutex_);
    if (pending_exception_ == nullptr && local != nullptr) {
      pending_exception_ =
          static_cast<jthrowable>(env->NewGlobalRef(local));
      if (pending_exception_ == nullptr) {
        transport_failed_.store(true, std::memory_order_release);
      }
    }
    if (local != nullptr) env->DeleteLocalRef(local);
  }

  JNIEnv* owner_env_ = nullptr;
  JavaVM* vm_ = nullptr;
  jobject callback_ = nullptr;
  jmethodID method_ = nullptr;
  std::atomic<bool> transport_failed_{false};
  std::mutex exception_mutex_;
  jthrowable pending_exception_ = nullptr;
};

inferlab::EdgeModelDescriptor model_descriptor(
    JNIEnv* env, jbyteArray logical_model_id, jbyteArray model_format,
    jbyteArray quantization, jbyteArray tokenizer_id,
    jbyteArray checkpoint_revision, jbyteArray conversion_provenance,
    jbyteArray model_path, jbyteArray tokenizer_path, const jlong context_limit,
    const jboolean prefer_mmap) {
  inferlab::EdgeModelDescriptor model;
  model.logical_model_id = byte_array_string(
      env, logical_model_id, kMaximumIdentityBytes, "logicalModelId", false);
  model.model_format = byte_array_string(
      env, model_format, kMaximumIdentityBytes, "modelFormat", false);
  model.quantization = byte_array_string(
      env, quantization, kMaximumIdentityBytes, "quantization", false);
  model.tokenizer_id = byte_array_string(
      env, tokenizer_id, kMaximumIdentityBytes, "tokenizerId", false);
  model.checkpoint_revision = byte_array_string(
      env, checkpoint_revision, kMaximumIdentityBytes,
      "checkpointRevision", false);
  model.conversion_provenance = byte_array_string(
      env, conversion_provenance, kMaximumIdentityBytes,
      "conversionProvenance", false);
  model.model_path = byte_array_string(
      env, model_path, kMaximumIdentityBytes, "modelPath", false);
  model.tokenizer_path = byte_array_string(
      env, tokenizer_path, kMaximumIdentityBytes, "tokenizerPath", true);
  model.model_path_label =
      inferlab_plan::redacted_path_label(model.model_path.string());
  model.context_limit = positive_size(context_limit, "contextLimit");
  model.prefer_mmap = prefer_mmap == JNI_TRUE;
  inferlab::validate_edge_model_descriptor(model);
  return model;
}

inferlab::EdgeGenerationRequest generation_request(
    JNIEnv* env, jbyteArray prompt, const jlong maximum_output_tokens,
    const jlong context_tokens, const jlong batch_size,
    const jlong thread_count, const jlong seed, const jfloat temperature,
    const jfloat top_p) {
  inferlab::EdgeGenerationRequest request;
  request.prompt = byte_array_string(
      env, prompt, inferlab::kMaximumEdgePromptBytes, "utf8Prompt", false,
      true);
  request.maximum_output_tokens =
      positive_size(maximum_output_tokens, "maximumOutputTokens");
  request.context_tokens = positive_size(context_tokens, "contextTokens");
  request.batch_size = positive_size(batch_size, "batchSize");
  request.thread_count = positive_size(thread_count, "threadCount");
  request.seed = non_negative_u64(seed, "seed");
  request.temperature = temperature;
  request.top_p = top_p;
  inferlab::validate_edge_generation_request(request);
  return request;
}

}  // namespace

extern "C" JNIEXPORT jint JNICALL
Java_com_inferlab_edge_InferLabEdgeRuntime_nativeBridgeApiVersion(
    JNIEnv*, jclass) {
  return kAndroidBridgeApiVersion;
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_inferlab_edge_InferLabEdgeRuntime_nativeCapabilities(
    JNIEnv* env, jclass, jbyteArray runtime_id_bytes) {
  return jni_call<jbyteArray>(env, nullptr, [&] {
    const std::string runtime_id = byte_array_string(
        env, runtime_id_bytes, 64U, "runtimeId", false);
    if (inferlab::find_edge_runtime_registration(runtime_id) == nullptr) {
      throw std::invalid_argument("unknown edge runtime id");
    }
    return string_bytes(
        env, capabilities_json(
                 inferlab::edge_runtime_capabilities_for_current_build(
                     runtime_id)));
  });
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_inferlab_edge_InferLabEdgeRuntime_nativeCreate(
    JNIEnv* env, jclass, jbyteArray runtime_id_bytes) {
  return jni_call<jlong>(env, 0, [&] {
    const std::string runtime_id = byte_array_string(
        env, runtime_id_bytes, 64U, "runtimeId", false);
    if (inferlab::find_edge_runtime_registration(runtime_id) == nullptr) {
      throw std::invalid_argument("unknown edge runtime id");
    }
    auto runtime = inferlab::create_edge_runtime(runtime_id);
    if (!runtime) {
      throw std::logic_error(
          "requested edge runtime is not linked in this Android build");
    }
    return register_session(std::move(runtime));
  });
}

extern "C" JNIEXPORT void JNICALL
Java_com_inferlab_edge_InferLabEdgeRuntime_nativeDestroy(
    JNIEnv* env, jclass, const jlong handle) {
  jni_call_void(env, [&] {
    auto session = remove_session(handle);
    if (!session) return;
    session->closing.store(true, std::memory_order_release);
    session->runtime->cancel();
    // Do not wait on lifecycle_mutex. An active callback may be the caller;
    // shared ownership defers runtime destruction until active calls return.
  });
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_inferlab_edge_InferLabEdgeRuntime_nativeLoad(
    JNIEnv* env, jclass, const jlong handle, jbyteArray logical_model_id,
    jbyteArray model_format, jbyteArray quantization, jbyteArray tokenizer_id,
    jbyteArray checkpoint_revision, jbyteArray conversion_provenance,
    jbyteArray model_path, jbyteArray tokenizer_path, const jlong context_limit,
    const jboolean prefer_mmap) {
  return jni_call<jbyteArray>(env, nullptr, [&] {
    auto session = require_session(handle);
    auto model = model_descriptor(
        env, logical_model_id, model_format, quantization, tokenizer_id,
        checkpoint_revision, conversion_provenance, model_path, tokenizer_path,
        context_limit, prefer_mmap);
    require_not_callback_reentry(*session);
    std::lock_guard<std::mutex> lock(session->lifecycle_mutex);
    require_open_session(*session);
    const auto result = session->runtime->load(model);
    inferlab::validate_edge_runtime_operation_result(result);
    return string_bytes(env, operation_json(result));
  });
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_inferlab_edge_InferLabEdgeRuntime_nativeGenerate(
    JNIEnv* env, jclass, const jlong handle, jbyteArray prompt,
    const jlong maximum_output_tokens, const jlong context_tokens,
    const jlong batch_size, const jlong thread_count, const jlong seed,
    const jfloat temperature, const jfloat top_p, jobject callback) {
  return jni_call<jbyteArray>(env, nullptr, [&] {
    auto session = require_session(handle);
    auto request = generation_request(
        env, prompt, maximum_output_tokens, context_tokens, batch_size,
        thread_count, seed, temperature, top_p);
    std::unique_ptr<JavaTokenCallback> callback_bridge;
    if (callback != nullptr) {
      callback_bridge = std::make_unique<JavaTokenCallback>(env, callback);
    }
    require_not_callback_reentry(*session);
    std::lock_guard<std::mutex> lock(session->lifecycle_mutex);
    require_open_session(*session);
    const auto result = session->runtime->generate(
        request,
        [&callback_bridge, &session](const std::size_t index,
                                    const std::string& token_text) {
          if (callback_bridge == nullptr) return true;
          CallbackSessionScope callback_scope(session.get());
          return callback_bridge->invoke(index, token_text);
        });
    inferlab::validate_edge_generation_result_for_request(result, request);
    if (callback_bridge != nullptr &&
        callback_bridge->rethrow_if_pending(env)) {
      return static_cast<jbyteArray>(nullptr);
    }
    if (callback_bridge != nullptr) {
      callback_bridge->throw_if_transport_failed();
    }
    return string_bytes(env, generation_json(result));
  });
}

extern "C" JNIEXPORT void JNICALL
Java_com_inferlab_edge_InferLabEdgeRuntime_nativeCancel(
    JNIEnv* env, jclass, const jlong handle) {
  jni_call_void(env, [&] {
    auto session = require_session(handle);
    session->runtime->cancel();
  });
}

extern "C" JNIEXPORT jbyteArray JNICALL
Java_com_inferlab_edge_InferLabEdgeRuntime_nativeReset(
    JNIEnv* env, jclass, const jlong handle) {
  return jni_call<jbyteArray>(env, nullptr, [&] {
    auto session = require_session(handle);
    require_not_callback_reentry(*session);
    std::lock_guard<std::mutex> lock(session->lifecycle_mutex);
    require_open_session(*session);
    const auto result = session->runtime->reset();
    inferlab::validate_edge_runtime_operation_result(result);
    return string_bytes(env, operation_json(result));
  });
}

extern "C" JNIEXPORT void JNICALL
Java_com_inferlab_edge_InferLabEdgeRuntime_nativeUnload(
    JNIEnv* env, jclass, const jlong handle) {
  jni_call_void(env, [&] {
    auto session = require_session(handle);
    require_not_callback_reentry(*session);
    std::lock_guard<std::mutex> lock(session->lifecycle_mutex);
    require_open_session(*session);
    session->runtime->unload();
  });
}
