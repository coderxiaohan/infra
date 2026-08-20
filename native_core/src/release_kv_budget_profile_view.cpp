#include "release_kv_budget_profile_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace inferlab_release_kv_budget {

std::string binding_json(const Binding& binding, const int indent) {
  validate_profile(binding.profile);
  if (binding.source_role.empty() ||
      binding.matched != (binding.match_count == 1U) ||
      (binding.matched != !binding.matched_source_identity.empty()) ||
      (binding.budget_compatible && !binding.matched) ||
      (binding.matched && binding.budget_status == "not_evaluated") ||
      (!binding.matched && binding.budget_status != "not_evaluated") ||
      (binding.matched && binding.budget_compatible &&
       !binding.issues.empty()) ||
      (binding.matched && !binding.budget_compatible &&
       binding.issues.empty()) ||
      (!binding.matched && binding.issues.empty())) {
    throw std::invalid_argument(
        "release KV-budget binding view received invalid state");
  }
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "release KV-budget JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"budget_compatible\": "
      << inferlab_plan::json_bool(binding.budget_compatible) << ",\n";
  out << nested << "\"budget_status\": "
      << inferlab_plan::json_string(binding.budget_status) << ",\n";
  out << nested << "\"gpu_memory_gib\": "
      << inferlab_plan::fixed6(binding.profile.gpu_memory_gib) << ",\n";
  out << nested << "\"id\": "
      << inferlab_plan::json_string(binding.profile.id) << ",\n";
  out << nested << "\"issues\": "
      << inferlab_plan::json_string_array(binding.issues, nested_indent)
      << ",\n";
  out << nested << "\"match_count\": " << binding.match_count << ",\n";
  out << nested << "\"matched\": "
      << inferlab_plan::json_bool(binding.matched) << ",\n";
  out << nested << "\"matched_source_identity\": "
      << inferlab_plan::json_string(binding.matched_source_identity) << ",\n";
  out << nested << "\"output_tokens\": " << binding.profile.output_tokens
      << ",\n";
  out << nested << "\"prefill_chunk_tokens\": "
      << binding.profile.prefill_chunk_tokens << ",\n";
  out << nested << "\"prompt_tokens\": " << binding.profile.prompt_tokens
      << ",\n";
  out << nested << "\"source_role\": "
      << inferlab_plan::json_string(binding.source_role) << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "compiled fixture KV-budget binding only; not measured GPU "
             "capacity, allocator execution, offload transfer, benchmark "
             "performance, or release evidence")
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_release_kv_budget
