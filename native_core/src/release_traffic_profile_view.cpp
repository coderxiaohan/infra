#include "release_traffic_profile_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <stdexcept>
#include <string>

namespace inferlab_release_traffic {

std::string binding_json(const Binding& binding, const int indent) {
  validate_profile(binding.profile);
  if (binding.source_role.empty() ||
      binding.matched != (binding.match_count == 1U) ||
      (binding.matched != !binding.matched_source_identity.empty()) ||
      (binding.matched && !binding.issues.empty()) ||
      (!binding.matched && binding.issues.empty())) {
    throw std::invalid_argument(
        "release traffic-profile binding view received invalid state");
  }
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "release traffic-profile JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"gpu_hour_cost\": "
      << inferlab_plan::fixed6(binding.profile.gpu_hour_cost) << ",\n";
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
  out << nested << "\"output_tokens\": "
      << binding.profile.output_tokens << ",\n";
  out << nested << "\"prompt_tokens\": "
      << binding.profile.prompt_tokens << ",\n";
  out << nested << "\"request_qps\": "
      << inferlab_plan::fixed6(binding.profile.request_qps) << ",\n";
  out << nested << "\"source_role\": "
      << inferlab_plan::json_string(binding.source_role) << ",\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(
             "compiled fixture profile binding only; not observed traffic, "
             "measured cost, autoscaling execution, or release evidence")
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_release_traffic
