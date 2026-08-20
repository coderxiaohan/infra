#include "edge_runtime_resource_plan.hpp"

#include "checked_index.hpp"
#include "edge_runtime_matrix_internal.hpp"
#include "edge_runtime_matrix_model.hpp"
#include "inferlab/edge_runtime.hpp"
#include "inferlab/kv_cache.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace inferlab::edge_runtime_matrix {
namespace {

using matrix_detail::append_unique;

std::uint64_t checked_u64_from_i64(const std::int64_t value,
                                   const char* name) {
  if (value < 0) {
    throw std::overflow_error(std::string(name) + " must be non-negative");
  }
  return static_cast<std::uint64_t>(value);
}

int checked_int_from_size(const std::size_t value, const char* name) {
  if (value > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::overflow_error(std::string(name) + " exceeds int range");
  }
  return static_cast<int>(value);
}

std::uint64_t estimate_edge_kv_bytes(const RuntimeRow& row) {
  const KVCacheConfig kv_config{
      checked_int_from_size(row.concurrent_sessions,
                            "edge concurrent sessions"),
      checked_int_from_size(row.context_tokens, "edge context tokens"),
      row.layers,
      row.kv_heads,
      row.head_dim,
      parse_dtype(row.kv_dtype),
  };
  const auto estimate = estimate_kv_cache(kv_config);
  return checked_u64_from_i64(estimate.total_bytes,
                              "edge estimated KV bytes");
}

std::uint64_t checked_resource_total(const std::uint64_t first,
                                     const std::uint64_t second,
                                     const std::uint64_t third,
                                     const std::uint64_t fourth,
                                     const char* name) {
  const auto maximum =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  if (first > maximum || second > maximum || third > maximum ||
      fourth > maximum) {
    throw std::overflow_error(std::string(name) +
                              " input exceeds signed byte range");
  }
  std::int64_t total = internal::checked_add_i64(
      static_cast<std::int64_t>(first), static_cast<std::int64_t>(second),
      name);
  total = internal::checked_add_i64(
      total, static_cast<std::int64_t>(third), name);
  total = internal::checked_add_i64(
      total, static_cast<std::int64_t>(fourth), name);
  return checked_u64_from_i64(total, name);
}

std::uint64_t checked_runtime_workspace_total(const RuntimeRow& row) {
  const auto maximum =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  if (row.runtime_workspace_budget_bytes > maximum ||
      row.concurrent_sessions > maximum ||
      (row.runtime_workspace_budget_bytes != 0U &&
       static_cast<std::uint64_t>(row.concurrent_sessions) >
           maximum / row.runtime_workspace_budget_bytes)) {
    throw std::overflow_error(
        "edge runtime workspace session total exceeds signed byte range");
  }
  return row.runtime_workspace_budget_bytes *
         static_cast<std::uint64_t>(row.concurrent_sessions);
}

std::uint64_t checked_model_residency_total(const RuntimeRow& row) {
  if (!row.model_file_bytes.has_value()) {
    throw std::logic_error(
        "edge model residency requires an observed model file size");
  }
  if (row.mmap_strategy == "prefer_mmap_runtime_supported") {
    return *row.model_file_bytes;
  }

  const auto maximum =
      static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
  if (*row.model_file_bytes > maximum ||
      row.concurrent_sessions > maximum ||
      (*row.model_file_bytes != 0U &&
       static_cast<std::uint64_t>(row.concurrent_sessions) >
           maximum / *row.model_file_bytes)) {
    throw std::overflow_error(
        "edge model residency session total exceeds signed byte range");
  }
  return *row.model_file_bytes *
         static_cast<std::uint64_t>(row.concurrent_sessions);
}

void project_budget_delta(const std::uint64_t required,
                          const std::uint64_t budget,
                          std::optional<std::uint64_t>* headroom,
                          std::optional<std::uint64_t>* shortfall) {
  if (required <= budget) {
    *headroom = budget - required;
    shortfall->reset();
  } else {
    headroom->reset();
    *shortfall = required - budget;
  }
}

void validate_budget_delta(const std::uint64_t required,
                           const std::uint64_t budget, const bool fit,
                           const std::optional<std::uint64_t>& headroom,
                           const std::optional<std::uint64_t>& shortfall,
                           const char* name) {
  if (fit != (required <= budget)) {
    throw std::logic_error(std::string(name) + " fit decision drift");
  }
  if (fit) {
    if (!headroom.has_value() || shortfall.has_value() ||
        *headroom != budget - required) {
      throw std::logic_error(std::string(name) + " headroom drift");
    }
  } else if (headroom.has_value() || !shortfall.has_value() ||
             *shortfall != required - budget) {
    throw std::logic_error(std::string(name) + " shortfall drift");
  }
}

bool supports_mmap(const RuntimeRow& row) {
  const auto* registration = find_edge_runtime_registration(row.runtime_id);
  return registration != nullptr &&
         registration->declared_capabilities.supports_mmap;
}

struct ThreadRecommendation {
  std::size_t count = 0U;
  std::string status;
};

ThreadRecommendation derive_thread_recommendation(const RuntimeRow& row) {
  if (row.concurrent_sessions == 0U) {
    throw std::invalid_argument(
        "edge thread recommendation requires at least one session");
  }
  const auto* registration = find_edge_runtime_registration(row.runtime_id);
  const bool explicit_thread_control =
      registration != nullptr && registration->declared_capabilities
                                     .supports_explicit_thread_count;
  if (!explicit_thread_control) {
    return {row.thread_count, "runtime_managed_target_sweep_required"};
  }
  if (row.target_physical_core_count == 0U) {
    return {row.thread_count,
            "configured_baseline_target_core_count_unavailable"};
  }

  const auto per_session_core_budget = std::max<std::size_t>(
      1U, row.target_physical_core_count / row.concurrent_sessions);
  if (row.concurrent_sessions > row.target_physical_core_count) {
    return {
        std::min(row.thread_count, per_session_core_budget),
        "minimum_one_thread_concurrent_sessions_exceed_core_budget"};
  }
  return {
      std::min(row.thread_count, per_session_core_budget),
      row.thread_count <= per_session_core_budget
          ? "configured_count_within_per_session_core_budget"
          : "capped_to_per_session_physical_core_budget"};
}

std::string derive_mmap_strategy(const RuntimeRow& row) {
  if (!row.mmap_requested) return "buffered_load_requested";
  return supports_mmap(row) ? "prefer_mmap_runtime_supported"
                            : "mmap_requested_runtime_unsupported";
}

const std::optional<double>& configured_workload_tpot(
    const RuntimeRow& row) noexcept {
  return row.concurrent_sessions > 1U
             ? row.concurrent_execution.median_tpot_ms
             : row.metrics.tpot_ms;
}

const std::optional<double>& configured_workload_throughput(
    const RuntimeRow& row) noexcept {
  return row.concurrent_sessions > 1U
             ? row.concurrent_execution
                   .aggregate_generated_tokens_per_second
             : row.metrics.decode_tokens_per_second;
}

bool eligible_execute_candidate(const RuntimeRow& row) {
  return row.target_runtime_contract_supported && row.executed &&
         row.blockers.empty() &&
         configured_workload_tpot(row).has_value() &&
         configured_workload_throughput(row).has_value() &&
         row.observed_resource_fit.value_or(false);
}

std::vector<std::string> estimated_candidate_row_ids(const Matrix& matrix) {
  std::vector<std::string> candidate_ids;
  for (const auto& row : matrix.runtime_rows) {
    if (row.target_runtime_contract_supported &&
        row.estimated_resource_fit.value_or(false)) {
      candidate_ids.push_back(row.row_id);
    }
  }
  return candidate_ids;
}

struct PerformanceSelection {
  const RuntimeRow* row = nullptr;
  std::size_t eligible_count = 0U;
};

PerformanceSelection performance_selection(const Matrix& matrix) {
  PerformanceSelection selection;
  for (const auto& row : matrix.runtime_rows) {
    if (!eligible_execute_candidate(row)) continue;
    ++selection.eligible_count;
    const auto& row_tpot = configured_workload_tpot(row);
    const auto& row_throughput = configured_workload_throughput(row);
    const auto& selected_tpot =
        selection.row == nullptr
            ? row_tpot
            : configured_workload_tpot(*selection.row);
    const auto& selected_throughput =
        selection.row == nullptr
            ? row_throughput
            : configured_workload_throughput(*selection.row);
    if (selection.row == nullptr ||
        *row_tpot < *selected_tpot ||
        (*row_tpot == *selected_tpot &&
         *row_throughput > *selected_throughput)) {
      selection.row = &row;
    }
  }
  return selection;
}

void derive_observation_alignment(Matrix* matrix) {
  auto& recommendation = matrix->deployment_recommendation;
  recommendation.observation_alignment_gaps.clear();
  if (matrix->runtime_rows.empty()) {
    recommendation.observation_alignment_status =
        "unavailable_no_runtime_rows";
    return;
  }
  if (matrix->mode != Mode::kExecute) {
    recommendation.observation_alignment_status = "not_observed_plan";
    recommendation.observation_alignment_gaps.push_back(
        "target_execute_required");
    return;
  }

  for (const auto& row : matrix->runtime_rows) {
    if (!row.executed) {
      append_unique(&recommendation.observation_alignment_gaps,
                    row.row_id + ":not_executed");
      continue;
    }
    if (!configured_workload_tpot(row).has_value()) {
      append_unique(&recommendation.observation_alignment_gaps,
                    row.row_id + ":configured_workload_tpot_missing");
    }
    if (!configured_workload_throughput(row).has_value()) {
      append_unique(&recommendation.observation_alignment_gaps,
                    row.row_id + ":configured_workload_throughput_missing");
    }
    if (!row.observed_resource_fit.has_value()) {
      append_unique(&recommendation.observation_alignment_gaps,
                    row.row_id + ":observed_memory_fit_missing");
    } else if (!*row.observed_resource_fit) {
      append_unique(&recommendation.observation_alignment_gaps,
                    row.row_id + ":observed_memory_budget_exceeded");
    }
  }

  if (matrix->runtime_rows.size() > 1U) {
    std::string reference_thread_control;
    bool runtime_managed_thread_control = false;
    for (const auto& row : matrix->runtime_rows) {
      if (!row.executed) continue;
      if (row.thread_control_status ==
          "runtime_managed_not_explicitly_controlled") {
        runtime_managed_thread_control = true;
      }
      if (reference_thread_control.empty()) {
        reference_thread_control = row.thread_control_status;
      } else if (row.thread_control_status != reference_thread_control) {
        append_unique(&recommendation.observation_alignment_gaps,
                      "thread_control_mode_mismatch");
      }
    }
    if (runtime_managed_thread_control) {
      append_unique(&recommendation.observation_alignment_gaps,
                    "runtime_managed_thread_count_unverified");
    }
  }

  const auto& reference = matrix->runtime_rows.front();
  if (reference.executed) {
    for (std::size_t index = 1U; index < matrix->runtime_rows.size(); ++index) {
      const auto& candidate = matrix->runtime_rows[index];
      if (!candidate.executed) continue;
      if (candidate.observed_prompt_tokens !=
          reference.observed_prompt_tokens) {
        append_unique(&recommendation.observation_alignment_gaps,
                      "observed_prompt_tokens_mismatch");
      }
      if (candidate.observed_generated_tokens !=
          reference.observed_generated_tokens) {
        append_unique(&recommendation.observation_alignment_gaps,
                      "observed_generated_tokens_mismatch");
      }
      if (candidate.finish_reason != reference.finish_reason) {
        append_unique(&recommendation.observation_alignment_gaps,
                      "finish_reason_mismatch");
      }
    }
  }
  recommendation.observation_alignment_status =
      recommendation.observation_alignment_gaps.empty()
          ? "aligned_for_configured_workload_performance_ranking"
          : "incomplete_or_mismatched";
}

}  // namespace

