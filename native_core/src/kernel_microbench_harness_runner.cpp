#include "kernel_microbench_harness_runner.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include "inferlab/attention.hpp"
#include "inferlab/matrix_ops.hpp"
#include "inferlab/normalization.hpp"
#include "inferlab/paged_attention.hpp"
#include "inferlab/quantization.hpp"
#include "inferlab/rope.hpp"

#if defined(INFERLAB_ENABLE_CUDA_BUILD) && INFERLAB_ENABLE_CUDA_BUILD
#include "attention_runtime.hpp"
#include "matrix_ops_runtime.hpp"
#include "normalization_runtime.hpp"
#include "paged_kv_runtime.hpp"
#include "quantization_runtime.hpp"
#include "rope_runtime.hpp"
#endif

#if defined(INFERLAB_HAS_GOOGLE_BENCHMARK) && INFERLAB_HAS_GOOGLE_BENCHMARK
#include <benchmark/benchmark.h>
#endif

#if defined(INFERLAB_HAS_NVTX) && INFERLAB_HAS_NVTX
#if defined(INFERLAB_NVTX3_HEADER) && INFERLAB_NVTX3_HEADER
#include <nvtx3/nvToolsExt.h>
#else
#include <nvToolsExt.h>
#endif
#endif

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace inferlab::kernel_microbench {
namespace {

struct RmsNormData {
  RmsNormShape shape;
  std::vector<float> input;
  std::vector<float> gamma;
};

struct RopeData {
  RopeShape shape;
  std::vector<float> input;
};

struct RowSoftmaxData {
  RowSoftmaxShape shape;
  std::vector<float> input;
};

struct Int4DequantMatvecData {
  Int4DequantMatvecShape shape;
  QuantizedMatrix weights;
  std::vector<float> activation;
};

struct PagedKvGatherData {
  PagedKvGatherShape shape;
  std::vector<float> physical_cache;
  RequestBlocks blocks;
  PagedKVShape paged_shape;
};

struct DecodeAttentionData {
  DecodeAttentionShape shape;
  std::vector<float> query;
  std::vector<float> key;
  std::vector<float> value;
  AttentionShape attention_shape;
};

using PreparedPayload =
    std::variant<RmsNormData, RopeData, RowSoftmaxData,
                 Int4DequantMatvecData, PagedKvGatherData,
                 DecodeAttentionData>;

struct PreparedCase {
  const CaseSpec* spec = nullptr;
  PreparedPayload payload;
};

struct CandidateOutput {
  std::vector<float> values;
  std::optional<double> device_elapsed_ms;
};

std::vector<float> deterministic_values(const std::size_t count,
                                        const int seed,
                                        const int salt) {
  if (seed < 0 || salt < 0) {
    throw std::invalid_argument(
        "microbenchmark deterministic input seed and salt must be non-negative");
  }
  std::vector<float> values(count);
  const auto seed_value = static_cast<std::uint64_t>(seed);
  const auto salt_value = static_cast<std::uint64_t>(salt);
  for (std::size_t index = 0; index < count; ++index) {
    const auto bounded_index = static_cast<std::uint64_t>(index % 4093U);
    const auto pattern =
        (bounded_index * 37U + seed_value * 13U + salt_value * 17U + 11U) %
        257U;
    const auto centered = static_cast<std::int64_t>(pattern) - 128;
    values[index] = static_cast<float>(centered) / 128.0F;
  }
  return values;
}

void require_finite_output(const std::vector<float>& output,
                           const std::size_t expected_size,
                           const std::string& label) {
  if (output.size() != expected_size) {
    throw std::runtime_error(label + " output size mismatch");
  }
  for (const float value : output) {
    if (!std::isfinite(value)) {
      throw std::runtime_error(label + " output contains a non-finite value");
    }
  }
}

PreparedCase prepare_case(const CaseSpec& spec) {
  PreparedCase prepared;
  prepared.spec = &spec;
  prepared.payload = std::visit(
      [&spec](const auto& shape) -> PreparedPayload {
        using T = std::decay_t<decltype(shape)>;
        if constexpr (std::is_same_v<T, RmsNormShape>) {
          const auto elements = internal::checked_product_size(
              shape.rows, shape.hidden, "RMSNorm input");
          auto gamma = deterministic_values(
              static_cast<std::size_t>(shape.hidden),
              spec.deterministic_seed, 2);
          for (float& value : gamma) value = 1.0F + value * 0.1F;
          return RmsNormData{shape,
                             deterministic_values(
                                 elements, spec.deterministic_seed, 1),
                             std::move(gamma)};
        } else if constexpr (std::is_same_v<T, RopeShape>) {
          const auto elements = internal::checked_product_size(
              shape.sequence, shape.head_dimension, "RoPE input");
          return RopeData{
              shape,
              deterministic_values(elements, spec.deterministic_seed, 3)};
        } else if constexpr (std::is_same_v<T, RowSoftmaxShape>) {
          const auto elements = internal::checked_product_size(
              shape.rows, shape.columns, "row softmax input");
          return RowSoftmaxData{
              shape,
              deterministic_values(elements, spec.deterministic_seed, 4)};
        } else if constexpr (
            std::is_same_v<T, Int4DequantMatvecShape>) {
          const auto weight_elements = internal::checked_product_size(
              shape.output_features, shape.input_features,
              "INT4 dequant matvec weights");
          const auto weights = deterministic_values(
              weight_elements, spec.deterministic_seed, 5);
          auto quantized = quantize_symmetric_per_group(
              weights, shape.output_features, shape.input_features,
              shape.group_size, QuantizationBits::kInt4);
          return Int4DequantMatvecData{
              shape, std::move(quantized),
              deterministic_values(
                  static_cast<std::size_t>(shape.input_features),
                  spec.deterministic_seed, 6)};
        } else if constexpr (std::is_same_v<T, PagedKvGatherShape>) {
          const int block_count = internal::checked_ceil_div_non_negative_int(
              shape.tokens, shape.block_size,
              "Paged KV gather block count");
          const auto physical_elements = internal::checked_product_size(
              block_count, shape.block_size, shape.heads,
              shape.head_dimension, "Paged KV gather physical cache");
          RequestBlocks blocks;
          blocks.request_id = 1;
          blocks.token_count = shape.tokens;
          blocks.shared_prefix_tokens = 0;
          blocks.private_token_count = shape.tokens;
          blocks.private_block_start = 0;
          for (int block = 0; block < block_count; ++block) {
            blocks.block_table.push_back(block);
          }
          return PagedKvGatherData{
              shape,
              deterministic_values(
                  physical_elements, spec.deterministic_seed, 7),
              std::move(blocks),
              PagedKVShape{shape.heads, shape.head_dimension,
                           shape.block_size}};
        } else {
          const auto query_elements = internal::checked_product_size(
              shape.heads, shape.head_dimension,
              "decode attention query");
          const auto kv_elements = internal::checked_product_size(
              shape.heads, shape.context, shape.head_dimension,
              "decode attention KV");
          return DecodeAttentionData{
              shape,
              deterministic_values(
                  query_elements, spec.deterministic_seed, 8),
              deterministic_values(
                  kv_elements, spec.deterministic_seed, 9),
              deterministic_values(
                  kv_elements, spec.deterministic_seed, 10),
              AttentionShape{shape.heads, 1, shape.context,
                             shape.head_dimension}};
        }
      },
      spec.shape);
  return prepared;
}

std::vector<float> run_reference(const PreparedCase& prepared) {
  return std::visit(
      [](const auto& data) -> std::vector<float> {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, RmsNormData>) {
          return rms_norm_cpu(data.input, data.shape.rows, data.shape.hidden,
                              data.gamma, 1.0e-5F);
        } else if constexpr (std::is_same_v<T, RopeData>) {
          return apply_rope_cpu(data.input, data.shape.sequence,
                                data.shape.head_dimension, 10000.0F, 0);
        } else if constexpr (std::is_same_v<T, RowSoftmaxData>) {
          return row_softmax_cpu(data.input, data.shape.rows,
                                 data.shape.columns);
        } else if constexpr (std::is_same_v<T, Int4DequantMatvecData>) {
          return fused_dequant_matvec(data.weights, data.activation);
        } else if constexpr (std::is_same_v<T, PagedKvGatherData>) {
          return gather_paged_kv_cache(data.physical_cache, data.blocks,
                                       data.paged_shape);
        } else {
          return scaled_dot_product_attention_cpu(
              data.query, data.key, data.value, data.attention_shape, false);
        }
      },
      prepared.payload);
}

CandidateOutput run_cuda_candidate(const PreparedCase& prepared) {
#if defined(INFERLAB_ENABLE_CUDA_BUILD) && INFERLAB_ENABLE_CUDA_BUILD
  return std::visit(
      [](const auto& data) -> CandidateOutput {
        using T = std::decay_t<decltype(data)>;
        if constexpr (std::is_same_v<T, RmsNormData>) {
          auto result = rms_norm_cuda_runtime_profiled(
              data.input, data.shape.rows, data.shape.hidden, data.gamma,
              1.0e-5F);
          return {std::move(result.output),
                  static_cast<double>(result.elapsed_ms)};
        } else if constexpr (std::is_same_v<T, RopeData>) {
          auto result = apply_rope_cuda_runtime_profiled(
              data.input, data.shape.sequence, data.shape.head_dimension,
              10000.0F, 0);
          return {std::move(result.output),
                  static_cast<double>(result.elapsed_ms)};
        } else if constexpr (std::is_same_v<T, RowSoftmaxData>) {
          auto result = row_softmax_block_cuda_runtime_profiled(
              data.input, data.shape.rows, data.shape.columns);
          return {std::move(result.output),
                  static_cast<double>(result.elapsed_ms)};
        } else if constexpr (std::is_same_v<T, Int4DequantMatvecData>) {
          auto result = fused_dequant_matvec_cuda_runtime_profiled(
              data.weights, data.activation);
          return {std::move(result.output),
                  static_cast<double>(result.elapsed_ms)};
        } else if constexpr (std::is_same_v<T, PagedKvGatherData>) {
          auto result = gather_paged_kv_cache_cuda_runtime_profiled(
              data.physical_cache, data.blocks, data.paged_shape);
          return {std::move(result.output),
                  static_cast<double>(result.elapsed_ms)};
        } else {
          auto result = scaled_dot_product_attention_shared_cuda_runtime_profiled(
              data.query, data.key, data.value, data.attention_shape, false);
          return {std::move(result.output),
                  static_cast<double>(result.elapsed_ms)};
        }
      },
      prepared.payload);
#else
  (void)prepared;
  throw std::runtime_error("CUDA runtime backend was not compiled");
#endif
}

CandidateOutput run_candidate(const PreparedCase& prepared,
                              const ExecutionBackend backend) {
  if (backend == ExecutionBackend::cpu_reference) {
    return {run_reference(prepared), std::nullopt};
  }
  return run_cuda_candidate(prepared);
}

CorrectnessResult run_correctness(const PreparedCase& prepared,
                                  const ExecutionBackend backend) {
  if (prepared.spec == nullptr) {
    throw std::logic_error("prepared microbenchmark case has no specification");
  }
  const CaseSpec& spec = *prepared.spec;
  CorrectnessResult result;
  result.case_id = spec.case_id;
  result.operator_name = spec.operator_name;
  if (spec.operator_contract == nullptr) {
    throw std::logic_error(
        "microbenchmark correctness case has no operator contract");
  }
  result.oracle = spec.operator_contract->correctness_oracle;
  result.attempted = true;
  result.output_item_count = spec.output_item_count;
  try {
    const auto reference = run_reference(prepared);
    require_finite_output(reference, spec.output_item_count,
                          "microbenchmark CPU oracle");
    result.reference_generated = true;
    if (backend == ExecutionBackend::cpu_reference) {
      result.state = CorrectnessState::reference_generated;
      result.passed = true;
      return result;
    }

    const auto candidate = run_cuda_candidate(prepared);
    require_finite_output(candidate.values, spec.output_item_count,
                          "microbenchmark CUDA candidate");
    result.comparison_performed = true;
    double max_absolute_error = 0.0;
    double max_relative_error = 0.0;
    std::size_t mismatch_count = 0;
    for (std::size_t index = 0; index < reference.size(); ++index) {
      const double expected = static_cast<double>(reference[index]);
      const double actual = static_cast<double>(candidate.values[index]);
      const double absolute_error = std::abs(actual - expected);
      const double relative_error =
          absolute_error / std::max(std::abs(expected), 1.0e-12);
      max_absolute_error = std::max(max_absolute_error, absolute_error);
      max_relative_error = std::max(max_relative_error, relative_error);
      const double allowed = spec.absolute_tolerance +
                             spec.relative_tolerance * std::abs(expected);
      if (absolute_error > allowed) {
        mismatch_count = internal::checked_add_size(
            mismatch_count, 1U,
            "microbenchmark correctness mismatch count");
      }
    }
    result.max_absolute_error = max_absolute_error;
    result.max_relative_error = max_relative_error;
    result.mismatch_count = mismatch_count;
    result.passed = mismatch_count == 0U;
    result.state = result.passed ? CorrectnessState::passed
                                 : CorrectnessState::failed;
  } catch (const std::exception& error) {
    result.state = CorrectnessState::runtime_error;
    result.passed = false;
    result.error = inferlab_plan::redact_error_message(error.what());
  }
  return result;
}

bool correctness_allows_timing(const CorrectnessResult& result,
                               const ExecutionBackend backend) {
  if (backend == ExecutionBackend::cpu_reference) {
    return result.state == CorrectnessState::reference_generated &&
           result.passed;
  }
  return result.state == CorrectnessState::passed && result.passed &&
         result.comparison_performed;
}

#if defined(INFERLAB_HAS_NVTX) && INFERLAB_HAS_NVTX
class ScopedNvtxRange {
 public:
  ScopedNvtxRange(const std::string& name, const bool enabled)
      : enabled_(enabled) {
    if (enabled_) nvtxRangePushA(name.c_str());
  }
  ~ScopedNvtxRange() {
    if (enabled_) nvtxRangePop();
  }
  ScopedNvtxRange(const ScopedNvtxRange&) = delete;
  ScopedNvtxRange& operator=(const ScopedNvtxRange&) = delete;

 private:
  bool enabled_ = false;
};
#else
class ScopedNvtxRange {
 public:
  ScopedNvtxRange(const std::string&, bool) {}
};
#endif

#if defined(INFERLAB_HAS_GOOGLE_BENCHMARK) && INFERLAB_HAS_GOOGLE_BENCHMARK
double time_to_microseconds(const double value, const benchmark::TimeUnit unit) {
  switch (unit) {
    case benchmark::kNanosecond:
      return value / 1000.0;
    case benchmark::kMicrosecond:
      return value;
    case benchmark::kMillisecond:
      return value * 1000.0;
    case benchmark::kSecond:
      return value * 1000000.0;
  }
  throw std::logic_error("unknown google/benchmark time unit");
}

class CollectingReporter final : public benchmark::BenchmarkReporter {
 public:
  bool ReportContext(const Context&) override { return true; }

  void ReportRuns(const std::vector<Run>& reports) override {
    for (const auto& run : reports) {
      if (run.run_type != Run::RT_Iteration) continue;
      if (!run.skip_message.empty()) {
        errors.push_back(
            inferlab_plan::redact_error_message(run.skip_message));
        continue;
      }
      const double elapsed_us =
          time_to_microseconds(run.GetAdjustedRealTime(), run.time_unit);
      if (!std::isfinite(elapsed_us) || elapsed_us <= 0.0) {
        errors.push_back("google/benchmark returned non-positive timing");
        continue;
      }
      elapsed_microseconds.push_back(elapsed_us);
    }
  }

  std::vector<double> elapsed_microseconds;
  std::vector<std::string> errors;
};

class BenchmarkLibrarySession {
 public:
  BenchmarkLibrarySession() {
    static char program[] = "inferlab_kernel_microbench_harness";
    char* arguments[] = {program, nullptr};
    int argument_count = 1;
    benchmark::Initialize(&argument_count, arguments);
  }
  ~BenchmarkLibrarySession() {
    benchmark::ClearRegisteredBenchmarks();
    benchmark::Shutdown();
  }
  BenchmarkLibrarySession(const BenchmarkLibrarySession&) = delete;
  BenchmarkLibrarySession& operator=(const BenchmarkLibrarySession&) = delete;
};

double quantile(std::vector<double> values, const double probability) {
  if (values.empty() || !std::isfinite(probability) || probability <= 0.0 ||
      probability > 1.0) {
    throw std::invalid_argument("invalid microbenchmark timing quantile input");
  }
  std::sort(values.begin(), values.end());
  const double rank = std::ceil(probability *
                                static_cast<double>(values.size()));
  const std::size_t one_based = static_cast<std::size_t>(rank);
  const std::size_t index = one_based == 0U ? 0U : one_based - 1U;
  if (index >= values.size()) {
    throw std::logic_error("microbenchmark timing quantile index overflow");
  }
  return values[index];
}

TimingResult run_timing(const PreparedCase& prepared,
                        const HarnessOptions& options,
                        const std::string& nvtx_range) {
  if (prepared.spec == nullptr) {
    throw std::logic_error("prepared timing case has no specification");
  }
  const CaseSpec& spec = *prepared.spec;
  TimingResult result{spec.case_id, spec.operator_name};
  result.attempted = true;
  result.timing_source =
      options.backend == ExecutionBackend::cuda_runtime
          ? "CUDA event elapsed time reported through google/benchmark manual time"
          : "google/benchmark real time for the CPU reference";
  try {
    benchmark::ClearRegisteredBenchmarks();
    auto* registration = benchmark::RegisterBenchmark(
        spec.benchmark_name.c_str(),
        [&prepared, &options, &nvtx_range](benchmark::State& state) {
          for (auto _ : state) {
            (void)_;
            try {
              ScopedNvtxRange range(
                  nvtx_range,
                  options.backend == ExecutionBackend::cuda_runtime);
              auto output = run_candidate(prepared, options.backend);
              require_finite_output(output.values,
                                    prepared.spec->output_item_count,
                                    "timed microbenchmark candidate");
              if (options.backend == ExecutionBackend::cuda_runtime) {
                if (!output.device_elapsed_ms.has_value() ||
                    !std::isfinite(*output.device_elapsed_ms) ||
                    *output.device_elapsed_ms <= 0.0) {
                  throw std::runtime_error(
                      "CUDA profiled runtime returned invalid elapsed time");
                }
                state.SetIterationTime(*output.device_elapsed_ms / 1000.0);
              }
              benchmark::DoNotOptimize(output.values.data());
              benchmark::DoNotOptimize(output.values.size());
              benchmark::ClobberMemory();
            } catch (const std::exception& error) {
              const std::string redacted =
                  inferlab_plan::redact_error_message(error.what());
              state.SkipWithError(redacted);
              break;
            }
          }
        });
    registration->MinTime(options.benchmark_min_time_ms / 1000.0)
        ->Repetitions(options.benchmark_repetitions)
        ->ReportAggregatesOnly(false)
        ->Unit(benchmark::kMicrosecond);
    if (options.backend == ExecutionBackend::cuda_runtime) {
      registration->UseManualTime();
    } else {
      registration->UseRealTime();
    }

    CollectingReporter reporter;
    const std::size_t matched =
        benchmark::RunSpecifiedBenchmarks(&reporter, spec.benchmark_name);
    benchmark::ClearRegisteredBenchmarks();
    if (matched != 1U) {
      throw std::runtime_error(
          "google/benchmark did not match exactly one registered case");
    }
    if (!reporter.errors.empty()) {
      throw std::runtime_error(reporter.errors.front());
    }
    if (reporter.elapsed_microseconds.empty()) {
      throw std::runtime_error(
          "google/benchmark produced no iteration timing records");
    }

    result.reported_run_count = reporter.elapsed_microseconds.size();
    result.latency_us_p50 = quantile(reporter.elapsed_microseconds, 0.50);
    result.latency_us_p99 = quantile(reporter.elapsed_microseconds, 0.99);
    const double throughput =
        static_cast<double>(spec.output_item_count) * 1000000.0 /
        *result.latency_us_p50;
    if (!std::isfinite(throughput) || throughput <= 0.0) {
      throw std::runtime_error(
          "microbenchmark throughput derivation was not finite and positive");
    }
    result.throughput_items_per_second = throughput;
    result.state = TimingState::completed;
  } catch (const std::exception& error) {
    benchmark::ClearRegisteredBenchmarks();
    result.state = TimingState::runtime_error;
    result.error = inferlab_plan::redact_error_message(error.what());
  }
  return result;
}
#endif

}  // namespace

