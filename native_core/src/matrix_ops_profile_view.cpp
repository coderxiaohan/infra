#include "matrix_ops_profile_view.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <iomanip>
#include <sstream>
#include <string>
#include <utility>

namespace inferlab::matrix_ops_profile {
namespace {

std::pair<std::string, std::string> indentation(const int indent) {
  return {
      inferlab_plan::json_indent(indent),
      inferlab_plan::json_indent(internal::checked_add_int(
          indent, 2, "matrix ops profile JSON indent")),
  };
}

std::string dense_json(const DenseOpRooflineProfile& profile,
                       const int indent) {
  const auto [pad, nested] = indentation(indent);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"compulsory_arithmetic_intensity\": " << std::fixed
      << std::setprecision(6) << profile.compulsory_arithmetic_intensity
      << ",\n";
  out << nested << "\"compulsory_bytes\": " << profile.compulsory_bytes
      << ",\n";
  out << nested << "\"flop_count\": " << profile.flop_count << ",\n";
  out << nested << "\"fma_count\": " << profile.fma_count << ",\n";
  out << nested << "\"k\": " << profile.k << ",\n";
  out << nested << "\"lhs_unique_elements\": "
      << profile.lhs_unique_elements << ",\n";
  out << nested << "\"m\": " << profile.m << ",\n";
  out << nested << "\"n\": " << profile.n << ",\n";
  out << nested << "\"naive_lhs_element_reads\": "
      << profile.naive_lhs_element_reads << ",\n";
  out << nested << "\"naive_loop_arithmetic_intensity\": " << std::fixed
      << std::setprecision(6) << profile.naive_loop_arithmetic_intensity
      << ",\n";
  out << nested << "\"naive_loop_bytes\": " << profile.naive_loop_bytes
      << ",\n";
  out << nested << "\"naive_output_element_writes\": "
      << profile.naive_output_element_writes << ",\n";
  out << nested << "\"naive_rhs_element_reads\": "
      << profile.naive_rhs_element_reads << ",\n";
  out << nested << "\"output_elements\": " << profile.output_elements
      << ",\n";
  out << nested << "\"rhs_unique_elements\": "
      << profile.rhs_unique_elements << "\n";
  out << pad << "}";
  return out.str();
}

std::string softmax_json(const RowSoftmaxRooflineProfile& profile,
                         const int indent) {
  const auto [pad, nested] = indentation(indent);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"add_ops\": " << profile.add_ops << ",\n";
  out << nested << "\"cols\": " << profile.cols << ",\n";
  out << nested << "\"compare_ops\": " << profile.compare_ops << ",\n";
  out << nested << "\"div_ops\": " << profile.div_ops << ",\n";
  out << nested << "\"elements\": " << profile.elements << ",\n";
  out << nested << "\"exp_ops\": " << profile.exp_ops << ",\n";
  out << nested << "\"input_read_bytes\": " << profile.input_read_bytes
      << ",\n";
  out << nested << "\"output_read_bytes\": " << profile.output_read_bytes
      << ",\n";
  out << nested << "\"output_write_bytes\": " << profile.output_write_bytes
      << ",\n";
  out << nested << "\"rows\": " << profile.rows << ",\n";
  out << nested << "\"scalar_ops_excluding_exp\": "
      << profile.scalar_ops_excluding_exp << ",\n";
  out << nested << "\"scalar_ops_per_byte\": " << std::fixed
      << std::setprecision(6) << profile.scalar_ops_per_byte << ",\n";
  out << nested << "\"total_memory_bytes\": "
      << profile.total_memory_bytes << "\n";
  out << pad << "}";
  return out.str();
}

std::string tiled_json(const TiledMatmulRooflineProfile& profile,
                       const int indent) {
  const auto [pad, nested] = indentation(indent);
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"flop_count\": " << profile.flop_count << ",\n";
  out << nested << "\"k\": " << profile.k << ",\n";
  out << nested << "\"m\": " << profile.m << ",\n";
  out << nested << "\"n\": " << profile.n << ",\n";
  out << nested << "\"naive_to_tiled_byte_reduction\": " << std::fixed
      << std::setprecision(6) << profile.naive_to_tiled_byte_reduction
      << ",\n";
  out << nested << "\"output_element_writes\": "
      << profile.output_element_writes << ",\n";
  out << nested << "\"tile_k\": " << profile.tile_k << ",\n";
  out << nested << "\"tile_m\": " << profile.tile_m << ",\n";
  out << nested << "\"tile_n\": " << profile.tile_n << ",\n";
  out << nested << "\"tiled_arithmetic_intensity\": " << std::fixed
      << std::setprecision(6) << profile.tiled_arithmetic_intensity << ",\n";
  out << nested << "\"tiled_global_bytes\": "
      << profile.tiled_global_bytes << ",\n";
  out << nested << "\"tiled_lhs_element_loads\": "
      << profile.tiled_lhs_element_loads << ",\n";
  out << nested << "\"tiled_rhs_element_loads\": "
      << profile.tiled_rhs_element_loads << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

std::string artifact_json(const Plan& plan) {
  validate_plan(plan);
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_matrix_ops_profile",
      "Executed static scalar operation/byte-count modeling only; no matrix "
      "kernel, benchmark timing, GPU profiling, or product runtime was "
      "executed by this profile artifact.");
  out << "  \"matrix_ops_profile\": {\n";
  out << "    \"profile_only\": true,\n";
  out << "    \"matmul\": " << dense_json(plan.matmul, 4) << ",\n";
  out << "    \"matvec\": " << dense_json(plan.matvec, 4) << ",\n";
  out << "    \"tiled_matmul\": "
      << tiled_json(plan.tiled_matmul, 4) << ",\n";
  out << "    \"row_softmax\": "
      << softmax_json(plan.row_softmax, 4) << "\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab::matrix_ops_profile
