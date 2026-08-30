#include "edge_llama_cpp_runtime.hpp"

#include "edge_process_metrics.hpp"
#include "edge_runtime_adapter_utils.hpp"

#include <llama.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab {
namespace {

using edge_runtime_adapter::Clock;
using edge_runtime_adapter::CooperativeGenerationDeadline;
using edge_runtime_adapter::elapsed_ms;
using edge_runtime_adapter::operation_result;

constexpr const char* kRuntimeId = "llama_cpp";

bool llama_abort_callback(void* data) {
  const auto* cancelled = static_cast<const std::atomic<bool>*>(data);
  return cancelled != nullptr && cancelled->load(std::memory_order_relaxed);
}

std::uint32_t sampler_seed(const std::uint64_t seed) {
  return static_cast<std::uint32_t>(seed) ^
         static_cast<std::uint32_t>(seed >> 32U);
}

std::vector<llama_token> tokenize_prompt(const llama_vocab* vocab,
                                         const std::string& prompt,
                                         const std::size_t maximum_tokens) {
  if (prompt.size() >
      static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
    throw std::invalid_argument("prompt exceeds llama.cpp text length range");
  }
  const auto text_size = static_cast<std::int32_t>(prompt.size());
  const std::int32_t required = llama_tokenize(
      vocab, prompt.data(), text_size, nullptr, 0, true, true);
  if (required == std::numeric_limits<std::int32_t>::min() || required >= 0) {
    throw std::runtime_error("llama.cpp failed to size prompt tokens");
  }
  const std::int32_t token_count = -required;
  if (static_cast<std::size_t>(token_count) > maximum_tokens) {
    throw std::invalid_argument(
        "llama.cpp prompt token count exceeds the request prompt budget");
  }
  std::vector<llama_token> tokens(static_cast<std::size_t>(token_count));
  const std::int32_t written = llama_tokenize(
      vocab, prompt.data(), text_size, tokens.data(), token_count, true, true);
  if (written != token_count) {
    throw std::runtime_error("llama.cpp failed to tokenize prompt");
  }
  return tokens;
}

std::string token_piece(const llama_vocab* vocab, const llama_token token) {
  char inline_buffer[128];
  std::int32_t written = llama_token_to_piece(
      vocab, token, inline_buffer,
      static_cast<std::int32_t>(sizeof(inline_buffer)), 0, true);
  if (written >= 0) {
    if (written > static_cast<std::int32_t>(sizeof(inline_buffer))) {
      throw std::runtime_error(
          "llama.cpp token piece exceeded the inline buffer");
    }
    return std::string(inline_buffer, static_cast<std::size_t>(written));
  }
  if (written == std::numeric_limits<std::int32_t>::min()) {
    throw std::runtime_error("llama.cpp token piece size overflow");
  }
  const std::int32_t required = -written;
  if (static_cast<std::size_t>(required) > kMaximumEdgeTokenTextBytes) {
    throw std::runtime_error(
        "llama.cpp token piece exceeds the adapter allocation limit");
  }
  std::vector<char> buffer(static_cast<std::size_t>(required));
  written = llama_token_to_piece(vocab, token, buffer.data(), required, 0,
                                 true);
  if (written < 0 || written > required) {
    throw std::runtime_error("llama.cpp failed to render token piece");
  }
  return std::string(buffer.data(), static_cast<std::size_t>(written));
}

void add_sampler(llama_sampler* chain, llama_sampler* sampler) {
  if (chain == nullptr || sampler == nullptr) {
    if (sampler != nullptr) llama_sampler_free(sampler);
    throw std::runtime_error("llama.cpp failed to create sampler stage");
  }
  llama_sampler_chain_add(chain, sampler);
}

struct ContextDeleter {
  void operator()(llama_context* context) const noexcept {
    if (context != nullptr) llama_free(context);
  }
};

struct SamplerDeleter {
  void operator()(llama_sampler* sampler) const noexcept {
    if (sampler != nullptr) llama_sampler_free(sampler);
  }
};

class LlamaCppEdgeRuntime final : public IEdgeRuntime {
 public:
  ~LlamaCppEdgeRuntime() override { unload(); }

  EdgeRuntimeCapabilities capabilities() const override {
    return edge_runtime_capabilities_for_current_build(kRuntimeId);
  }