void prepare_edge_resource_plan(
    const std::optional<std::uint64_t>& model_file_bytes, RuntimeRow* row) {
  if (row == nullptr) {
    throw std::invalid_argument("edge resource row must not be null");
  }
  row->model_file_bytes.reset();
  row->estimated_model_residency_total_bytes.reset();
  row->estimated_kv_cache_bytes.reset();
  row->estimated_runtime_workspace_total_bytes.reset();
  row->estimated_required_bytes.reset();
  row->estimated_resource_fit.reset();
  row->estimated_headroom_bytes.reset();
  row->estimated_shortfall_bytes.reset();
  row->resource_plan_status.clear();
  const auto thread_recommendation = derive_thread_recommendation(*row);
  row->recommended_thread_count = thread_recommendation.count;
  row->thread_recommendation_status = thread_recommendation.status;
  row->mmap_strategy = derive_mmap_strategy(*row);

  row->model_file_bytes = model_file_bytes;

  row->estimated_kv_cache_bytes = estimate_edge_kv_bytes(*row);
  row->estimated_runtime_workspace_total_bytes =
      checked_runtime_workspace_total(*row);

  if (!row->model_file_bytes.has_value()) {
    row->resource_plan_status = "model_size_unavailable";
    return;
  }
  if (*row->model_file_bytes == 0U) {
    row->resource_plan_status = "model_file_empty_or_unreadable";
    return;
  }
  row->estimated_model_residency_total_bytes =
      checked_model_residency_total(*row);
  row->estimated_required_bytes = checked_resource_total(
      *row->estimated_model_residency_total_bytes,
      *row->estimated_kv_cache_bytes,
      *row->estimated_runtime_workspace_total_bytes,
      row->system_reserve_bytes,
      "edge estimated required bytes");
  row->estimated_resource_fit =
      *row->estimated_required_bytes <= row->system_memory_budget_bytes;
  project_budget_delta(*row->estimated_required_bytes,
                       row->system_memory_budget_bytes,
                       &row->estimated_headroom_bytes,
                       &row->estimated_shortfall_bytes);
  row->resource_plan_status = *row->estimated_resource_fit
                                  ? "estimated_fit"
                                  : "estimated_budget_exceeded";
}

