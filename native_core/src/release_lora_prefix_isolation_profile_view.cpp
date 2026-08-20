#include "release_lora_prefix_isolation_profile_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace inferlab_release_lora_prefix_isolation {

std::string binding_json(const Binding& binding, const int indent) {
  validate_profile(binding.profile);
  if (binding.source_role.empty() ||
      binding.matched != (binding.match_count == 1U) ||
      binding.matched != !binding.matched_source_identity.empty() ||
      (binding.isolation_compatible && !binding.matched) ||
      (binding.matched && binding.isolation_status == "not_evaluated") ||
      (!binding.matched && binding.isolation_status != "not_evaluated") ||
      (binding.matched && binding.isolation_compatible &&
       !binding.issues.empty()) ||
      (binding.matched && !binding.isolation_compatible &&
       binding.issues.empty()) ||
      (!binding.matched && binding.issues.empty())) {
    throw std::invalid_argument(
        "release LoRA/prefix isolation view received invalid binding");
  }
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "release LoRA/prefix isolation JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"adapter_residency_limit\": "
      << binding.profile.adapter_residency_limit << ",\n";
  out << nested << "\"cross_adapter_prefix_sharing_allowed\": "
      << inferlab_plan::json_bool(
             binding.profile.cross_adapter_prefix_sharing_allowed)
      << ",\n";
  out << nested << "\"id\": "
      << inferlab_plan::json_string(binding.profile.id) << ",\n";
  out << nested << "\"isolation_compatible\": "
      << inferlab_plan::json_bool(binding.isolation_compatible) << ",\n";
  out << nested << "\"isolation_status\": "
      << inferlab_plan::json_string(binding.isolation_status) << ",\n";
  out << nested << "\"issues\": "
      << inferlab_plan::json_string_array(binding.issues, nested_indent)
      << ",\n";
  out << nested << "\"match_count\": " << binding.match_count << ",\n";
  out << nested << "\"matched\": "
      << inferlab_plan::json_bool(binding.matched) << ",\n";
  out << nested << "\"matched_source_identity\": "
      << inferlab_plan::json_string(binding.matched_source_identity) << ",\n";
  out << nested << "\"prefix_cache_key_scope\": "
      << inferlab_plan::json_string(binding.profile.prefix_cache_key_scope)
      << ",\n";
  out << nested << "\"source_role\": "
      << inferlab_plan::json_string(binding.source_role) << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "compiled adapter namespace and residency binding only; not "
             "runtime cache-key enforcement, tenant isolation proof, live "
             "adapter residency, product execution, or release evidence")
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_release_lora_prefix_isolation
