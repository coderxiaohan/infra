#include "attention_probe_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::attention_probe {
namespace {

std::string profile_json(const AttentionReadProfile& profile,
                         const int indent) {
  const auto pad = inferlab_plan::json_indent(indent);
  const auto nested = inferlab_plan::json_indent(
      internal::checked_add_int(indent, 2, "attention profile indent"));
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"causal_mask_sparsity\": " << std::fixed
      << std::setprecision(6) << profile.causal_mask_sparsity << ",\n";
  out << nested << "\"key_element_reads_for_scores\": "
      << profile.key_element_reads_for_scores << ",\n";
  out << nested << "\"kv_element_reads_dense\": "
      << profile.kv_element_reads_dense << ",\n";
  out << nested << "\"kv_element_reads_mask_aware\": "
      << profile.kv_element_reads_mask_aware << ",\n";
  out << nested << "\"masked_score_elements\": "
      << profile.masked_score_elements << ",\n";
  out << nested << "\"output_write_elements\": "
      << profile.output_write_elements << ",\n";
  out << nested << "\"probability_intermediate_bytes\": "
      << profile.probability_intermediate_bytes << ",\n";
  out << nested << "\"qk_multiply_add_ops\": "
      << profile.qk_multiply_add_ops << ",\n";
  out << nested << "\"query_element_reads_for_scores\": "
      << profile.query_element_reads_for_scores << ",\n";
  out << nested << "\"score_elements\": " << profile.score_elements << ",\n";
  out << nested << "\"score_intermediate_bytes\": "
      << profile.score_intermediate_bytes << ",\n";
  out << nested << "\"total_intermediate_bytes\": "
      << profile.total_intermediate_bytes << ",\n";
  out << nested << "\"unmasked_score_elements\": "
      << profile.unmasked_score_elements << ",\n";
  out << nested << "\"value_element_reads_dense\": "
      << profile.value_element_reads_dense << ",\n";
  out << nested << "\"value_element_reads_mask_aware\": "
      << profile.value_element_reads_mask_aware << ",\n";
  out << nested << "\"value_multiply_add_ops_dense\": "
      << profile.value_multiply_add_ops_dense << ",\n";
  out << nested << "\"value_multiply_add_ops_mask_aware\": "
      << profile.value_multiply_add_ops_mask_aware << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_attention_probe",
      "Executed deterministic scalar CPU attention reference and static "
      "read-profile calculation only; not a fused production kernel, GPU "
      "timing result, or product runtime benchmark.");
  out << "  \"attention\": {\n";
  out << "    \"head_dim\": " << plan.request.shape.head_dim << ",\n";
  out << "    \"key_length\": " << plan.request.shape.key_length << ",\n";
  out << "    \"num_heads\": " << plan.request.shape.num_heads << ",\n";
  out << "    \"output_elements\": " << plan.output.size() << ",\n";
  out << "    \"output_sum_checksum\": " << std::fixed
      << std::setprecision(6) << plan.output_sum_checksum << ",\n";
  out << "    \"profile\": {\n";
  out << "      \"causal_square\": {\n";
  out << "        \"key_length\": " << plan.causal_shape.key_length << ",\n";
  out << "        \"query_length\": "
      << plan.causal_shape.query_length << ",\n";
  out << "        \"read_profile\": "
      << profile_json(plan.causal_profile, 8) << "\n";
  out << "      },\n";
  out << "      \"non_causal\": {\n";
  out << "        \"read_profile\": "
      << profile_json(plan.non_causal_profile, 8) << "\n";
  out << "      }\n";
  out << "    },\n";
  out << "    \"query_length\": "
      << plan.request.shape.query_length << ",\n";
  out << "    \"score_elements\": "
      << plan.non_causal_profile.score_elements << ",\n";
  out << "    \"working_set_cap_elements\": "
      << kMaxWorkingElements << ",\n";
  out << "    \"working_set_elements\": "
      << plan.footprint.working_elements << "\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::attention_probe
