#pragma once

#include <initializer_list>
#include <string>
#include <vector>

namespace inferlab_catalog::required_metrics_internal {

void append_metric_names(std::vector<std::string>& metrics,
                         std::initializer_list<const char*> names);
void append_native_core_audit_catalog_metrics(
    std::vector<std::string>& metrics, const std::string& prefix);

void append_artifact_catalog_single_source_metrics(
    std::vector<std::string>& metrics);
void append_query_family_metrics(std::vector<std::string>& metrics);
void append_artifact_schema_registry_metrics(
    std::vector<std::string>& metrics);
void append_native_core_audit_register_metrics(
    std::vector<std::string>& metrics);

void append_release_evidence_pipeline_metrics(
    std::vector<std::string>& metrics);
void append_release_evidence_pipeline_query_metrics(
    std::vector<std::string>& metrics);
void append_evidence_query_result_metrics(std::vector<std::string>& metrics);
void append_evidence_query_result_extended_metrics(
    std::vector<std::string>& metrics);

}  // namespace inferlab_catalog::required_metrics_internal
