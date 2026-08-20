#include "quantization_probe_model.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::quantization_probe {
namespace {

void validate_config(const Config& config) {
  if (config.rows <= 0 || config.cols <= 0 ||
      config.group_size <= 0) {
    throw std::invalid_argument(
        "quantization probe dimensions and group size must be positive");
  }
  static_cast<void>(inferlab_plan::checked_element_count(
      "quantization weights", config.rows, config.cols));
}

std::vector<float> build_weights(const Config& config) {
  return quantization_reference::build_periodic_fixture(
      config.rows, config.cols, {17, 13, 21, 10, 10.0F},
      "quantization weights");
}

std::vector<float> build_activation(const Config& config) {
  return quantization_reference::build_periodic_fixture(
      1, config.cols, {0, 7, 11, 5, 5.0F},
      "quantization activation");
}

void validate_quantized_result(
    const quantization_reference::Result& actual,
    const std::vector<float>& weights,
    const Config& config,
    const QuantizationBits bits,
    const char* label) {
  const auto expected = quantization_reference::run_group_reference(
      weights, config.rows, config.cols, config.group_size, bits);
  if (!quantization_reference::same_result(actual, expected)) {
    throw std::logic_error(std::string(label) +
                           " result is not canonical");
  }
}

}  // namespace

Config default_config() { return Config{}; }

Plan build_plan(const Config& config) {
  validate_config(config);
  Plan plan;
  plan.config = config;
  plan.weights = build_weights(config);
  plan.activation = build_activation(config);
  plan.int8 = quantization_reference::run_group_reference(
      plan.weights, config.rows, config.cols, config.group_size,
      QuantizationBits::kInt8);
  plan.int4 = quantization_reference::run_group_reference(
      plan.weights, config.rows, config.cols, config.group_size,
      QuantizationBits::kInt4);
  plan.int4_materialized =
      weight_only_matvec(plan.int4.matrix, plan.activation);
  plan.int4_fused =
      fused_dequant_matvec(plan.int4.matrix, plan.activation);
  plan.int4_matvec_diff =
      error_stats(plan.int4_materialized, plan.int4_fused);
  plan.fp32_bytes = internal::checked_product_size(
      plan.weights.size(), sizeof(float), "fp32 quantization bytes");
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_config(plan.config);
  if (plan.weights != build_weights(plan.config) ||
      plan.activation != build_activation(plan.config)) {
    throw std::logic_error(
        "quantization probe fixture is not canonical");
  }

  validate_quantized_result(
      plan.int8, plan.weights, plan.config, QuantizationBits::kInt8,
      "INT8 quantization");
  validate_quantized_result(
      plan.int4, plan.weights, plan.config, QuantizationBits::kInt4,
      "INT4 quantization");

  const auto expected_materialized =
      weight_only_matvec(plan.int4.matrix, plan.activation);
  const auto expected_fused =
      fused_dequant_matvec(plan.int4.matrix, plan.activation);
  const auto expected_diff =
      error_stats(expected_materialized, expected_fused);
  if (plan.int4_materialized != expected_materialized ||
      plan.int4_fused != expected_fused ||
      !quantization_reference::same_error_stats(
          plan.int4_matvec_diff, expected_diff)) {
    throw std::logic_error(
        "quantization probe matvec result is not canonical");
  }

  const auto expected_fp32_bytes = internal::checked_product_size(
      plan.weights.size(), sizeof(float), "fp32 quantization bytes");
  if (plan.fp32_bytes == 0U ||
      plan.fp32_bytes != expected_fp32_bytes) {
    throw std::logic_error(
        "quantization probe FP32 storage summary is not canonical");
  }
}

}  // namespace inferlab::quantization_probe