  EdgeRuntimeOperationResult load(
      const EdgeModelDescriptor& model) override {
    // A failed replacement load must not leave a previously loaded model
    // reachable under a new request identity.
    unload();
    try {
      validate_edge_model_descriptor(model);
    } catch (const std::exception& error) {
      return operation_result(EdgeRuntimeStatus::kInvalidConfiguration,
                              "invalid_model_descriptor", error.what());
    }
    if (model.model_format != "gguf") {
      return operation_result(EdgeRuntimeStatus::kInvalidConfiguration,
                              "unsupported_model_format",
                              "llama.cpp adapter requires GGUF");
    }
    if (model.prefer_mmap && !llama_supports_mmap()) {
      return operation_result(EdgeRuntimeStatus::kInvalidConfiguration,
                              "mmap_unavailable",
                              "requested mmap is unavailable in linked llama.cpp");
    }
    try {
      static std::once_flag backend_once;
      std::call_once(backend_once, [] { ggml_backend_load_all(); });

      llama_model_params params = llama_model_default_params();
      params.n_gpu_layers = 0;
      params.load_mode = model.prefer_mmap ? LLAMA_LOAD_MODE_MMAP
                                           : LLAMA_LOAD_MODE_NONE;
      const auto begin = Clock::now();
      model_ = llama_model_load_from_file(model.model_path.string().c_str(),
                                          params);
      const auto end = Clock::now();
      if (model_ == nullptr) {
        return operation_result(
            EdgeRuntimeStatus::kFailed, "model_load_failed",
            "llama.cpp could not load model label " + model.model_path_label);
      }
      if (llama_model_has_encoder(model_) ||
          !llama_model_has_decoder(model_)) {
        unload();
        return operation_result(
            EdgeRuntimeStatus::kInvalidConfiguration,
            "unsupported_model_architecture",
            "first llama.cpp adapter scope requires a decoder-only GGUF");
      }
      model_descriptor_ = model;
      load_time_ms_ = elapsed_ms(begin, end);
      process_metrics_.reset();
      process_metrics_.bind_mapped_file(model.model_path);
      process_metrics_.observe();
      generate_count_ = 0U;
      cancel_requested_.store(false, std::memory_order_relaxed);
      return operation_result(EdgeRuntimeStatus::kReady, "", "model_loaded");
    } catch (const std::exception&) {
      unload();
      return operation_result(EdgeRuntimeStatus::kFailed,
                              "runtime_load_exception",
                              "llama.cpp adapter load raised an exception");
    }
  }