void finalize_edge_resource_observation(RuntimeRow* row) {
  if (row == nullptr) {
    throw std::invalid_argument("edge resource observation row is null");
  }
  row->observed_working_set_bytes.reset();
  row->observed_working_set_source.clear();
  row->observed_resource_fit.reset();
  row->observed_headroom_bytes.reset();
  row->observed_shortfall_bytes.reset();
  row->observed_resource_status.clear();
  if (!row->executed) {
    row->observed_resource_status = "not_observed_not_executed";
    return;
  }

  if (row->metrics.peak_pss_bytes.has_value()) {
    row->observed_working_set_bytes = row->metrics.peak_pss_bytes;
    row->observed_working_set_source = "peak_pss_checkpoint_max";
  } else if (row->metrics.peak_rss_bytes.has_value()) {
    row->observed_working_set_bytes = row->metrics.peak_rss_bytes;
    row->observed_working_set_source = "process_lifetime_peak_rss";
  } else {
    row->observed_resource_status = "unavailable_working_set_metric_missing";
    return;
  }

  const auto required = checked_resource_total(
      *row->observed_working_set_bytes, row->system_reserve_bytes, 0U, 0U,
      "edge observed working set plus reserve");
  row->observed_resource_fit = required <= row->system_memory_budget_bytes;
  project_budget_delta(required, row->system_memory_budget_bytes,
                       &row->observed_headroom_bytes,
                       &row->observed_shortfall_bytes);
  row->observed_resource_status = *row->observed_resource_fit
                                      ? "observed_fit"
                                      : "observed_budget_exceeded";
}

