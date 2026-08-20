#include "edge_runtime_quality_comparison.hpp"

#include "edge_runtime_matrix_internal.hpp"
#include "edge_runtime_matrix_model.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace inferlab::edge_runtime_matrix {
namespace {

using matrix_detail::append_unique;

constexpr char kQualityCriterion[] =
    "exact_bounded_callback_text_bytes_for_one_deterministic_request";

bool is_fnv1a64_fingerprint(const std::string& value) {
  if (value.size() != 24U || value.rfind("fnv1a64:", 0U) != 0U) {
    return false;
  }
  return std::all_of(value.begin() + 8, value.end(), [](const char digit) {
    return (digit >= '0' && digit <= '9') ||
           (digit >= 'a' && digit <= 'f');
  });
}

bool capture_is_observed(const GeneratedTextQualityObservation& observation) {
  return observation.capture_status == "observed_bounded_callback_text";
}

bool capture_has_comparable_text(
    const GeneratedTextQualityObservation& observation,
    const RuntimeRow& row) {
  return capture_is_observed(observation) &&
         observation.generated_text_bytes.has_value() &&
         *observation.generated_text_bytes > 0U &&
         row.observed_generated_tokens > 0U &&
         !observation.generated_text_fingerprint.empty();
}

bool any_capture_lacks_comparable_text(const Matrix& matrix,
                                       const QualityComparison& comparison) {
  for (std::size_t index = 0U; index < comparison.observations.size();
       ++index) {
    if (!capture_has_comparable_text(comparison.observations[index],
                                     matrix.runtime_rows[index])) {
      return true;
    }
  }
  return false;
}

std::size_t first_mismatch_byte_offset(const std::string& reference,
                                       const std::string& candidate) {
  const auto mismatch = std::mismatch(reference.begin(), reference.end(),
                                      candidate.begin(), candidate.end());
  return static_cast<std::size_t>(mismatch.first - reference.begin());
}

void mark_all_unavailable(QualityComparison* comparison) {
  for (const auto& observation : comparison->observations) {
    append_unique(&comparison->unavailable_row_ids, observation.row_id);
  }
}

}  // namespace

void derive_quality_comparison(Matrix* matrix) {
  if (matrix == nullptr ||
      matrix->quality_comparison.observations.size() !=
          matrix->runtime_rows.size()) {
    throw std::logic_error(
        "edge quality observation count does not match runtime rows");
  }
  auto& comparison = matrix->quality_comparison;
  comparison.criterion = kQualityCriterion;
  comparison.reference_row_id.clear();
  comparison.matching_row_ids.clear();
  comparison.mismatching_row_ids.clear();
  comparison.unavailable_row_ids.clear();
  for (auto& observation : comparison.observations) {
    observation.exact_match_with_reference = false;
    observation.first_mismatch_byte_offset.reset();
  }

  if (matrix->runtime_rows.size() <= 1U) {
    comparison.status = "not_comparable_single_or_empty";
    mark_all_unavailable(&comparison);
  } else if (!matrix->comparison_identity_gaps.empty()) {
    comparison.status = "not_comparable_identity_gap";
    mark_all_unavailable(&comparison);
  } else if (matrix->mode != Mode::kExecute) {
    comparison.status = "not_observed_plan";
    mark_all_unavailable(&comparison);
  } else if (std::any_of(matrix->runtime_rows.begin(),
                         matrix->runtime_rows.end(),
                         [](const RuntimeRow& row) { return !row.executed; })) {
    comparison.status = "not_observed_incomplete_execute";
    mark_all_unavailable(&comparison);
  } else if (std::any_of(
                 comparison.observations.begin(),
                 comparison.observations.end(), [](const auto& observation) {
                   return !capture_is_observed(observation) ||
                          !observation.generated_text_bytes.has_value() ||
                          observation.generated_text_fingerprint.empty();
                 })) {
    comparison.status = "unavailable_generated_text_capture";
    mark_all_unavailable(&comparison);
  } else if (any_capture_lacks_comparable_text(*matrix, comparison)) {
    comparison.status = "not_comparable_empty_generated_text";
    mark_all_unavailable(&comparison);
  } else {
    auto& reference = comparison.observations.front();
    comparison.reference_row_id = reference.row_id;
    reference.exact_match_with_reference = true;
    for (std::size_t index = 1U; index < comparison.observations.size();
         ++index) {
      auto& candidate = comparison.observations[index];
      candidate.exact_match_with_reference =
          candidate.retained_text_for_comparison ==
          reference.retained_text_for_comparison;
      if (!candidate.exact_match_with_reference) {
        candidate.first_mismatch_byte_offset = first_mismatch_byte_offset(
            reference.retained_text_for_comparison,
            candidate.retained_text_for_comparison);
      }
      append_unique(candidate.exact_match_with_reference
                        ? &comparison.matching_row_ids
                        : &comparison.mismatching_row_ids,
                    candidate.row_id);
    }
    comparison.status = comparison.mismatching_row_ids.empty()
                            ? "observed_exact_text_match"
                            : "observed_text_mismatch";
  }
  for (auto& observation : comparison.observations) {
    std::string{}.swap(observation.retained_text_for_comparison);
  }
}

