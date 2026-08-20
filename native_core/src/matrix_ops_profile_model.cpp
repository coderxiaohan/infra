#include "matrix_ops_profile_model.hpp"

#include <stdexcept>

namespace inferlab::matrix_ops_profile {
namespace {

void validate_request(const Request& request) {
  if (request.m <= 0 || request.k <= 0 || request.n <= 0 ||
      request.tile_m <= 0 || request.tile_k <= 0 ||
      request.tile_n <= 0 || request.softmax_rows <= 0 ||
      request.softmax_cols <= 0) {
    throw std::invalid_argument(
        "matrix ops profile dimensions must be positive");
  }
}

bool same_dense(const DenseOpRooflineProfile& lhs,
                const DenseOpRooflineProfile& rhs) {
  return lhs.m == rhs.m && lhs.k == rhs.k && lhs.n == rhs.n &&
         lhs.lhs_unique_elements == rhs.lhs_unique_elements &&
         lhs.rhs_unique_elements == rhs.rhs_unique_elements &&
         lhs.output_elements == rhs.output_elements &&
         lhs.fma_count == rhs.fma_count &&
         lhs.flop_count == rhs.flop_count &&
         lhs.compulsory_bytes == rhs.compulsory_bytes &&
         lhs.naive_lhs_element_reads == rhs.naive_lhs_element_reads &&
         lhs.naive_rhs_element_reads == rhs.naive_rhs_element_reads &&
         lhs.naive_output_element_writes ==
             rhs.naive_output_element_writes &&
         lhs.naive_loop_bytes == rhs.naive_loop_bytes &&
         lhs.compulsory_arithmetic_intensity ==
             rhs.compulsory_arithmetic_intensity &&
         lhs.naive_loop_arithmetic_intensity ==
             rhs.naive_loop_arithmetic_intensity;
}

bool same_tiled(const TiledMatmulRooflineProfile& lhs,
                const TiledMatmulRooflineProfile& rhs) {
  return lhs.m == rhs.m && lhs.k == rhs.k && lhs.n == rhs.n &&
         lhs.tile_m == rhs.tile_m && lhs.tile_k == rhs.tile_k &&
         lhs.tile_n == rhs.tile_n &&
         lhs.flop_count == rhs.flop_count &&
         lhs.tiled_lhs_element_loads == rhs.tiled_lhs_element_loads &&
         lhs.tiled_rhs_element_loads == rhs.tiled_rhs_element_loads &&
         lhs.output_element_writes == rhs.output_element_writes &&
         lhs.tiled_global_bytes == rhs.tiled_global_bytes &&
         lhs.tiled_arithmetic_intensity ==
             rhs.tiled_arithmetic_intensity &&
         lhs.naive_to_tiled_byte_reduction ==
             rhs.naive_to_tiled_byte_reduction;
}

bool same_softmax(const RowSoftmaxRooflineProfile& lhs,
                  const RowSoftmaxRooflineProfile& rhs) {
  return lhs.rows == rhs.rows && lhs.cols == rhs.cols &&
         lhs.elements == rhs.elements &&
         lhs.input_read_bytes == rhs.input_read_bytes &&
         lhs.output_read_bytes == rhs.output_read_bytes &&
         lhs.output_write_bytes == rhs.output_write_bytes &&
         lhs.total_memory_bytes == rhs.total_memory_bytes &&
         lhs.compare_ops == rhs.compare_ops &&
         lhs.exp_ops == rhs.exp_ops && lhs.add_ops == rhs.add_ops &&
         lhs.div_ops == rhs.div_ops &&
         lhs.scalar_ops_excluding_exp == rhs.scalar_ops_excluding_exp &&
         lhs.scalar_ops_per_byte == rhs.scalar_ops_per_byte;
}

}  // namespace

Plan build_plan(const Request& request) {
  validate_request(request);
  Plan plan;
  plan.request = request;
  plan.matmul = profile_matmul_roofline(
      request.m, request.k, request.n);
  plan.matvec = profile_matvec_roofline(request.m, request.k);
  plan.tiled_matmul = profile_tiled_matmul_roofline(
      request.m, request.k, request.n, request.tile_m,
      request.tile_k, request.tile_n);
  plan.row_softmax = profile_row_softmax_roofline(
      request.softmax_rows, request.softmax_cols);
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_request(plan.request);
  const auto matmul = profile_matmul_roofline(
      plan.request.m, plan.request.k, plan.request.n);
  const auto matvec =
      profile_matvec_roofline(plan.request.m, plan.request.k);
  const auto tiled = profile_tiled_matmul_roofline(
      plan.request.m, plan.request.k, plan.request.n,
      plan.request.tile_m, plan.request.tile_k,
      plan.request.tile_n);
  const auto softmax = profile_row_softmax_roofline(
      plan.request.softmax_rows, plan.request.softmax_cols);
  if (!same_dense(plan.matmul, matmul) ||
      !same_dense(plan.matvec, matvec) ||
      !same_tiled(plan.tiled_matmul, tiled) ||
      !same_softmax(plan.row_softmax, softmax)) {
    throw std::logic_error(
        "matrix ops profile plan is not canonical");
  }
}

}  // namespace inferlab::matrix_ops_profile