void validate_edge_resource_row(const RuntimeRow& row) {
  const auto expected_thread_recommendation =
      derive_thread_recommendation(row);
  if (row.recommended_thread_count != expected_thread_recommendation.count ||
      row.thread_recommendation_status !=
          expected_thread_recommendation.status) {
    throw std::logic_error("edge thread recommendation drift");
  }
  if (row.mmap_strategy != derive_mmap_strategy(row)) {
    throw std::logic_error("edge mmap strategy drift");
  }
  if (!row.estimated_kv_cache_bytes.has_value()) {
    throw std::logic_error("edge resource row has no KV estimate");
  }
  if (*row.estimated_kv_cache_bytes != estimate_edge_kv_bytes(row)) {
    throw std::logic_error("edge resource row KV estimate drift");
  }
  if (!row.estimated_runtime_workspace_total_bytes.has_value() ||
      *row.estimated_runtime_workspace_total_bytes !=
          checked_runtime_workspace_total(row)) {
    throw std::logic_error("edge runtime workspace session total drift");
  }
  if (!row.model_file_bytes.has_value()) {
    if (row.resource_plan_status != "model_size_unavailable" ||
        row.estimated_model_residency_total_bytes.has_value() ||
        row.estimated_required_bytes.has_value() ||
        row.estimated_resource_fit.has_value() ||
        row.estimated_headroom_bytes.has_value() ||
        row.estimated_shortfall_bytes.has_value()) {
      throw std::logic_error(
          "edge resource row carries an estimate without model size");
    }
  } else if (*row.model_file_bytes == 0U) {
    if (row.resource_plan_status != "model_file_empty_or_unreadable" ||
        row.estimated_model_residency_total_bytes.has_value() ||
        row.estimated_required_bytes.has_value() ||
        row.estimated_resource_fit.has_value() ||
        row.estimated_headroom_bytes.has_value() ||
        row.estimated_shortfall_bytes.has_value()) {
      throw std::logic_error(
          "empty or unreadable edge model file carries a resource-fit estimate");
    }
  } else {
    if (!row.estimated_model_residency_total_bytes.has_value() ||
        *row.estimated_model_residency_total_bytes !=
            checked_model_residency_total(row) ||
        !row.estimated_required_bytes.has_value() ||
        !row.estimated_resource_fit.has_value()) {
      throw std::logic_error("edge resource estimate is incomplete");
    }
    const auto expected_required = checked_resource_total(
        *row.estimated_model_residency_total_bytes,
        *row.estimated_kv_cache_bytes,
        *row.estimated_runtime_workspace_total_bytes,
        row.system_reserve_bytes,
        "edge estimated required bytes validation");
    if (*row.estimated_required_bytes != expected_required) {
      throw std::logic_error("edge estimated required bytes drift");
    }
    validate_budget_delta(expected_required, row.system_memory_budget_bytes,
                          *row.estimated_resource_fit,
                          row.estimated_headroom_bytes,
                          row.estimated_shortfall_bytes,
                          "edge estimated resource");
    const std::string expected_status =
        *row.estimated_resource_fit ? "estimated_fit"
                                    : "estimated_budget_exceeded";
    if (row.resource_plan_status != expected_status) {
      throw std::logic_error("edge estimated resource status drift");
    }
  }

  if (!row.executed) {
    if (row.observed_resource_status != "not_observed_not_executed" ||
        row.observed_working_set_bytes.has_value() ||
        !row.observed_working_set_source.empty() ||
        row.observed_resource_fit.has_value() ||
        row.observed_headroom_bytes.has_value() ||
        row.observed_shortfall_bytes.has_value()) {
      throw std::logic_error(
          "non-executed edge row carries observed resource evidence");
    }
    return;
  }

  if (!row.observed_working_set_bytes.has_value()) {
    if (row.observed_resource_status !=
            "unavailable_working_set_metric_missing" ||
        !row.observed_working_set_source.empty() ||
        row.observed_resource_fit.has_value() ||
        row.observed_headroom_bytes.has_value() ||
        row.observed_shortfall_bytes.has_value()) {
      throw std::logic_error(
          "edge row without working set carries observed resource result");
    }
    return;
  }

  const bool pss_projection =
      row.observed_working_set_source == "peak_pss_checkpoint_max" &&
      row.metrics.peak_pss_bytes == row.observed_working_set_bytes;
  const bool rss_projection =
      row.observed_working_set_source == "process_lifetime_peak_rss" &&
      !row.metrics.peak_pss_bytes.has_value() &&
      row.metrics.peak_rss_bytes == row.observed_working_set_bytes;
  if ((!pss_projection && !rss_projection) ||
      !row.observed_resource_fit.has_value()) {
    throw std::logic_error("edge observed working set projection drift");
  }
  const auto observed_required = checked_resource_total(
      *row.observed_working_set_bytes, row.system_reserve_bytes, 0U, 0U,
      "edge observed working set validation");
  validate_budget_delta(observed_required, row.system_memory_budget_bytes,
                        *row.observed_resource_fit,
                        row.observed_headroom_bytes,
                        row.observed_shortfall_bytes,
                        "edge observed resource");
  const std::string expected_status =
      *row.observed_resource_fit ? "observed_fit"
                                 : "observed_budget_exceeded";
  if (row.observed_resource_status != expected_status) {
    throw std::logic_error("edge observed resource status drift");
  }
}

