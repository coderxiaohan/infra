#include "release_deployment_resource_profile_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace inferlab_release_deployment_resource {

std::string binding_json(const Binding& binding, const int indent) {
  validate_profile(binding.profile);
  if (binding.source_role.empty() ||
      binding.matched != (binding.match_count == 1U) ||
      binding.matched != !binding.matched_source_identity.empty() ||
      (binding.resource_compatible && !binding.matched) ||
      (binding.matched && binding.resource_status == "not_evaluated") ||
      (!binding.matched && binding.resource_status != "not_evaluated") ||
      (binding.matched && binding.resource_compatible &&
       !binding.issues.empty()) ||
      (binding.matched && !binding.resource_compatible &&
       binding.issues.empty()) ||
      (!binding.matched && binding.issues.empty())) {
    throw std::invalid_argument(
        "release deployment resource binding view received invalid state");
  }
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "release deployment resource JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"accelerator_memory_gib\": "
      << inferlab_plan::fixed6(binding.profile.accelerator_memory_gib)
      << ",\n";
  out << nested << "\"accelerators_per_node\": "
      << binding.profile.accelerators_per_node << ",\n";
  out << nested << "\"accelerators_per_replica\": "
      << binding.profile.accelerators_per_replica << ",\n";
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
  out << nested << "\"pipeline_parallel_size\": "
      << binding.profile.pipeline_parallel_size << ",\n";
  out << nested << "\"replica_count\": "
      << binding.profile.replica_count << ",\n";
  out << nested << "\"resource_compatible\": "
      << inferlab_plan::json_bool(binding.resource_compatible) << ",\n";
  out << nested << "\"resource_status\": "
      << inferlab_plan::json_string(binding.resource_status) << ",\n";
  out << nested << "\"source_role\": "
      << inferlab_plan::json_string(binding.source_role) << ",\n";
  out << nested << "\"tensor_parallel_size\": "
      << binding.profile.tensor_parallel_size << ",\n";
  out << nested << "\"total_accelerator_count\": "
      << binding.profile.total_accelerator_count << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "compiled deployment resource binding only; not Kubernetes "
             "admission, GPU allocation, collective execution, measured "
             "capacity, benchmark performance, or release evidence")
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_release_deployment_resource
