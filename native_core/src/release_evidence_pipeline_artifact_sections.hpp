#pragma once

#include "plan_json_utils.hpp"
#include "release_evidence_pipeline_artifact_projection.hpp"

#include <cstddef>
#include <ostream>
#include <string>

namespace inferlab_release_pipeline::detail {

inline void append_count_field(std::ostream& out, const std::string& key,
                               const std::size_t value) {
  out << "    " << inferlab_plan::json_string(key) << ": " << value
      << ",\n";
}

inline void append_bool_field(std::ostream& out, const std::string& key,
                              const bool value) {
  out << "    " << inferlab_plan::json_string(key) << ": "
      << inferlab_plan::json_bool(value) << ",\n";
}

inline void append_string_field(std::ostream& out, const std::string& key,
                                const std::string& value) {
  out << "    " << inferlab_plan::json_string(key) << ": "
      << inferlab_plan::json_string(value) << ",\n";
}

inline void append_repair_metric_fields(
    std::ostream& out, const std::string& prefix,
    const RepairPathSummaryMetrics& metrics) {
  append_count_field(out, prefix + "_repair_path_count", metrics.path_count);
  append_count_field(out, prefix + "_repair_path_source_location_count",
                     metrics.source_location_count);
  append_count_field(out, prefix + "_repair_path_source_location_gap_count",
                     metrics.source_location_gap_count);
  append_count_field(out, prefix + "_repair_path_evidence_counter_count",
                     metrics.evidence_counter_count);
}

void append_release_pipeline_header_and_summary_core(
    std::ostream& out,
    const ReleasePipelineArtifactProjection& projection);

void append_release_pipeline_summary_query_and_catalog(
    std::ostream& out,
    const ReleasePipelineArtifactProjection& projection);

void append_release_pipeline_summary_native_audit_and_close(
    std::ostream& out,
    const ReleasePipelineArtifactProjection& projection);

void append_release_pipeline_artifact_body(
    std::ostream& out,
    const ReleasePipelineArtifactProjection& projection);

}  // namespace inferlab_release_pipeline::detail
