#include "advanced_kv_router_artifact_view.hpp"

#include "catalog_derived_views.hpp"
#include "checked_index.hpp"
#include "native_trace_context_view.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

namespace inferlab::advanced_kv_router_artifact_view {
namespace {

std::string int_array_json(const std::vector<int>& values,
                           const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "advanced KV router integer array JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << "[\n";
  for (std::size_t index = 0U; index < values.size(); ++index) {
    out << nested << values[index]
        << (index == values.size() - 1U ? "\n" : ",\n");
  }
  out << pad << "]";
  return out.str();
}

std::string workload_json(
    const advanced_kv_router_artifact::WorkloadRecord& record,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "advanced KV router workload JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"output_tokens\": " << record.output_tokens
      << ",\n";
  out << nested << "\"prefix_cache_eligible\": "
      << inferlab_plan::json_bool(record.prefix_cache_eligible) << ",\n";
  out << nested << "\"prefix_class\": "
      << inferlab_plan::json_string(record.prefix_class) << ",\n";
  out << nested << "\"prompt_tokens\": " << record.prompt_tokens
      << ",\n";
  out << nested << "\"request_ordinal\": "
      << record.request_ordinal << ",\n";
  out << nested << "\"total_tokens\": " << record.total_tokens << "\n";
  out << pad << "}";
  return out.str();
}

std::string policy_json(
    const advanced_kv_router_artifact::PolicyRecord& record,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "advanced KV router policy JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"cache_hits\": " << record.cache_hits << ",\n";
  out << nested << "\"final_worker_loads\": "
      << int_array_json(record.final_worker_loads, nested_indent) << ",\n";
  out << nested << "\"peak_worker_load\": "
      << record.peak_worker_load << ",\n";
  out << nested << "\"policy\": "
      << inferlab_plan::json_string(record.policy) << ",\n";
  out << nested << "\"prefix_hit_rate\": "
      << inferlab_plan::fixed6(record.prefix_hit_rate) << ",\n";
  out << nested << "\"recomputed_tokens\": "
      << record.recomputed_tokens << ",\n";
  out << nested << "\"request_count\": " << record.request_count
      << ",\n";
  out << nested << "\"reused_tokens\": " << record.reused_tokens
      << ",\n";
  out << nested << "\"worker_count\": " << record.worker_count
      << ",\n";
  out << nested << "\"worker_load_imbalance\": "
      << record.worker_load_imbalance << "\n";
  out << pad << "}";
  return out.str();
}

std::string decision_json(
    const std::string& policy,
    const advanced_kv_router_artifact::DecisionRecord& decision,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "advanced KV router decision JSON indent");
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"cache_hit\": "
      << inferlab_plan::json_bool(decision.cache_hit) << ",\n";
  out << nested << "\"policy\": "
      << inferlab_plan::json_string(policy) << ",\n";
  out << nested << "\"recomputed_tokens\": "
      << decision.recomputed_tokens << ",\n";
  out << nested << "\"request_ordinal\": "
      << decision.request_ordinal << ",\n";
  out << nested << "\"reused_tokens\": "
      << decision.reused_tokens << ",\n";
  out << nested << "\"trace_record_key\": "
      << inferlab_plan::json_string(decision.trace_record_key) << ",\n";
  out << nested << "\"worker_id\": " << decision.worker_id << ",\n";
  out << nested << "\"worker_load_after\": "
      << decision.worker_load_after << "\n";
  out << pad << "}";
  return out.str();
}

std::size_t decision_count(
    const advanced_kv_router_artifact::Plan& plan) {
  std::size_t count = 0U;
  for (const auto& record : plan.records) {
    count = inferlab::internal::checked_add_size(
        count, record.decisions.size(),
        "advanced KV router rendered decision count");
  }
  return count;
}

}  // namespace

std::string artifact_json(
    const advanced_kv_router_artifact::Plan& plan) {
  advanced_kv_router_artifact::validate_plan(plan);
  const auto& artifact_spec =
      inferlab_high_value::require_managed_artifact(kCatalogId);
  const std::string artifact_type =
      inferlab_catalog::registry_artifact_type(artifact_spec);
  const std::string artifact_path =
      inferlab_catalog::raw_artifact_path(artifact_spec.artifact);
  const std::string claim_scope =
      inferlab_catalog::registry_claim_boundary(artifact_spec);
  const auto& best = plan.records[plan.best_prefix_hit_record_index];
  const std::size_t total_decisions = decision_count(plan);

  std::ostringstream out;
  out << "{\n";
  out << "  \"schema_version\": 1,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(artifact_type) << ",\n";
  inferlab_plan::write_common_dry_run_context_metadata(
      out, artifact_type, claim_scope,
      {"vLLM", "SGLang", "NVIDIA Dynamo", "KServe/llm-d"},
      artifact_spec.target, artifact_path, artifact_spec.command);
  out << "  \"status\": \"ok\",\n";
  out << "  \"mode\": \"dry_run\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"execution_kind\": \"synthetic_simulation\",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(claim_scope) << ",\n";
  out << "  \"requires_real_backend\": true,\n";
  out << "  \"external_products\": "
      << inferlab_plan::json_string_array(
             {"vLLM", "SGLang", "NVIDIA Dynamo", "KServe/llm-d"}, 2)
      << ",\n";
  out << "  \"fixture_basis\": \"compiled deterministic request sequence\",\n";
  out << "  \"trace_timestamp_semantics\": "
      << inferlab_plan::json_string(
             "optional externally supplied routing-decision observation timestamps; the compiled simulation does not synthesize runtime time")
      << ",\n";
  out << "  \"privacy_contract\": {\n";
  out << "    \"raw_request_ids_emitted\": false,\n";
  out << "    \"raw_prefix_ids_emitted\": false,\n";
  out << "    \"request_identity\": \"stable request ordinal within the compiled fixture\",\n";
  out << "    \"prefix_identity\": \"artifact-local anonymous prefix class\"\n";
  out << "  },\n";
  out << "  \"not_implemented_by_inferlab\": "
      << inferlab_plan::json_string_array(
             {"production router", "KV connector",
              "distributed control plane"},
             2)
      << ",\n";
  out << "  \"canonical_owners\": {\n";
  out << "    \"routing_algorithm\": \"advanced_serving_routing.cpp\",\n";
  out << "    \"artifact_composition\": \"advanced_kv_router_artifact_model.hpp/.cpp\",\n";
  out << "    \"trace_contract\": \"native_trace_context.hpp/.cpp\",\n";
  out << "    \"artifact_projection\": \"advanced_kv_router_artifact_view.hpp/.cpp\",\n";
  out << "    \"cli_request_output\": \"advanced_kv_router_main.cpp\"\n";
  out << "  },\n";
  out << "  \"catalog_evidence_contract\": "
      << inferlab_plan::catalog_evidence_contract_json(
             "dry_run_ready", 1U, claim_scope, artifact_spec.command, 2)
      << ",\n";
  out << "  \"native_trace_context\": "
      << inferlab_native_trace::trace_context_projection_json(
             plan.trace_context, 2)
      << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"best_policy\": "
      << inferlab_plan::json_string(best.policy) << ",\n";
  out << "    \"best_policy_scope\": \"highest compiled prefix-hit rate with deterministic recompute/load tie-breakers\",\n";
  out << "    \"best_prefix_hit_rate\": "
      << inferlab_plan::fixed6(best.prefix_hit_rate) << ",\n";
  out << "    \"decision_count\": " << total_decisions << ",\n";
  out << "    \"policy_count\": " << plan.records.size() << ",\n";
  out << "    \"request_count\": " << plan.workload.size() << ",\n";
  out << "    \"scenario_count\": " << plan.records.size() << "\n";
  out << "  },\n";
  out << "  \"workload\": [\n";
  for (std::size_t index = 0U; index < plan.workload.size(); ++index) {
    out << workload_json(plan.workload[index], 4)
        << (index == plan.workload.size() - 1U ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"records\": [\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    out << policy_json(plan.records[index], 4)
        << (index == plan.records.size() - 1U ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"decisions\": [\n";
  std::size_t rendered = 0U;
  for (const auto& record : plan.records) {
    for (const auto& decision : record.decisions) {
      out << decision_json(record.policy, decision, 4);
      rendered = inferlab::internal::checked_add_size(
          rendered, 1U, "advanced KV router rendered decision ordinal");
      out << (rendered == total_decisions ? "\n" : ",\n");
    }
  }
  out << "  ],\n";
  out << "  \"blockers\": [\n";
  out << "    \"target_product_router_execution_evidence_missing\"\n";
  out << "  ],\n";
  out << "  \"not_run\": "
      << inferlab_plan::default_not_run_json(2) << ",\n";
  out << "  \"recommended_next_commands\": [\n";
  out << "    " << inferlab_plan::json_string(artifact_spec.command)
      << "\n";
  out << "  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::advanced_kv_router_artifact_view
