#include "decoder_probe_view.hpp"

#include "plan_json_utils.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::decoder_probe {

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto& attention_only = plan.attention_only;
  const auto& result = plan.output;

  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_decoder_probe",
      "Executed deterministic scalar CPU decoder-block reference only; not "
      "a production fused decoder, GPU kernel benchmark, or product runtime "
      "performance result.");
  out << "  \"decoder_block\": {\n";
  out << "    \"attention_only_checksum\": " << std::fixed
      << std::setprecision(6) << attention_only.trace.output_checksum
      << ",\n";
  out << "    \"attention_score_elements\": "
      << result.trace.attention_score_elements << ",\n";
  out << "    \"ffn_checksum\": " << std::fixed
      << std::setprecision(6) << result.trace.ffn_checksum << ",\n";
  out << "    \"ffn_elements\": " << result.trace.ffn_elements << ",\n";
  out << "    \"ffn_intermediate_size\": "
      << result.trace.ffn_intermediate_size << ",\n";
  out << "    \"fixture_element_cap\": "
      << probe::kMaxDecoderProbeFixtureElements << ",\n";
  out << "    \"fixture_input_elements\": "
      << plan.fixture.input_elements << ",\n";
  out << "    \"fixture_total_elements\": "
      << plan.fixture.total_elements << ",\n";
  out << "    \"fixture_weight_elements\": "
      << plan.fixture.weight_elements << ",\n";
  out << "    \"head_dim\": " << result.trace.head_dim << ",\n";
  out << "    \"hidden_size\": " << result.trace.hidden_size << ",\n";
  out << "    \"input_elements\": " << result.trace.input_elements << ",\n";
  out << "    \"num_heads\": " << result.trace.num_heads << ",\n";
  out << "    \"output_abs_sum\": " << std::fixed
      << std::setprecision(6) << plan.output_abs_sum << ",\n";
  out << "    \"output_checksum\": " << std::fixed
      << std::setprecision(6) << result.trace.output_checksum << ",\n";
  out << "    \"output_elements\": " << result.trace.output_elements
      << ",\n";
  out << "    \"projected_elements\": "
      << result.trace.projected_elements << ",\n";
  out << "    \"seq_len\": " << result.trace.seq_len << "\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::decoder_probe
