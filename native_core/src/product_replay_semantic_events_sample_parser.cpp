#include "product_replay_semantic_events_sample_parser.hpp"

#include "artifact_field_projection.hpp"
#include "artifact_metadata_reader.hpp"
#include "checked_index.hpp"
#include "plan_json_utils.hpp"
#include "trace_correlation_contract.hpp"
#include "trace_span_lifecycle_contract.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab::product_replay_semantic_events {
namespace {

constexpr std::uintmax_t kMaximumSampleBytes = 4U * 1024U * 1024U;
constexpr std::size_t kMaximumSampleCount = 64U;
constexpr std::size_t kMaximumEventLineCount = 100000U;
constexpr std::size_t kMaximumEventLineBytes = 64U * 1024U;

void increment(std::size_t& value, const char* context) {
  value = inferlab::internal::checked_add_size(value, 1U, context);
}

void append_string(std::vector<std::string>& values,
                   const std::string& value) {
  inferlab_plan::append_unique_nonempty_string(values, value);
}

bool field_is_valid(const std::string& document,
                    const EventFieldSpec& field, bool& present) {
  present = inferlab_artifact::contains_top_level_key_fallback(
      document, field.name);
  if (!present) return false;
  switch (field.kind) {
    case EventFieldKind::string: {
      const auto value = inferlab_artifact::project_string_field(
          document, {field.name.c_str()});
      return value.present && !value.invalid &&
             inferlab_plan::has_non_whitespace(value.value);
    }
    case EventFieldKind::unsigned_integer: {
      const auto value = inferlab_artifact::project_unsigned_field(
          document, {field.name.c_str()});
      return value.present && !value.invalid;
    }
    case EventFieldKind::non_negative_number: {
      std::size_t start = std::string::npos;
      std::size_t end = std::string::npos;
      if (!inferlab_artifact::find_top_level_json_value_fallback(
              document, field.name, start, end) ||
          start >= end || end > document.size()) {
        return false;
      }
      const std::string value_text = document.substr(start, end - start);
      std::size_t consumed = 0U;
      try {
        const double value = std::stod(value_text, &consumed);
        return consumed == value_text.size() && std::isfinite(value) &&
               value >= 0.0;
      } catch (const std::exception&) {
        return false;
      }
    }
    case EventFieldKind::string_array: {
      const std::string section =
          inferlab_artifact::json_array_section_or_empty(document,
                                                          field.name);
      std::vector<std::string> values;
      if (section.empty() ||
          !inferlab_artifact::parse_string_array_items_fallback(
              section, values) ||
          values.empty() ||
          !std::all_of(values.begin(), values.end(),
                       [](const auto& value) {
                         return inferlab_plan::has_non_whitespace(value);
                       })) {
        return false;
      }
      return std::set<std::string>(values.begin(), values.end()).size() ==
             values.size();
    }
  }
  throw std::logic_error(
      "unknown product replay semantic event field kind");
}

void project_request_trace_fields(const std::string& line,
                                  NormalizedEventRow& row) {
  const auto request_id =
      inferlab_artifact::project_string_field(line, {"request_id"});
  row.request_id_present =
      request_id.present && !request_id.invalid &&
      inferlab_plan::has_non_whitespace(request_id.value);
  if (row.request_id_present) row.request_id = request_id.value;

  const std::string request_ids_section =
      inferlab_artifact::json_array_section_or_empty(line, "request_ids");
  if (!request_ids_section.empty()) {
    std::vector<std::string> values;
    if (inferlab_artifact::parse_string_array_items_fallback(
            request_ids_section, values) &&
        !values.empty() &&
        std::all_of(values.begin(), values.end(), [](const auto& value) {
          return inferlab_plan::has_non_whitespace(value);
        }) &&
        std::set<std::string>(values.begin(), values.end()).size() ==
            values.size()) {
      row.request_ids_present = true;
      row.request_ids = std::move(values);
    }
  }

  const bool trace_id_field_present =
      inferlab_artifact::contains_top_level_key_fallback(line, "trace_id");
  if (trace_id_field_present) {
    const auto trace_id =
        inferlab_artifact::project_string_field(line, {"trace_id"});
    if (trace_id.present && !trace_id.invalid &&
        inferlab_trace::valid_w3c_trace_id(trace_id.value)) {
      row.trace_id_present = true;
      row.trace_id = trace_id.value;
    } else {
      row.trace_context_valid = false;
      inferlab_plan::append_unique_nonempty_string(row.invalid_fields,
                                                   "trace_id");
      inferlab_plan::append_unique_nonempty_string(
          row.issues, "trace_id_must_be_nonzero_32_char_lowercase_hex");
    }
  }

  const bool trace_ids_field_present =
      inferlab_artifact::contains_top_level_key_fallback(line, "trace_ids");
  if (trace_ids_field_present) {
    const std::string trace_ids_section =
        inferlab_artifact::json_array_section_or_empty(line, "trace_ids");
    std::vector<std::string> values;
    if (!trace_ids_section.empty() &&
        inferlab_artifact::parse_string_array_items_fallback(
            trace_ids_section, values) &&
        !values.empty() &&
        std::all_of(values.begin(), values.end(), [](const auto& value) {
          return inferlab_trace::valid_w3c_trace_id(value);
        })) {
      row.trace_ids_present = true;
      row.trace_ids = std::move(values);
    } else {
      row.trace_context_valid = false;
      inferlab_plan::append_unique_nonempty_string(row.invalid_fields,
                                                   "trace_ids");
      inferlab_plan::append_unique_nonempty_string(
          row.issues,
          "trace_ids_must_be_nonempty_array_of_w3c_trace_ids");
    }
  }

  if (trace_id_field_present && trace_ids_field_present) {
    row.trace_context_valid = false;
    inferlab_plan::append_unique_nonempty_string(
        row.issues, "trace_id_and_trace_ids_are_mutually_exclusive");
  }
  if (row.trace_id_present && !row.request_id_present) {
    row.trace_context_valid = false;
    inferlab_plan::append_unique_nonempty_string(
        row.issues, "trace_id_requires_direct_request_id");
  }
  if (row.trace_ids_present &&
      (!row.request_ids_present ||
       row.trace_ids.size() != row.request_ids.size())) {
    row.trace_context_valid = false;
    inferlab_plan::append_unique_nonempty_string(
        row.issues, "trace_ids_must_align_one_to_one_with_request_ids");
  }

  const bool request_span_id_field_present =
      inferlab_artifact::contains_top_level_key_fallback(
          line, "request_span_id");
  if (request_span_id_field_present) {
    const auto span_id = inferlab_artifact::project_string_field(
        line, {"request_span_id"});
    if (span_id.present && !span_id.invalid &&
        inferlab_trace::valid_w3c_span_id(span_id.value)) {
      row.request_span_id_present = true;
      row.request_span_id = span_id.value;
    } else {
      row.request_span_context_valid = false;
      inferlab_plan::append_unique_nonempty_string(
          row.invalid_fields, "request_span_id");
      inferlab_plan::append_unique_nonempty_string(
          row.issues,
          "request_span_id_must_be_nonzero_16_char_lowercase_hex");
    }
  }

  const bool request_span_ids_field_present =
      inferlab_artifact::contains_top_level_key_fallback(
          line, "request_span_ids");
  if (request_span_ids_field_present) {
    const std::string section =
        inferlab_artifact::json_array_section_or_empty(
            line, "request_span_ids");
    std::vector<std::string> values;
    if (!section.empty() &&
        inferlab_artifact::parse_string_array_items_fallback(
            section, values) &&
        !values.empty() &&
        std::all_of(values.begin(), values.end(), [](const auto& value) {
          return inferlab_trace::valid_w3c_span_id(value);
        }) &&
        std::set<std::string>(values.begin(), values.end()).size() ==
            values.size()) {
      row.request_span_ids_present = true;
      row.request_span_ids = std::move(values);
    } else {
      row.request_span_context_valid = false;
      inferlab_plan::append_unique_nonempty_string(
          row.invalid_fields, "request_span_ids");
      inferlab_plan::append_unique_nonempty_string(
          row.issues,
          "request_span_ids_must_be_unique_nonempty_array_of_w3c_span_ids");
    }
  }
  if (request_span_id_field_present && request_span_ids_field_present) {
    row.request_span_context_valid = false;
    inferlab_plan::append_unique_nonempty_string(
        row.issues,
        "request_span_id_and_request_span_ids_are_mutually_exclusive");
  }
  if (row.request_span_id_present &&
      (!row.request_id_present || !row.trace_id_present)) {
    row.request_span_context_valid = false;
    inferlab_plan::append_unique_nonempty_string(
        row.issues,
        "request_span_id_requires_direct_request_id_and_trace_id");
  }
  if (row.request_span_ids_present &&
      (!row.request_ids_present || !row.trace_ids_present ||
       row.request_span_ids.size() != row.request_ids.size() ||
       row.request_span_ids.size() != row.trace_ids.size())) {
    row.request_span_context_valid = false;
    inferlab_plan::append_unique_nonempty_string(
        row.issues,
        "request_span_ids_must_align_with_request_ids_and_trace_ids");
  }

  const auto timestamp =
      inferlab_artifact::project_unsigned_field(line, {"timestamp_ms"});
  row.timestamp_value_present = timestamp.present && !timestamp.invalid;
  if (row.timestamp_value_present) row.timestamp_ms = timestamp.value;
}

void project_request_terminal_status(const std::string& line,
                                     NormalizedEventRow& row) {
  if (row.rule == nullptr || row.rule->event_type != "request_terminal") {
    return;
  }
  const auto status =
      inferlab_artifact::project_string_field(line, {"status"});
  if (status.present && !status.invalid &&
      inferlab_trace_lifecycle::valid_terminal_status(status.value)) {
    row.request_terminal_status_present = true;
    row.request_terminal_status = status.value;
    return;
  }
  row.required_fields_valid = false;
  inferlab_plan::append_unique_nonempty_string(row.invalid_fields,
                                               "status");
  inferlab_plan::append_unique_nonempty_string(
      row.issues, "request_terminal_status_must_be_canonical");
}

NormalizedEventRow parse_event_line(
    const std::string& line, const ReplaySampleRequest& request,
    const std::size_t source_index, const std::size_t line_number) {
  NormalizedEventRow row;
  row.source_index = source_index;
  row.line_number = line_number;
  row.json_valid = inferlab_artifact::json_document_is_valid_object(line);
  if (!row.json_valid) {
    row.issues.push_back("event_line_json_malformed_or_has_duplicate_keys");
    return row;
  }

  const auto event_type =
      inferlab_artifact::project_string_field(line, {"event_type"});
  if (!event_type.present || event_type.invalid ||
      !inferlab_plan::has_non_whitespace(event_type.value)) {
    row.issues.push_back("event_type_missing_or_wrong_type");
    return row;
  }
  row.rule = find_semantic_event_rule(event_type.value);
  row.event_type_known = row.rule != nullptr;
  if (!row.event_type_known) {
    row.issues.push_back("unsupported_event_type");
    return row;
  }

  if (request.origin == SampleInputOrigin::default_fixture) {
    row.product_profile_id = request.expected_product_profile_id;
    row.product_identity_valid =
        mature_product_profiles::find_product_profile(
            row.product_profile_id) != nullptr;
  } else {
    const auto product =
        inferlab_artifact::project_string_field(line, {"product"});
    if (product.present && !product.invalid &&
        inferlab_plan::has_non_whitespace(product.value) &&
        mature_product_profiles::find_product_profile(product.value) !=
            nullptr) {
      row.product_profile_id = product.value;
      row.product_identity_valid = true;
    }
  }
  if (!row.product_identity_valid) {
    row.issues.push_back(
        "canonical_product_profile_identity_missing_or_invalid");
  }

  for (const auto& field_name : row.rule->required_fields) {
    const auto& field = require_event_field(field_name);
    bool present = false;
    if (field_is_valid(line, field, present)) continue;
    if (present) {
      row.invalid_fields.push_back(field.name);
      row.issues.push_back("required_event_field_wrong_type:" + field.name);
    } else {
      row.missing_fields.push_back(field.name);
      row.issues.push_back("required_event_field_missing:" + field.name);
    }
  }
  row.required_fields_valid = row.missing_fields.empty() &&
                              row.invalid_fields.empty();

  project_request_trace_fields(line, row);
  project_request_terminal_status(line, row);

  bool timestamp_field_present = false;
  row.timestamp_present = field_is_valid(
      line, require_event_field("timestamp_ms"), timestamp_field_present);
  bool claim_field_present = false;
  row.claim_scope_present = field_is_valid(
      line, require_event_field("claim_scope"), claim_field_present);
  if (request.origin == SampleInputOrigin::explicit_replay) {
    if (!row.timestamp_present) {
      row.issues.push_back(
          timestamp_field_present ? "timestamp_ms_wrong_type"
                                  : "timestamp_ms_missing");
    }
    if (!row.claim_scope_present) {
      row.issues.push_back(
          claim_field_present ? "claim_scope_wrong_type_or_empty"
                              : "claim_scope_missing");
    }
  }

  const bool common_schema_valid =
      row.json_valid && row.event_type_known &&
      row.product_identity_valid && row.required_fields_valid &&
      row.trace_context_valid && row.request_span_context_valid;
  row.fixture_schema_valid =
      request.origin == SampleInputOrigin::default_fixture &&
      common_schema_valid;
  row.explicit_replay_valid =
      request.origin == SampleInputOrigin::explicit_replay &&
      common_schema_valid && row.timestamp_present &&
      row.claim_scope_present;
  return row;
}

}  // namespace