void derive_edge_deployment_recommendation(Matrix* matrix) {
  if (matrix == nullptr) {
    throw std::invalid_argument("edge deployment matrix must not be null");
  }
  auto& recommendation = matrix->deployment_recommendation;
  recommendation = {};
  recommendation.candidate_row_ids = estimated_candidate_row_ids(*matrix);
  if (matrix->runtime_rows.empty()) {
    derive_observation_alignment(matrix);
    recommendation.status = "unavailable_no_runtime_rows";
    recommendation.criterion = "none";
    return;
  }
  derive_observation_alignment(matrix);
  if (matrix->mode != Mode::kExecute) {
    recommendation.status = recommendation.candidate_row_ids.empty()
                                ? "no_estimated_resource_fit_candidate"
                                : "target_execute_comparison_required";
    recommendation.criterion =
        "no runtime preference before target execute evidence";
    return;
  }
  if (matrix->comparison_identity_status !=
      "logical_identity_aligned_quality_comparison_allowed") {
    recommendation.status = "identity_alignment_required";
    recommendation.criterion =
        "cross-runtime identity must align before ranking";
    return;
  }
  if (recommendation.observation_alignment_status !=
      "aligned_for_configured_workload_performance_ranking") {
    recommendation.status = "complete_execute_observations_required";
    recommendation.criterion =
        "all runtime rows must align on observed token counts and finish reason";
    return;
  }

  const auto selection = performance_selection(*matrix);
  if (selection.row == nullptr ||
      selection.eligible_count != matrix->runtime_rows.size()) {
    recommendation.status = "complete_execute_metrics_required";
    recommendation.criterion =
        "requires successful lifecycle, observed memory fit, configured-workload TPOT and configured-workload generated-token throughput";
    return;
  }
  recommendation.status = "target_execute_performance_candidate";
  recommendation.selected_row_id = selection.row->row_id;
  recommendation.runtime_id = selection.row->runtime_id;
  recommendation.backend = selection.row->backend;
  recommendation.model_format = selection.row->model_format;
  recommendation.thread_count_status =
      selection.row->thread_recommendation_status;
  recommendation.thread_count =
      recommendation.thread_count_status ==
              "runtime_managed_target_sweep_required"
          ? 0U
          : selection.row->recommended_thread_count;
  recommendation.mmap_strategy = selection.row->mmap_strategy;
  recommendation.criterion =
      "lowest configured-workload observed TPOT, then highest configured-workload observed generated-token throughput";
}