  EdgeGenerationResult generate(
      const EdgeGenerationRequest& request,
      const EdgeTokenCallback& on_token) override {
    // This store is the new generation's cooperative-cancellation
    // linearization point. A later cross-thread cancel is never cleared.
    cancel_requested_.store(false, std::memory_order_relaxed);
    EdgeGenerationResult result;
    result.metrics.load_time_ms = load_time_ms_;
    result.metrics.provenance =
        "llama_cpp_11cd988_in_process_steady_clock;cooperative_generation_deadline_process_scheduler;peak_rss_is_process_lifetime_getrusage;peak_pss_is_max_of_adapter_checkpoints;mapped_model_bytes_is_latest_proc_maps_checkpoint_exact_main_path";
    project_edge_process_metrics(process_metrics_, &result.metrics);
    if (model_ == nullptr) {
      result.status = EdgeRuntimeStatus::kNotLoaded;
      result.error_category = "model_not_loaded";
      result.detail = "load must complete before generate";
      return result;
    }
    EdgeProcessMetricFinalizer process_metric_finalizer(
        &process_metrics_, model_descriptor_.model_path);
    std::unique_ptr<CooperativeGenerationDeadline> deadline;
    const auto finish_generation =
        [&deadline, &process_metric_finalizer](EdgeGenerationResult value) {
          if (deadline) value = deadline->finish(std::move(value));
          return process_metric_finalizer.finish(std::move(value));
        };
    std::unique_ptr<llama_context, ContextDeleter> context;
    std::unique_ptr<llama_sampler, SamplerDeleter> sampler;
    try {
      validate_edge_generation_request(request);
      if (request.thread_count >
              static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
          request.context_tokens >
              static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max())) {
        throw std::invalid_argument("request exceeds llama.cpp integer range");
      }
      if (request.context_tokens > model_descriptor_.context_limit) {
        throw std::invalid_argument(
            "requested context exceeds model descriptor limit");
      }
      deadline = std::make_unique<CooperativeGenerationDeadline>(
          request.maximum_generation_duration_ms, &cancel_requested_);

      const auto generate_begin = Clock::now();
      const llama_vocab* vocab = llama_model_get_vocab(model_);
      if (vocab == nullptr) {
        throw std::runtime_error("llama.cpp model has no vocabulary");
      }
      auto prompt_tokens =
          tokenize_prompt(vocab, request.prompt,
                          request.context_tokens - request.maximum_output_tokens);
      if (prompt_tokens.empty() ||
          prompt_tokens.size() > request.context_tokens ||
          request.maximum_output_tokens >
              request.context_tokens - prompt_tokens.size()) {
        throw std::invalid_argument(
            "prompt and maximum output exceed configured context");
      }

      llama_context_params context_params = llama_context_default_params();
      context_params.n_ctx = static_cast<std::uint32_t>(request.context_tokens);
      context_params.n_batch =
          std::min(context_params.n_batch, context_params.n_ctx);
      context_params.n_ubatch =
          std::min(context_params.n_ubatch, context_params.n_batch);
      if (context_params.n_batch == 0U || context_params.n_ubatch == 0U) {
        throw std::runtime_error(
            "llama.cpp default token batch capacity is invalid");
      }
      context_params.n_threads = static_cast<std::int32_t>(request.thread_count);
      context_params.n_threads_batch =
          static_cast<std::int32_t>(request.thread_count);
      context_params.abort_callback = llama_abort_callback;
      context_params.abort_callback_data = &cancel_requested_;
      context_params.no_perf = false;
      context.reset(llama_init_from_model(model_, context_params));
      if (!context) {
        throw std::runtime_error("llama.cpp failed to create context");
      }
      const auto actual_context_tokens = llama_n_ctx(context.get());
      const auto actual_sequence_tokens = llama_n_ctx_seq(context.get());
      const auto actual_batch_tokens = llama_n_batch(context.get());
      const auto actual_microbatch_tokens = llama_n_ubatch(context.get());
      if (actual_context_tokens < request.context_tokens ||
          actual_sequence_tokens < request.context_tokens) {
        throw std::invalid_argument(
            "llama.cpp actual context is smaller than the requested context");
      }
      if (actual_batch_tokens == 0U || actual_microbatch_tokens == 0U) {
        throw std::runtime_error(
            "llama.cpp actual token batch capacity is invalid");
      }
      const auto prefill_batch_tokens = static_cast<std::size_t>(
          std::min(actual_batch_tokens,
                   static_cast<std::uint32_t>(
                       std::numeric_limits<std::int32_t>::max())));

      llama_sampler_chain_params sampler_params =
          llama_sampler_chain_default_params();
      sampler_params.no_perf = false;
      sampler.reset(llama_sampler_chain_init(sampler_params));
      if (!sampler) {
        throw std::runtime_error("llama.cpp failed to create sampler");
      }
      if (request.temperature == 0.0F) {
        add_sampler(sampler.get(), llama_sampler_init_greedy());
      } else {
        add_sampler(sampler.get(),
                    llama_sampler_init_top_p(request.top_p, 1U));
        add_sampler(sampler.get(),
                    llama_sampler_init_temp(request.temperature));
        add_sampler(sampler.get(),
                    llama_sampler_init_dist(sampler_seed(request.seed)));
      }
      const auto prefill_begin = Clock::now();
      for (std::size_t offset = 0U; offset < prompt_tokens.size();) {
        if (cancel_requested_.load(std::memory_order_relaxed)) {
          result.status = EdgeRuntimeStatus::kCancelled;
          result.error_category = "cancelled_during_prefill";
          result.finish_reason = "cancelled";
          return finish_generation(std::move(result));
        }
        const std::size_t chunk = std::min(
            prefill_batch_tokens, prompt_tokens.size() - offset);
        llama_batch batch = llama_batch_get_one(
            prompt_tokens.data() + offset, static_cast<std::int32_t>(chunk));
        const std::int32_t decode_status = llama_decode(context.get(), batch);
        if (decode_status != 0) {
          if (decode_status == 2 ||
              cancel_requested_.load(std::memory_order_relaxed)) {
            result.status = EdgeRuntimeStatus::kCancelled;
            result.error_category = "cancelled_during_prefill";
            result.finish_reason = "cancelled";
            return finish_generation(std::move(result));
          }
          throw std::runtime_error("llama.cpp prompt decode failed");
        }
        offset += chunk;
      }
      const auto prefill_end = Clock::now();
      result.prompt_tokens = prompt_tokens.size();
      const double prefill_ms = elapsed_ms(prefill_begin, prefill_end);
      if (prefill_ms > 0.0) {
        result.metrics.prompt_tokens_per_second =
            static_cast<double>(result.prompt_tokens) * 1000.0 / prefill_ms;
      }

      std::optional<Clock::time_point> first_token_time;
      std::optional<Clock::time_point> last_token_time;
      for (std::size_t index = 0U;
           index < request.maximum_output_tokens; ++index) {
        if (cancel_requested_.load(std::memory_order_relaxed)) {
          result.status = EdgeRuntimeStatus::kCancelled;
          result.error_category = "cancelled_during_decode";
          result.finish_reason = "cancelled";
          return finish_generation(std::move(result));
        }
        llama_token token =
            llama_sampler_sample(sampler.get(), context.get(), -1);
        if (llama_vocab_is_eog(vocab, token)) {
          result.finish_reason = "end_of_generation";
          break;
        }
        const std::string piece = token_piece(vocab, token);
        const auto callback_time = Clock::now();
        if (!first_token_time.has_value()) {
          first_token_time = callback_time;
          const double ttft = elapsed_ms(generate_begin, callback_time);
          if (generate_count_ == 0U) {
            result.metrics.cold_ttft_ms = ttft;
          } else {
            result.metrics.warm_ttft_ms = ttft;
          }
        }
        last_token_time = callback_time;
        ++result.generated_tokens;
        if (on_token && !on_token(index, piece)) {
          cancel_requested_.store(true, std::memory_order_relaxed);
          result.status = EdgeRuntimeStatus::kCancelled;
          result.error_category = "cancelled_by_token_callback";
          result.finish_reason = "cancelled";
          return finish_generation(std::move(result));
        }
        if (cancel_requested_.load(std::memory_order_relaxed)) {
          result.status = EdgeRuntimeStatus::kCancelled;
          result.error_category = "cancelled_externally";
          result.finish_reason = "cancelled";
          return finish_generation(std::move(result));
        }
        if (result.generated_tokens == request.maximum_output_tokens) {
          result.finish_reason = "length";
          break;
        }
        llama_batch batch = llama_batch_get_one(&token, 1);
        const std::int32_t decode_status = llama_decode(context.get(), batch);
        if (decode_status != 0) {
          if (decode_status == 2 ||
              cancel_requested_.load(std::memory_order_relaxed)) {
            result.status = EdgeRuntimeStatus::kCancelled;
            result.error_category = "cancelled_during_decode";
            result.finish_reason = "cancelled";
            return finish_generation(std::move(result));
          }
          throw std::runtime_error("llama.cpp token decode failed");
        }
      }
      if (result.finish_reason.empty()) result.finish_reason = "length";
      if (first_token_time.has_value()) {
        const double decode_ms =
            elapsed_ms(*first_token_time, *last_token_time);
        if (result.generated_tokens > 1U && decode_ms > 0.0) {
          result.metrics.tpot_ms =
              decode_ms / static_cast<double>(result.generated_tokens - 1U);
          result.metrics.decode_tokens_per_second =
              static_cast<double>(result.generated_tokens - 1U) * 1000.0 /
              decode_ms;
        }
      }
      ++generate_count_;
      result.status = EdgeRuntimeStatus::kReady;
      result.detail = "generation_completed";
      return finish_generation(std::move(result));
    } catch (const std::invalid_argument& error) {
      result.status = EdgeRuntimeStatus::kInvalidConfiguration;
      result.error_category = "invalid_generation_request";
      result.detail = error.what();
      return finish_generation(std::move(result));
    } catch (const std::exception& error) {
      result.status = EdgeRuntimeStatus::kFailed;
      result.error_category = "runtime_generation_failed";
      result.detail = error.what();
      return finish_generation(std::move(result));
    }
  }

