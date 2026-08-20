#include "quantization_probe_view.hpp"

#include "plan_json_utils.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace inferlab::quantization_probe {
namespace {

std::string quantized_result_json(const char* name,
                                  const quantization_reference::Result& result,
                                  const std::size_t fp32_bytes) {
  std::ostringstream out;
  out << "    \"" << name << "\": {\n";
  out << "      \"max_abs_error\": " << std::fixed
      << std::setprecision(6) << result.error.max_abs_error << ",\n";
  out << "      \"mean_abs_error\": " << std::fixed
      << std::setprecision(6) << result.error.mean_abs_error << ",\n";
  out << "      \"scale_count\": " << result.matrix.scales.size()
      << ",\n";
  out << "      \"storage_bytes\": " << result.storage_bytes << ",\n";
  out << "      \"storage_ratio_vs_fp32\": " << std::fixed
      << std::setprecision(6)
      << (static_cast<double>(result.storage_bytes) /
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
      out, "native_quantization_probe",
      "Executed deterministic scalar CPU quantization reference only; not "
      "a calibrated production quantizer, CUDA microkernel benchmark, or "
      "product runtime performance result.");
  out << "  \"config\": {\n";
  out << "    \"cols\": " << plan.config.cols << ",\n";
  out << "    \"fp32_bytes\": " << plan.fp32_bytes << ",\n";
  out << "    \"group_size\": " << plan.config.group_size << ",\n";
  out << "    \"rows\": " << plan.config.rows << "\n";
  out << "  },\n";
  out << "  \"quantization\": {\n";
  out << quantized_result_json("int8", plan.int8, plan.fp32_bytes)
      << ",\n";
  out << quantized_result_json("int4", plan.int4, plan.fp32_bytes)
      << "\n";
  out << "  },\n";
  out << "  \"weight_only_matvec\": {\n";
  out << "    \"int4_fused_vs_materialized_max_abs_error\": "
      << std::fixed << std::setprecision(6)
      << plan.int4_matvec_diff.max_abs_error << ",\n";
  out << "    \"int4_fused_vs_materialized_mean_abs_error\": "
      << std::fixed << std::setprecision(6)
      << plan.int4_matvec_diff.mean_abs_error << "\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::quantization_probe