void validate_edge_deployment_recommendation(const Matrix& matrix) {
  const auto& recommendation = matrix.deployment_recommendation;
  Matrix expected_matrix = matrix;
  derive_edge_deployment_recommendation(&expected_matrix);
  const auto& expected = expected_matrix.deployment_recommendation;
  if (recommendation.status != expected.status ||
      recommendation.candidate_row_ids != expected.candidate_row_ids ||
      recommendation.observation_alignment_status !=
          expected.observation_alignment_status ||
      recommendation.observation_alignment_gaps !=
          expected.observation_alignment_gaps ||
      recommendation.selected_row_id != expected.selected_row_id ||
      recommendation.runtime_id != expected.runtime_id ||
      recommendation.backend != expected.backend ||
      recommendation.model_format != expected.model_format ||
      recommendation.thread_count != expected.thread_count ||
      recommendation.thread_count_status != expected.thread_count_status ||
      recommendation.mmap_strategy != expected.mmap_strategy ||
      recommendation.criterion != expected.criterion) {
    throw std::logic_error("edge deployment recommendation derivation drift");
  }
  if (recommendation.status.empty() || recommendation.criterion.empty()) {
    throw std::logic_error("edge deployment recommendation is incomplete");
  }
  if (recommendation.observation_alignment_status.empty()) {
    throw std::logic_error(
        "edge deployment observation alignment status is empty");
  }
  const bool observations_aligned =
      recommendation.observation_alignment_status ==
      "aligned_for_configured_workload_performance_ranking";
  if (observations_aligned &&
      !recommendation.observation_alignment_gaps.empty()) {
    throw std::logic_error(
        "edge deployment observation alignment gaps drift");
  }
  if (!observations_aligned &&
      recommendation.observation_alignment_status !=
          "unavailable_no_runtime_rows" &&
      recommendation.observation_alignment_gaps.empty()) {
    throw std::logic_error(
        "edge deployment non-aligned observation has no reason");
  }
  if (recommendation.candidate_row_ids !=
      estimated_candidate_row_ids(matrix)) {
    throw std::logic_error("edge deployment estimated candidate set drift");
  }
  for (const auto& candidate_id : recommendation.candidate_row_ids) {
    const auto candidate = std::find_if(
        matrix.runtime_rows.begin(), matrix.runtime_rows.end(),
        [&candidate_id](const RuntimeRow& row) {
          return row.row_id == candidate_id &&
                 row.target_runtime_contract_supported &&
                 row.estimated_resource_fit.value_or(false);
        });
    if (candidate == matrix.runtime_rows.end()) {
      throw std::logic_error(
          "edge deployment candidate does not reference a fitting row");
    }
  }
  const bool selected =
      recommendation.status == "target_execute_performance_candidate";
  if (selected) {
    if (matrix.mode != Mode::kExecute || !observations_aligned ||
        recommendation.selected_row_id.empty() ||
        recommendation.runtime_id.empty() || recommendation.backend.empty() ||
        recommendation.model_format.empty() ||
        recommendation.thread_count_status.empty() ||
        recommendation.mmap_strategy.empty()) {
      throw std::logic_error(
          "selected edge deployment recommendation is incomplete");
    }
    const auto row = std::find_if(
        matrix.runtime_rows.begin(), matrix.runtime_rows.end(),
        [&recommendation](const RuntimeRow& value) {
          return value.row_id == recommendation.selected_row_id;
        });
    const auto canonical_selection = performance_selection(matrix);
    if (canonical_selection.eligible_count != matrix.runtime_rows.size() ||
        canonical_selection.row == nullptr ||
        row == matrix.runtime_rows.end() ||
        !eligible_execute_candidate(*row) ||
        canonical_selection.row != &*row ||
        row->runtime_id != recommendation.runtime_id ||
        row->backend != recommendation.backend ||
        row->model_format != recommendation.model_format) {
      throw std::logic_error(
          "edge deployment recommendation selection drift");
    }
  } else if (!recommendation.selected_row_id.empty() ||
             !recommendation.runtime_id.empty() ||
             !recommendation.backend.empty() ||
             !recommendation.model_format.empty() ||
             recommendation.thread_count != 0U ||
             !recommendation.thread_count_status.empty() ||
             !recommendation.mmap_strategy.empty()) {
    throw std::logic_error(
        "unselected edge deployment recommendation carries a selection");
  }
}

}  // namespace inferlab::edge_runtime_matrix
