#include "attention_probe_model.hpp"

#include "checked_index.hpp"

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace inferlab::attention_probe {
namespace {

bool same_shape(const AttentionShape& lhs, const AttentionShape& rhs) {
  return lhs.num_heads == rhs.num_heads &&
         lhs.query_length == rhs.query_length &&
         lhs.key_length == rhs.key_length &&
         lhs.head_dim == rhs.head_dim;
}

bool same_footprint(const Footprint& lhs, const Footprint& rhs) {
  return lhs.query_elements == rhs.query_elements &&
         lhs.kv_elements == rhs.kv_elements &&
         lhs.score_elements == rhs.score_elements &&
         lhs.working_elements == rhs.working_elements;
}

bool same_profile(const AttentionReadProfile& lhs,
                  const AttentionReadProfile& rhs) {
  return lhs.score_elements == rhs.score_elements &&
         lhs.unmasked_score_elements == rhs.unmasked_score_elements &&
         lhs.masked_score_elements == rhs.masked_score_elements &&
         lhs.query_element_reads_for_scores ==
             rhs.query_element_reads_for_scores &&
         lhs.key_element_reads_for_scores ==
             rhs.key_element_reads_for_scores &&
         lhs.value_element_reads_dense == rhs.value_element_reads_dense &&
         lhs.value_element_reads_mask_aware ==
             rhs.value_element_reads_mask_aware &&
         lhs.kv_element_reads_dense == rhs.kv_element_reads_dense &&
         lhs.kv_element_reads_mask_aware ==
             rhs.kv_element_reads_mask_aware &&
         lhs.score_intermediate_bytes == rhs.score_intermediate_bytes &&
         lhs.probability_intermediate_bytes ==
             rhs.probability_intermediate_bytes &&
         lhs.total_intermediate_bytes == rhs.total_intermediate_bytes &&
         lhs.output_write_elements == rhs.output_write_elements &&
         lhs.qk_multiply_add_ops == rhs.qk_multiply_add_ops &&
         lhs.value_multiply_add_ops_dense ==
             rhs.value_multiply_add_ops_dense &&
         lhs.value_multiply_add_ops_mask_aware ==
             rhs.value_multiply_add_ops_mask_aware &&
         lhs.causal_mask_sparsity == rhs.causal_mask_sparsity;
}

Footprint plan_footprint(const AttentionShape& shape) {
  if (shape.num_heads <= 0 || shape.query_length <= 0 ||
      shape.key_length <= 0 || shape.head_dim <= 0) {
    throw std::invalid_argument(
        "attention probe shape dimensions must be positive");
  }
  Footprint footprint;
  footprint.query_elements = internal::checked_product_size(
      shape.num_heads, shape.query_length, shape.head_dim,
      "attention probe query elements");
  footprint.kv_elements = internal::checked_product_size(
      shape.num_heads, shape.key_length, shape.head_dim,
      "attention probe KV elements");
  footprint.score_elements = internal::checked_product_size(
      shape.num_heads, shape.query_length, shape.key_length,
      "attention probe score elements");
  const auto tensor_elements = internal::checked_add_size(
      internal::checked_product_size(
          footprint.query_elements, 2,
          "attention probe query/output elements"),
      internal::checked_product_size(
          footprint.kv_elements, 2,
          "attention probe key/value elements"),
      "attention probe tensor elements");
  footprint.working_elements = internal::checked_add_size(
      tensor_elements,
      internal::checked_product_size(
          footprint.score_elements, 2,
          "attention probe score/probability elements"),
      "attention probe working elements");
  if (footprint.working_elements > kMaxWorkingElements) {
    throw std::invalid_argument(
        "attention probe exceeds reference-probe working-set cap");
  }
  return footprint;
}

std::vector<float> build_tensor(const std::size_t size, const int seed) {
  std::vector<float> tensor(size);
  for (std::size_t index = 0U; index < size; ++index) {
    const int centered = static_cast<int>(
                             (index * 17U +
                              static_cast<std::size_t>(seed)) %
                             23U) -
                         11;
    tensor[index] = static_cast<float>(centered) / 11.0F;
  }
  return tensor;
}

std::vector<float> execute_attention(const AttentionShape& shape,
                                     const Footprint& footprint) {
  const auto query = build_tensor(footprint.query_elements, 3);
  const auto key = build_tensor(footprint.kv_elements, 5);
  const auto value = build_tensor(footprint.kv_elements, 7);
  return scaled_dot_product_attention_cpu(query, key, value, shape);
}

Request validate_request(const Request& request) {
  (void)plan_footprint(request.shape);
  if (request.causal_profile_length <= 0) {
    throw std::invalid_argument(
        "causal profile length must be positive");
  }
  return request;
}

}  // namespace

Plan build_plan(const Request& request) {
  (void)validate_request(request);
  Plan plan;
  plan.request = request;
  plan.footprint = plan_footprint(request.shape);
  plan.causal_shape = {
      request.shape.num_heads,
      request.causal_profile_length,
      request.causal_profile_length,
      request.shape.head_dim,
  };
  plan.output = execute_attention(request.shape, plan.footprint);
  plan.non_causal_profile = profile_attention_reads(request.shape, false);
  plan.causal_profile = profile_attention_reads(plan.causal_shape, true);
  plan.output_sum_checksum =
      internal::checked_float_vector_sum_as_double(
          plan.output, "attention probe output");
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  (void)validate_request(plan.request);
  const auto expected_footprint = plan_footprint(plan.request.shape);
  const AttentionShape expected_causal_shape{
      plan.request.shape.num_heads,
      plan.request.causal_profile_length,
      plan.request.causal_profile_length,
      plan.request.shape.head_dim,
  };
  if (!same_footprint(plan.footprint, expected_footprint) ||
      !same_shape(plan.causal_shape, expected_causal_shape)) {
    throw std::logic_error(
        "attention probe derived shape or footprint is inconsistent");
  }
  const auto expected_output =
      execute_attention(plan.request.shape, expected_footprint);
  const auto expected_non_causal =
      profile_attention_reads(plan.request.shape, false);
  const auto expected_causal =
      profile_attention_reads(expected_causal_shape, true);
  if (plan.output != expected_output ||
      !same_profile(plan.non_causal_profile, expected_non_causal) ||
      !same_profile(plan.causal_profile, expected_causal) ||
      plan.non_causal_profile.score_elements < 0 ||
      static_cast<std::size_t>(
          plan.non_causal_profile.score_elements) !=
          plan.footprint.score_elements) {
    throw std::logic_error(
        "attention probe result is not canonical");
  }
  const auto expected_checksum =
      internal::checked_float_vector_sum_as_double(
          plan.output, "attention probe output");
  if (!std::isfinite(plan.output_sum_checksum) ||
      plan.output_sum_checksum != expected_checksum) {
    throw std::logic_error(
        "attention probe output summary is inconsistent");
  }
}

}  // namespace inferlab::attention_probe