BuildCapabilities compiled_harness_capabilities() {
  BuildCapabilities capabilities;
#if defined(INFERLAB_HAS_GOOGLE_BENCHMARK) && INFERLAB_HAS_GOOGLE_BENCHMARK
  capabilities.google_benchmark_compiled = true;
#endif
#if defined(INFERLAB_ENABLE_CUDA_BUILD) && INFERLAB_ENABLE_CUDA_BUILD
  capabilities.cuda_runtime_compiled = true;
#endif
#if defined(INFERLAB_HAS_NVTX) && INFERLAB_HAS_NVTX
  capabilities.nvtx_compiled = true;
#endif
  return capabilities;
}

void execute_harness_plan(HarnessPlan& plan) {
  if (!plan.options.execute_requested) {
    throw std::invalid_argument(
        "execute_harness_plan requires an explicit execute request");
  }
  if (plan.correctness_results.size() != plan.selected_cases.size() ||
      plan.timing_results.size() != plan.selected_cases.size() ||
      plan.profile_metadata.size() != plan.selected_cases.size()) {
    throw std::logic_error(
        "microbenchmark execution plan collections are misaligned");
  }

  if (plan.options.backend == ExecutionBackend::cuda_runtime &&
      !plan.capabilities.cuda_runtime_compiled) {
    for (std::size_t index = 0; index < plan.selected_cases.size(); ++index) {
      plan.correctness_results[index].state =
          CorrectnessState::backend_unavailable;
      plan.timing_results[index].state =
          TimingState::blocked_by_correctness;
    }
    finalize_harness_plan(plan);
    return;
  }

#if defined(INFERLAB_HAS_GOOGLE_BENCHMARK) && INFERLAB_HAS_GOOGLE_BENCHMARK
  std::unique_ptr<BenchmarkLibrarySession> benchmark_session;
  if (plan.capabilities.google_benchmark_compiled) {
    benchmark_session = std::make_unique<BenchmarkLibrarySession>();
  }
#endif

  for (std::size_t index = 0; index < plan.selected_cases.size(); ++index) {
    const auto& spec = plan.selected_cases[index];
    try {
      auto prepared = prepare_case(spec);
      plan.correctness_results[index] =
          run_correctness(prepared, plan.options.backend);
      if (!correctness_allows_timing(plan.correctness_results[index],
                                     plan.options.backend)) {
        plan.timing_results[index].state =
            TimingState::blocked_by_correctness;
        continue;
      }
      if (!plan.capabilities.google_benchmark_compiled) {
        plan.timing_results[index].state =
            TimingState::benchmark_unavailable;
        continue;
      }
#if defined(INFERLAB_HAS_GOOGLE_BENCHMARK) && INFERLAB_HAS_GOOGLE_BENCHMARK
      plan.timing_results[index] = run_timing(
          prepared, plan.options, plan.profile_metadata[index].nvtx_range);
      plan.profile_metadata[index].nvtx_range_emitted =
          plan.capabilities.nvtx_compiled &&
          plan.options.backend == ExecutionBackend::cuda_runtime &&
          plan.timing_results[index].state == TimingState::completed;
#else
      throw std::logic_error(
          "google/benchmark capability disagrees with compiled source");
#endif
    } catch (const std::exception& error) {
      auto& correctness = plan.correctness_results[index];
      if (!correctness.attempted) {
        correctness.attempted = true;
        correctness.state = CorrectnessState::runtime_error;
        correctness.error =
            inferlab_plan::redact_error_message(error.what());
      }
      auto& timing = plan.timing_results[index];
      if (timing.state == TimingState::not_requested) {
        timing.state = TimingState::blocked_by_correctness;
      }
    }
  }
  finalize_harness_plan(plan);
}

}  // namespace inferlab::kernel_microbench
