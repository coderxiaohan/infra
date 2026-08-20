#include "edge_executorch_runtime.hpp"

#include "edge_process_metrics.hpp"
#include "edge_runtime_adapter_utils.hpp"

#include <executorch/extension/llm/runner/llm_runner_helper.h>
#include <executorch/extension/llm/runner/stats.h>
#include <executorch/extension/llm/runner/text_llm_runner.h>
#include <executorch/extension/module/module.h>
#include <executorch/runtime/core/error.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <limits>
#include <memory>
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
namespace et_llm = ::executorch::extension::llm;
using ::executorch::extension::Module;
using ::executorch::runtime::Error;

constexpr const char* kRuntimeId = "executorch";

std::optional<double> positive_duration_ms(const long begin,
                                           const long end) {
  if (begin <= 0L || end < begin) return std::nullopt;
  return static_cast<double>(end - begin);
}

std::optional<std::size_t> checked_stats_count(
    const std::int64_t value) noexcept {
  if (value < 0 ||
      static_cast<std::uint64_t>(value) >
          static_cast<std::uint64_t>(
              std::numeric_limits<std::size_t>::max())) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(value);
}

struct StatsProjection {
  bool observed = false;
  std::int64_t prompt_tokens = 0;
  std::int64_t decode_tokens_after_first = 0;
  long inference_start_ms = 0L;
  long prompt_eval_end_ms = 0L;
  long first_token_ms = 0L;
  long inference_end_ms = 0L;
};

bool valid_stats_timeline(const StatsProjection& stats,
                          const std::size_t generated_tokens) noexcept {
  if (stats.inference_start_ms <= 0L ||
      stats.prompt_eval_end_ms < stats.inference_start_ms ||
      stats.inference_end_ms < stats.prompt_eval_end_ms) {
    return false;
  }
  // TextLLMRunner emits the token produced by prefill, then records
  // prompt_eval_end_ms before entering its remaining decode loop.
  return generated_tokens > 0U &&
         stats.first_token_ms >= stats.inference_start_ms &&
         stats.first_token_ms <= stats.prompt_eval_end_ms;
}

void project_first_token_latency(const StatsProjection& stats,
                                 const std::size_t generate_count,
                                 EdgeRuntimeMetrics* metrics) {
  if (metrics == nullptr || !stats.observed ||
      stats.first_token_ms < stats.inference_start_ms ||
      stats.first_token_ms > stats.prompt_eval_end_ms) {
    return;
  }
  const auto ttft_ms =
      positive_duration_ms(stats.inference_start_ms, stats.first_token_ms);
  if (!ttft_ms.has_value()) return;
  if (generate_count == 0U) {
    metrics->cold_ttft_ms = ttft_ms;
  } else {
    metrics->warm_ttft_ms = ttft_ms;
  }
}

class ExecuTorchEdgeRuntime final : public IEdgeRuntime {
 public:
  ~ExecuTorchEdgeRuntime() override { unload(); }

  EdgeRuntimeCapabilities capabilities() const override {
    return edge_runtime_capabilities_for_current_build(kRuntimeId);
  }

