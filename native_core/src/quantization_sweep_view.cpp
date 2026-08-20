#include "quantization_sweep_view.hpp"

#include "plan_json_utils.hpp"

#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::quantization_sweep {
namespace {

std::string record_json(const Record& record,
                        const std::size_t fp32_bytes) {
  std::ostringstream out;
  out << "    {\n";
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
      out, "native_quantization_sweep",
      "Executed deterministic scalar CPU group-size sweep only; not a "
      "production quantization policy recommendation, CUDA benchmark, or "
      "product runtime performance result.");
  out << "  \"config\": {\n";
  out << "    \"cols\": " << plan.config.cols << ",\n";
  out << "    \"fp32_bytes\": " << plan.fp32_bytes << ",\n";
  out << "    \"rows\": " << plan.config.rows << "\n";
  out << "  },\n";
  out << "  \"int4_group_sweep\": [\n";
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    if (index != 0U) {
      out << ",\n";
    }
    out << record_json(plan.records[index], plan.fp32_bytes);
  }
  out << "\n  ]\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::quantization_sweep