void validate_quality_comparison(const Matrix& matrix) {
  const auto& comparison = matrix.quality_comparison;
  const std::unordered_set<std::string> capture_statuses = {
      "not_observed_plan",
      "blocked_before_execute",
      "observed_bounded_callback_text",
      "unavailable_runtime_factory_missing",
      "unavailable_runtime_capability_drift",
      "unavailable_runtime_load_failed",
      "unavailable_runtime_adapter_exception",
      "unavailable_generated_text_size_overflow",
      "unavailable_generated_text_limit_exceeded",
      "unavailable_generated_text_matrix_budget_exceeded",
      "unavailable_runtime_input_stability",
      "unavailable_runtime_protocol_failed"};
  if (comparison.criterion != kQualityCriterion ||
      comparison.observations.size() != matrix.runtime_rows.size()) {
    throw std::logic_error("edge quality comparison contract drift");
  }

  bool all_captures_observed = true;
  bool all_captures_nonempty = true;
  std::size_t captured_text_bytes = 0U;
  for (std::size_t index = 0U; index < comparison.observations.size();
       ++index) {
    const auto& observation = comparison.observations[index];
    const auto& row = matrix.runtime_rows[index];
    const bool capture_observed = capture_is_observed(observation);
    if (observation.row_id != row.row_id ||
        capture_statuses.find(observation.capture_status) ==
            capture_statuses.end() ||
        !observation.retained_text_for_comparison.empty() ||
        (capture_observed &&
         (!row.executed || !observation.generated_text_bytes.has_value() ||
          *observation.generated_text_bytes >
              kMaximumGeneratedTextEvidenceBytes ||
          !is_fnv1a64_fingerprint(
              observation.generated_text_fingerprint))) ||
        (!capture_observed &&
         !observation.generated_text_fingerprint.empty())) {
      throw std::logic_error("edge quality observation projection drift");
    }
    if (capture_observed) {
      if (*observation.generated_text_bytes >
          kMaximumGeneratedTextMatrixEvidenceBytes - captured_text_bytes) {
        throw std::logic_error("edge quality matrix text budget drift");
      }
      captured_text_bytes += *observation.generated_text_bytes;
    }
    all_captures_observed = all_captures_observed && capture_observed;
    all_captures_nonempty =
        all_captures_nonempty && capture_observed &&
        observation.generated_text_bytes.has_value() &&
        *observation.generated_text_bytes > 0U &&
        row.observed_generated_tokens > 0U;
  }

  const bool all_rows_executed =
      std::all_of(matrix.runtime_rows.begin(), matrix.runtime_rows.end(),
                  [](const RuntimeRow& row) { return row.executed; });
  std::string expected_status;
  if (matrix.runtime_rows.size() <= 1U) {
    expected_status = "not_comparable_single_or_empty";
  } else if (!matrix.comparison_identity_gaps.empty()) {
    expected_status = "not_comparable_identity_gap";
  } else if (matrix.mode != Mode::kExecute) {
    expected_status = "not_observed_plan";
  } else if (!all_rows_executed) {
    expected_status = "not_observed_incomplete_execute";
  } else if (!all_captures_observed) {
    expected_status = "unavailable_generated_text_capture";
  } else if (!all_captures_nonempty) {
    expected_status = "not_comparable_empty_generated_text";
  } else {
    const bool mismatch = std::any_of(
        comparison.observations.begin() + 1,
        comparison.observations.end(), [](const auto& observation) {
          return !observation.exact_match_with_reference;
        });
    expected_status = mismatch ? "observed_text_mismatch"
                               : "observed_exact_text_match";
  }
  if (comparison.status != expected_status) {
    throw std::logic_error("edge quality comparison status drift");
  }

  const bool comparison_observed =
      expected_status == "observed_text_mismatch" ||
      expected_status == "observed_exact_text_match";
  std::vector<std::string> expected_matching;
  std::vector<std::string> expected_mismatching;
  std::vector<std::string> expected_unavailable;
  if (comparison_observed) {
    if (comparison.observations.empty() ||
        !comparison.observations.front().exact_match_with_reference ||
        comparison.observations.front()
            .first_mismatch_byte_offset.has_value()) {
      throw std::logic_error("edge quality reference projection drift");
    }
    const auto reference_bytes =
        *comparison.observations.front().generated_text_bytes;
    for (std::size_t index = 1U; index < comparison.observations.size();
         ++index) {
      const auto& observation = comparison.observations[index];
      const bool has_mismatch_offset =
          observation.first_mismatch_byte_offset.has_value();
      if (observation.exact_match_with_reference == has_mismatch_offset) {
        throw std::logic_error("edge quality mismatch offset drift");
      }
      if (has_mismatch_offset) {
        const auto candidate_bytes = *observation.generated_text_bytes;
        const auto common_extent = std::min(reference_bytes, candidate_bytes);
        if (*observation.first_mismatch_byte_offset > common_extent ||
            (reference_bytes == candidate_bytes &&
             *observation.first_mismatch_byte_offset >= common_extent)) {
          throw std::logic_error("edge quality mismatch offset out of range");
        }
      }
      (observation.exact_match_with_reference ? expected_matching
                                              : expected_mismatching)
          .push_back(observation.row_id);
    }
  } else {
    for (const auto& observation : comparison.observations) {
      if (observation.exact_match_with_reference ||
          observation.first_mismatch_byte_offset.has_value()) {
        throw std::logic_error(
            "unobserved edge quality row claims comparison detail");
      }
      expected_unavailable.push_back(observation.row_id);
    }
  }
  const std::string expected_reference =
      comparison_observed ? matrix.runtime_rows.front().row_id : "";
  if (comparison.reference_row_id != expected_reference ||
      comparison.matching_row_ids != expected_matching ||
      comparison.mismatching_row_ids != expected_mismatching ||
      comparison.unavailable_row_ids != expected_unavailable) {
    throw std::logic_error("edge quality comparison index drift");
  }
}

}  // namespace inferlab::edge_runtime_matrix