  EdgeRuntimeOperationResult load(
      const EdgeModelDescriptor& model) override {
    // Replacement is fail-closed: any failed load leaves the adapter empty.
    unload();
    try {
      validate_edge_model_descriptor(model);
    } catch (const std::exception& error) {
      return operation_result(EdgeRuntimeStatus::kInvalidConfiguration,
                              "invalid_model_descriptor", error.what());
    }
    if (model.model_format != "pte") {
      return operation_result(EdgeRuntimeStatus::kInvalidConfiguration,
                              "unsupported_model_format",
                              "ExecuTorch adapter requires PTE");
    }
    if (model.tokenizer_path.empty()) {
      return operation_result(EdgeRuntimeStatus::kInvalidConfiguration,
                              "tokenizer_artifact_missing",
                              "ExecuTorch adapter requires a tokenizer artifact");
    }
    try {
      const auto begin = Clock::now();
      auto tokenizer = et_llm::load_tokenizer(model.tokenizer_path.string());
      if (!tokenizer) {
        return operation_result(
            EdgeRuntimeStatus::kFailed, "tokenizer_load_failed",
            "ExecuTorch could not load the configured tokenizer artifact");
      }
      const Module::LoadMode load_mode =
          model.prefer_mmap
              ? Module::LoadMode::MmapUseMlockIgnoreErrors
              : Module::LoadMode::File;
      runner_ = et_llm::create_text_llm_runner(
          model.model_path.string(), std::move(tokenizer),
          std::vector<std::string>{}, -1.0F, nullptr, "forward", load_mode);
      if (!runner_) {
        return operation_result(
            EdgeRuntimeStatus::kFailed, "runner_creation_failed",
            "ExecuTorch could not create the text LLM runner");
      }
      const Error load_status = runner_->load();
      const auto end = Clock::now();
      if (load_status != Error::Ok || !runner_->is_loaded()) {
        runner_.reset();
        return operation_result(EdgeRuntimeStatus::kFailed,
                                "model_load_failed",
                                "ExecuTorch runner failed to load PTE");
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
                              "ExecuTorch adapter load raised an exception");
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
        "executorch_v1_2_0_text_llm_runner_stats_first_token_normalized;cooperative_generation_deadline_process_scheduler;peak_rss_is_process_lifetime_getrusage;peak_pss_is_max_of_adapter_checkpoints;mapped_model_bytes_is_latest_proc_maps_checkpoint_exact_pte_path;thread_count_runtime_managed";
    project_edge_process_metrics(process_metrics_, &result.metrics);
    if (!runner_ || !runner_->is_loaded()) {
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
    try {
      validate_edge_generation_request(request);
      if (request.maximum_output_tokens >
              static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) ||
          request.context_tokens >
              static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) {
        throw std::invalid_argument(
            "request exceeds ExecuTorch GenerationConfig integer range");
      }
      if (request.context_tokens > model_descriptor_.context_limit) {
        throw std::invalid_argument(
            "requested context exceeds model descriptor limit");
      }
      if (request.temperature != 0.0F || request.top_p != 1.0F) {
        throw std::invalid_argument(
            "first ExecuTorch adapter scope supports greedy generation only");
      }
      deadline = std::make_unique<CooperativeGenerationDeadline>(
          request.maximum_generation_duration_ms, &cancel_requested_);

      et_llm::GenerationConfig config;
      config.echo = false;
      config.ignore_eos = false;
      config.max_new_tokens =
          static_cast<std::int32_t>(request.maximum_output_tokens);
      config.seq_len = static_cast<std::int32_t>(request.context_tokens);
      config.temperature = 0.0F;

      std::size_t callback_count = 0U;
      std::atomic<bool> callback_cancelled{false};
      std::atomic<bool> callback_contract_violated{false};
      std::atomic<bool> callback_text_limit_violated{false};
      StatsProjection stats;
      if (cancel_requested_.load(std::memory_order_relaxed)) {
        result.status = EdgeRuntimeStatus::kCancelled;
        result.error_category = "cancelled_externally";
        result.finish_reason = "cancelled";
        return finish_generation(std::move(result));
      }
      const Error generation_status = runner_->generate(
          request.prompt, config,
          [&](const std::string& piece) {
            if (cancel_requested_.load(std::memory_order_relaxed)) {
              stop_runner_noexcept();
              return;
            }
            if (piece.size() > kMaximumEdgeTokenTextBytes) {
              callback_text_limit_violated.store(true,
                                                  std::memory_order_relaxed);
              cancel_requested_.store(true, std::memory_order_relaxed);
              stop_runner_noexcept();
              return;
            }
            if (callback_count >= request.maximum_output_tokens) {
              callback_contract_violated.store(true,
                                                std::memory_order_relaxed);
              cancel_requested_.store(true, std::memory_order_relaxed);
              stop_runner_noexcept();
              return;
            }
            const std::size_t token_index = callback_count++;
            if (on_token && !on_token(token_index, piece)) {
              callback_cancelled.store(true, std::memory_order_relaxed);
              cancel_requested_.store(true, std::memory_order_relaxed);
              stop_runner_noexcept();
              return;
            }
            if (cancel_requested_.load(std::memory_order_relaxed)) {
              // IEdgeRuntime permits cross-thread cancel. Keep third-party
              // stop mutation on the generation callback thread.
              stop_runner_noexcept();
            }
          },
          [&](const et_llm::Stats& value) {
            stats.observed = true;
            stats.prompt_tokens = value.num_prompt_tokens;
            stats.decode_tokens_after_first = value.num_generated_tokens;
            stats.inference_start_ms = value.inference_start_ms;
            stats.prompt_eval_end_ms = value.prompt_eval_end_ms;
            stats.first_token_ms = value.first_token_ms;
            stats.inference_end_ms = value.inference_end_ms;
          });
      result.generated_tokens = callback_count;
      if (result.generated_tokens > 0U) {
        project_first_token_latency(stats, generate_count_, &result.metrics);
      }
      if (callback_text_limit_violated.load(std::memory_order_relaxed)) {
        result.status = EdgeRuntimeStatus::kFailed;
        result.error_category = "executorch_token_text_limit_violation";
        result.detail =
            "ExecuTorch emitted token text above the runtime contract limit";
        return finish_generation(std::move(result));
      }
      if (callback_contract_violated.load(std::memory_order_relaxed)) {
        result.status = EdgeRuntimeStatus::kFailed;
        result.error_category = "executorch_callback_limit_violation";
        result.detail =
            "ExecuTorch emitted more token callbacks than the request limit";
        return finish_generation(std::move(result));
      }
      if (cancel_requested_.load(std::memory_order_relaxed)) {
        result.status = EdgeRuntimeStatus::kCancelled;
        result.error_category =
            callback_cancelled.load(std::memory_order_relaxed)
                ? "cancelled_by_token_callback"
                : "cancelled_externally";
        result.finish_reason = "cancelled";
        return finish_generation(std::move(result));
      }
      if (generation_status != Error::Ok) {
        result.status = EdgeRuntimeStatus::kFailed;
        result.error_category = "executorch_generate_error";
        result.detail = "ExecuTorch TextLLMRunner generate returned an error";
        return finish_generation(std::move(result));
      }
      const auto prompt_tokens = checked_stats_count(stats.prompt_tokens);
      const auto decode_tokens_after_first =
          checked_stats_count(stats.decode_tokens_after_first);
      if (!stats.observed || !prompt_tokens.has_value() ||
          !decode_tokens_after_first.has_value() ||
          *decode_tokens_after_first ==
              std::numeric_limits<std::size_t>::max()) {
        result.status = EdgeRuntimeStatus::kFailed;
        result.error_category = "executorch_stats_unavailable";
        result.detail = "ExecuTorch generation completed without valid Stats";
        return finish_generation(std::move(result));
      }
      // In pinned ExecuTorch v1.2.0 the prefill-produced first token is sent to
      // the callback before TextTokenGenerator starts. Stats counts only the
      // subsequent decode-loop tokens.
      const std::size_t generated_tokens = *decode_tokens_after_first + 1U;
      if (*prompt_tokens > request.context_tokens ||
          generated_tokens > request.maximum_output_tokens ||
          generated_tokens > request.context_tokens - *prompt_tokens) {
        result.status = EdgeRuntimeStatus::kFailed;
        result.error_category = "executorch_stats_contract_violation";
        result.detail =
            "ExecuTorch Stats exceed configured context or output limits";
        return finish_generation(std::move(result));
      }
      if (callback_count != generated_tokens) {
        result.status = EdgeRuntimeStatus::kFailed;
        result.error_category = "executorch_callback_stats_mismatch";
        result.detail =
            "ExecuTorch callback count does not match normalized generated token Stats";
        return finish_generation(std::move(result));
      }
      if (!valid_stats_timeline(stats, generated_tokens)) {
        result.status = EdgeRuntimeStatus::kFailed;
        result.error_category = "executorch_stats_timeline_violation";
        result.detail =
            "ExecuTorch Stats timestamps violate the generation timeline";
        return finish_generation(std::move(result));
      }
      result.prompt_tokens = *prompt_tokens;
      result.generated_tokens = generated_tokens;
      result.finish_reason =
          result.generated_tokens >= request.maximum_output_tokens
              ? "length"
              : "runtime_stop_unclassified";

      const auto prompt_ms = positive_duration_ms(
          stats.inference_start_ms, stats.prompt_eval_end_ms);
      if (prompt_ms.has_value() && *prompt_ms > 0.0) {
        result.metrics.prompt_tokens_per_second =
            static_cast<double>(result.prompt_tokens) * 1000.0 / *prompt_ms;
      }
      const auto decode_ms = positive_duration_ms(
          stats.prompt_eval_end_ms, stats.inference_end_ms);
      if (decode_ms.has_value() && *decode_ms > 0.0 &&
          *decode_tokens_after_first > 0U) {
        result.metrics.decode_tokens_per_second =
            static_cast<double>(*decode_tokens_after_first) * 1000.0 /
            *decode_ms;
      }
      const auto inter_token_ms = positive_duration_ms(
          stats.first_token_ms, stats.inference_end_ms);
      if (inter_token_ms.has_value() && *inter_token_ms > 0.0 &&
          result.generated_tokens > 1U) {
        result.metrics.tpot_ms =
            *inter_token_ms /
            static_cast<double>(result.generated_tokens - 1U);
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
    if (!runner_) {
      return operation_result(EdgeRuntimeStatus::kNotLoaded,
                              "model_not_loaded",
                              "reset requires a loaded runner");
    }
    runner_->reset();
    cancel_requested_.store(false, std::memory_order_relaxed);
    return operation_result(EdgeRuntimeStatus::kReady, "", "runtime_reset");
  }

  void unload() noexcept override {
    cancel_requested_.store(true, std::memory_order_relaxed);
    stop_runner_noexcept();
    runner_.reset();
    model_descriptor_ = {};
    load_time_ms_.reset();
    process_metrics_.reset();
    generate_count_ = 0U;
  }

 private:
  void stop_runner_noexcept() noexcept {
    try {
      if (runner_) (void)runner_->stop();
    } catch (...) {
      // cancel/unload are no-throw cleanup boundaries.
    }
  }

  std::unique_ptr<et_llm::TextLLMRunner> runner_;
  EdgeModelDescriptor model_descriptor_;
  std::optional<double> load_time_ms_;
  EdgeProcessMetricTracker process_metrics_;
  std::size_t generate_count_ = 0U;
  std::atomic<bool> cancel_requested_{false};
};

}  // namespace

std::unique_ptr<IEdgeRuntime> make_executorch_edge_runtime() {
  return std::make_unique<ExecuTorchEdgeRuntime>();
}

}  // namespace inferlab