std::size_t maximum_replay_sample_count() noexcept {
  return kMaximumSampleCount;
}

SampleSourceState parse_replay_sample_source(
    const ReplaySampleRequest& request, const std::size_t source_index,
    std::vector<NormalizedEventRow>& events) {
  SampleSourceState source;
  source.request = request;
  source.path_probe_status =
      inferlab_artifact::path_probe_status(request.path);
  source.present = source.path_probe_status != "missing" &&
                   source.path_probe_status != "not_configured" &&
                   source.path_probe_status != "path_probe_error";
  source.regular_file = source.path_probe_status == "file";
  if (source.path_probe_status == "path_probe_error") {
    source.path_probe_error = "failed to inspect replay sample path";
    source.issues.push_back("sample_path_probe_failed");
  } else if (!source.present) {
    source.issues.push_back("sample_missing");
  } else if (!source.regular_file) {
    source.issues.push_back("sample_path_is_not_a_regular_file");
  }
  if (!source.regular_file) {
    source.read_issue = true;
    return source;
  }

  source.byte_size = inferlab_artifact::file_size_or_zero(request.path);
  source.size_allowed = source.byte_size <= kMaximumSampleBytes;
  if (!source.size_allowed) {
    source.issues.push_back("sample_exceeds_maximum_byte_size");
    source.read_issue = true;
    return source;
  }

  const auto read_result = inferlab_artifact::read_text_with_status(
      request.path, static_cast<std::size_t>(kMaximumSampleBytes));
  source.text_read_status = read_result.status;
  source.text_read_error = read_result.error;
  if (read_result.status == "too_large") {
    source.size_allowed = false;
    source.issues.push_back("sample_exceeds_maximum_byte_size");
    source.read_issue = true;
    return source;
  }
  source.readable = read_result.readable && !read_result.empty;
  if (!source.readable) {
    source.issues.push_back(read_result.empty ? "sample_is_empty"
                                              : "sample_is_unreadable");
    source.read_issue = true;
    return source;
  }

  std::istringstream lines(read_result.text);
  std::string line;
  std::size_t physical_line_number = 0U;
  while (std::getline(lines, line)) {
    increment(physical_line_number,
              "product replay sample physical line number");
    if (line.empty()) continue;
    if (source.line_count == kMaximumEventLineCount) {
      source.issues.push_back("sample_exceeds_maximum_event_line_count");
      source.read_issue = true;
      break;
    }
    increment(source.line_count,
              "product replay sample event line count");
    NormalizedEventRow event;
    if (line.size() > kMaximumEventLineBytes) {
      event.source_index = source_index;
      event.line_number = physical_line_number;
      event.issues.push_back("event_line_exceeds_maximum_byte_size");
    } else {
      event = parse_event_line(line, request, source_index,
                               physical_line_number);
    }
    const std::size_t event_index = events.size();
    events.push_back(std::move(event));
    source.event_indices.push_back(event_index);
    const auto& stored = events.back();
    if (!stored.event_type_known) {
      if (std::find(stored.issues.begin(), stored.issues.end(),
                    "unsupported_event_type") != stored.issues.end()) {
        increment(source.unsupported_event_count,
                  "product replay unsupported event count");
      } else {
        increment(source.invalid_event_count,
                  "product replay invalid event count");
        source.read_issue = true;
        append_string(source.issues,
                      "sample_contains_unparseable_event_rows");
      }
      continue;
    }
    append_string(source.event_types, stored.rule->event_type);
    if (stored.product_identity_valid) {
      append_string(source.product_profile_ids,
                    stored.product_profile_id);
    }
    if (stored.fixture_schema_valid) {
      increment(source.fixture_event_count,
                "product replay fixture event count");
      increment(source.valid_event_count,
                "product replay valid event count");
    } else if (stored.explicit_replay_valid) {
      increment(source.explicit_replay_event_count,
                "product replay explicit event count");
      increment(source.valid_event_count,
                "product replay valid event count");
    } else {
      increment(source.invalid_event_count,
                "product replay invalid event count");
    }
  }

  source.source_contract_ready =
      source.readable && source.valid_event_count > 0U &&
      source.invalid_event_count == 0U;
  if (source.line_count == 0U) {
    source.issues.push_back("sample_has_no_nonempty_event_lines");
    source.read_issue = true;
  }
  return source;
}

}  // namespace inferlab::product_replay_semantic_events
