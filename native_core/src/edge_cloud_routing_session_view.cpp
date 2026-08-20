#include "edge_cloud_routing_session_view.hpp"

#include "plan_json_utils.hpp"

#include <sstream>

namespace inferlab::edge_cloud_routing {

std::string session_recovery_input_json(
    const EdgeCloudSessionRecoveryInput& input, const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"recovery_requested\": "
      << inferlab_plan::json_bool(input.recovery_requested) << ",\n";
  out << nested << "\"durable_transcript_available\": "
      << inferlab_plan::json_bool(input.durable_transcript_available) << ",\n";
  out << nested << "\"prompt_reconstructed_from_transcript\": "
      << inferlab_plan::json_bool(input.prompt_reconstructed_from_transcript)
      << ",\n";
  out << nested << "\"previous_response_complete\": "
      << inferlab_plan::json_bool(input.previous_response_complete) << ",\n";
  out << nested << "\"prior_delivered_chunks\": "
      << input.prior_delivered_chunks << ",\n";
  out << nested << "\"transcript_turn_count\": "
      << input.transcript_turn_count << ",\n";
  out << nested << "\"state_age_ms\": " << input.state_age_ms << ",\n";
  out << nested << "\"maximum_state_age_ms\": "
      << input.maximum_state_age_ms << ",\n";
  out << nested << "\"session_identity_emitted\": false,\n";
  out << nested << "\"previous_request_identity_emitted\": false,\n";
  out << nested << "\"transcript_revision_emitted\": false,\n";
  out << nested << "\"provenance\": "
      << inferlab_plan::json_string(input.provenance) << ",\n";
  out << nested
      << "\"semantic\": \"caller reconstructed the complete durable transcript into the current prompt; no transcript, partial output, KV cache or hidden state is emitted\"\n";
  out << pad << "}";
  return out.str();
}

std::string session_recovery_assessment_json(
    const EdgeCloudSessionRecoveryAssessment& assessment,
    const int indent) {
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(indent + 2);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"eligible\": "
      << inferlab_plan::json_bool(assessment.eligible) << ",\n";
  out << nested << "\"status\": "
      << inferlab_plan::json_string(assessment.status) << ",\n";
  out << nested << "\"strategy\": "
      << inferlab_plan::json_string(assessment.strategy) << ",\n";
  out << nested << "\"state_age_ms\": "
      << (assessment.state_age_ms.has_value()
              ? std::to_string(*assessment.state_age_ms)
              : "null")
      << ",\n";
  out << nested << "\"blockers\": "
      << inferlab_plan::json_string_array(assessment.blockers, indent + 2)
      << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab::edge_cloud_routing
