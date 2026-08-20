#include "trace_clock_calibration_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <sstream>
#include <string>

namespace inferlab_trace_clock {

std::string clock_calibration_usage_json(
    const CalibrationUsageProjection& projection, const int indent) {
  validate_clock_calibration_usage(projection);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "trace clock calibration JSON indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  const auto& source = projection.source;
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"all_lookups_calibrated\": "
      << inferlab_plan::json_bool(projection.all_lookups_calibrated)
      << ",\n";
  out << nested << "\"calibrated_timestamp_count\": "
      << projection.calibrated_timestamp_count << ",\n";
  out << nested << "\"calibration_scope\": "
      << inferlab_plan::json_string(projection.calibration_scope) << ",\n";
  out << nested << "\"clock_anchor_values_emitted\": false,\n";
  out << nested << "\"contract_fingerprint\": "
      << inferlab_plan::json_string(source.contract_fingerprint) << ",\n";
  out << nested << "\"decision\": "
      << inferlab_plan::json_string(projection.decision) << ",\n";
  out << nested << "\"distinct_calibration_entry_count\": "
      << projection.distinct_calibration_entry_count << ",\n";
  out << nested << "\"entry_count\": " << source.entry_count << ",\n";
  out << nested << "\"fingerprint_algorithm\": "
      << inferlab_plan::json_string(projection.fingerprint_algorithm)
      << ",\n";
  out << nested << "\"fingerprint_is_cryptographic\": false,\n";
  out << nested << "\"lookup_count\": " << projection.lookup_count
      << ",\n";
  out << nested << "\"normalized_timestamp_unit\": "
      << inferlab_plan::json_string(projection.normalized_timestamp_unit)
      << ",\n";
  out << nested << "\"raw_trace_identity_values_emitted\": false,\n";
  out << nested << "\"source_contract_ready\": "
      << inferlab_plan::json_bool(source.contract_ready) << ",\n";
  out << nested << "\"source_path_label\": "
      << inferlab_plan::json_string(source.source_path_label) << ",\n";
  out << nested << "\"source_path_redacted\": "
      << inferlab_plan::json_bool(source.path_redacted) << ",\n";
  out << nested << "\"source_path_probe_status\": "
      << inferlab_plan::json_string(source.path_probe_status) << ",\n";
  out << nested << "\"source_physical_line_count\": "
      << source.physical_line_count << ",\n";
  out << nested << "\"source_present\": "
      << inferlab_plan::json_bool(source.present) << ",\n";
  out << nested << "\"source_readable\": "
      << inferlab_plan::json_bool(source.readable) << ",\n";
  out << nested << "\"source_supplied\": "
      << inferlab_plan::json_bool(source.supplied) << ",\n";
  out << nested << "\"source_text_read_status\": "
      << inferlab_plan::json_string(source.text_read_status) << ",\n";
  out << nested << "\"uncalibrated_timestamp_count\": "
      << projection.uncalibrated_timestamp_count << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace inferlab_trace_clock
