#include "decoder_kv_probe_view.hpp"

#include "plan_json_utils.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::decoder_kv_probe {

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  const auto& output = plan.trace;
  const auto& profile = plan.cache_profile;
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_decoder_kv_probe",
      "Executed deterministic scalar CPU prefill/decode KV reference only; "
      "not a production paged-attention runtime, fused GPU kernel benchmark, "
      "or product serving performance result.");
  out << "  \"decoder_kv\": {\n";
  out << "    \"block_size_tokens\": "
      << plan.request.block_size_tokens << ",\n";
  out << "    \"cache_token_capacity\": "
      << plan.cache_token_capacity << ",\n";
  out << "    \"cache_profile\": {\n";
  out << "      \"average_attention_kv_read_elements_per_decode_step\": "
      << std::fixed << std::setprecision(6)
      << profile.average_attention_kv_read_elements_per_decode_step << ",\n";
  out << "      \"block_table_lookup_bytes\": "
      << profile.block_table_lookup_bytes << ",\n";
  out << "      \"block_table_lookup_count\": "
      << profile.block_table_lookup_count << ",\n";
  out << "      \"current_impl_physical_materialize_read_elements\": "
      << profile.current_impl_physical_materialize_read_elements << ",\n";
  out << "      \"current_impl_physical_materialize_write_elements\": "
      << profile.current_impl_physical_materialize_write_elements << ",\n";
  out << "      \"decode_attention_kv_read_elements\": "
      << profile.decode_attention_kv_read_elements << ",\n";
  out << "      \"decode_attention_score_elements\": "
      << profile.decode_attention_score_elements << ",\n";
  out << "      \"decode_kv_append_write_elements\": "
      << profile.decode_kv_append_write_elements << ",\n";
  out << "      \"final_kv_elements\": "
      << profile.final_kv_elements << ",\n";
  out << "      \"final_kv_utilization\": " << std::fixed
      << std::setprecision(6) << profile.final_kv_utilization << ",\n";
  out << "      \"final_logical_blocks\": "
      << profile.final_logical_blocks << ",\n";
  out << "      \"final_logical_kv_bytes\": "
      << profile.final_logical_kv_bytes << ",\n";
  out << "      \"final_reserved_kv_bytes\": "
      << profile.final_reserved_kv_bytes << ",\n";
  out << "      \"kv_elements_per_token\": "
      << profile.kv_elements_per_token << ",\n";
  out << "      \"paged_gather_read_elements\": "
      << profile.paged_gather_read_elements << ",\n";
  out << "      \"paged_gather_write_elements\": "
      << profile.paged_gather_write_elements << ",\n";
  out << "      \"prefill_kv_write_elements\": "
      << profile.prefill_kv_write_elements << "\n";
  out << "    },\n";
  out << "    \"decode_key_checksum\": " << std::fixed
      << std::setprecision(6) << output.decode_key_checksum << ",\n";
  out << "    \"decode_steps\": " << output.decode_steps << ",\n";
  out << "    \"final_tokens\": " << output.final_tokens << ",\n";
  out << "    \"fixture_element_cap\": "
      << probe::kMaxDecoderProbeFixtureElements << ",\n";
  out << "    \"fixture_input_elements\": "
      << plan.fixture.input_elements << ",\n";
  out << "    \"fixture_total_elements\": "
      << plan.fixture.total_elements << ",\n";
  out << "    \"fixture_weight_elements\": "
      << plan.fixture.weight_elements << ",\n";
  out << "    \"free_blocks\": " << output.free_blocks << ",\n";
  out << "    \"key_value_elements\": " << output.key_value_elements << ",\n";
  out << "    \"last_token_block_id\": "
      << output.last_token_block_id << ",\n";
  out << "    \"last_token_block_offset\": "
      << output.last_token_block_offset << ",\n";
  out << "    \"last_token_output_abs_sum\": " << std::fixed
      << std::setprecision(6) << plan.last_token_output_abs_sum << ",\n";
  out << "    \"last_token_output_checksum\": " << std::fixed
      << std::setprecision(6) << output.last_token_output_checksum << ",\n";
  out << "    \"last_token_output_elements\": "
      << output.last_token_output_elements << ",\n";
  out << "    \"prefill_key_checksum\": " << std::fixed
      << std::setprecision(6) << output.prefill_key_checksum << ",\n";
  out << "    \"prompt_tokens\": " << output.prompt_tokens << ",\n";
  out << "    \"used_block_table_gather\": "
      << inferlab_plan::json_bool(output.used_block_table_gather) << ",\n";
  out << "    \"used_blocks\": " << output.used_blocks << "\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::decoder_kv_probe