  void cancel() noexcept override {
    cancel_requested_.store(true, std::memory_order_relaxed);
  }

  EdgeRuntimeOperationResult reset() override {
    if (model_ == nullptr) {
      return operation_result(EdgeRuntimeStatus::kNotLoaded,
                              "model_not_loaded",
                              "reset requires a loaded model");
    }
    cancel_requested_.store(false, std::memory_order_relaxed);
    return operation_result(EdgeRuntimeStatus::kReady, "", "runtime_reset");
  }

  void unload() noexcept override {
    cancel_requested_.store(true, std::memory_order_relaxed);
    if (model_ != nullptr) {
      llama_model_free(model_);
      model_ = nullptr;
    }
    model_descriptor_ = {};
    load_time_ms_.reset();
    process_metrics_.reset();
    generate_count_ = 0U;
  }

 private:
  llama_model* model_ = nullptr;
  EdgeModelDescriptor model_descriptor_;
  std::optional<double> load_time_ms_;
  EdgeProcessMetricTracker process_metrics_;
  std::size_t generate_count_ = 0U;
  std::atomic<bool> cancel_requested_{false};
};

}  // namespace

std::unique_ptr<IEdgeRuntime> make_llama_cpp_edge_runtime() {
  return std::make_unique<LlamaCppEdgeRuntime>();
}

}  // namespace inferlab
