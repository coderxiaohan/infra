#include "quantization_channel_compare_view.hpp"

#include "plan_json_utils.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::quantization_channel_compare {
namespace {

std::string record_json(const char* name, const Record& record,
                        const std::size_t fp32_bytes) {
  std::ostringstream out;
  out << "    \"" << name << "\": {\n";
  out << "      \"group_size\": " << record.group_size << ",\n";
  out << "      \"max_abs_error\": " << std::fixed
      << std::setprecision(6) << record.summary.error.max_abs_error
      << ",\n";
  out << "      \"mean_abs_error\": " << std::fixed
      << std::setprecision(6) << record.summary.error.mean_abs_error
      << ",\n";
  out << "      \"scale_count\": " << record.summary.scale_count
      << ",\n";
  out << "      \"storage_bytes\": "
      << record.summary.storage_bytes << ",\n";
  out << "      \"storage_ratio_vs_fp32\": " << std::fixed
      << std::setprecision(6)
      << (static_cast<double>(record.summary.storage_bytes) /
          static_cast<double>(fp32_bytes))
      << "\n";
  out << "    }";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);

  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_quantization_channel_compare",
      "Executed deterministic scalar CPU granularity comparison only; not "
      "calibration evidence, a production packed-weight implementation, or "
      "a CUDA/product performance result.");
  out << "  \"config\": {\n";
  out << "    \"cols\": " << plan.config.cols << ",\n";
  out << "    \"fp32_bytes\": " << plan.fp32_bytes << ",\n";
  out << "    \"per_group_size\": " << plan.config.group_size
      << ",\n";
  out << "    \"rows\": " << plan.config.rows << "\n";
  out << "  },\n";
  out << "  \"comparison\": {\n";
  out << record_json("per_channel", plan.per_channel,
                     plan.fp32_bytes)
      << ",\n";
  out << record_json("per_group", plan.per_group, plan.fp32_bytes)
      << "\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::quantization_channel_compare
