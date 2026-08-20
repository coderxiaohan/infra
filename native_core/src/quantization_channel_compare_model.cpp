#include "quantization_channel_compare_model.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <stdexcept>
#include <vector>

namespace inferlab::quantization_channel_compare {
namespace {

void validate_config(const Config& config) {
  if (config.rows <= 0 || config.cols <= 0 ||
      config.group_size <= 0) {
    throw std::invalid_argument(
        "quantization channel-compare dimensions and group size must be positive");
  }
  static_cast<void>(inferlab_plan::checked_element_count(
      "quantization channel-compare weights", config.rows,
      config.cols));
}

std::vector<float> build_weights(const Config& config) {
  auto weights = quantization_reference::build_periodic_fixture(
      config.rows, config.cols, {13, 7, 9, 4, 4.0F},
      "quantization channel-compare weights");
  const int split_column = config.cols / 2;
  for (int row = 0; row < config.rows; ++row) {
    for (int col = 0; col < config.cols; ++col) {
      const auto index = internal::checked_flat_index(
          row, config.cols, col,
          "quantization channel-compare weights");
      const float column_scale =
          col < split_column ? 0.1F : 2.0F;
      weights[index] *= column_scale;
    }
  }
  return weights;
}

Record run_per_channel(const Config& config,
                       const std::vector<float>& weights) {
  const auto result = quantization_reference::run_channel_reference(
      weights, config.rows, config.cols, QuantizationBits::kInt4);
  return {result.matrix.group_size,
          quantization_reference::summarize(result)};
}

Record run_per_group(const Config& config,
                     const std::vector<float>& weights) {
  const auto result = quantization_reference::run_group_reference(
      weights, config.rows, config.cols, config.group_size,
      QuantizationBits::kInt4);
  return {result.matrix.group_size,
          quantization_reference::summarize(result)};
}

bool same_record(const Record& lhs, const Record& rhs) {
  return lhs.group_size == rhs.group_size &&
         quantization_reference::same_summary(
             lhs.summary, rhs.summary);
}

}  // namespace

Config default_config() { return Config{}; }

Plan build_plan(const Config& config) {
  validate_config(config);
  Plan plan;
  plan.config = config;
  plan.weights = build_weights(config);
  plan.per_channel = run_per_channel(config, plan.weights);
  plan.per_group = run_per_group(config, plan.weights);
  plan.fp32_bytes = internal::checked_product_size(
      plan.weights.size(), sizeof(float),
      "fp32 quantization channel-compare bytes");
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_config(plan.config);
  if (plan.weights != build_weights(plan.config)) {
    throw std::logic_error(
        "quantization channel-compare fixture is not canonical");
  }
  if (!same_record(
          plan.per_channel,
          run_per_channel(plan.config, plan.weights)) ||
      !same_record(
          plan.per_group,
          run_per_group(plan.config, plan.weights))) {
    throw std::logic_error(
        "quantization channel-compare result is not canonical");
  }
  if (plan.per_channel.group_size != plan.config.cols ||
      plan.per_group.group_size != plan.config.group_size) {
    throw std::logic_error(
        "quantization channel-compare granularity is inconsistent");
  }

  const auto expected_fp32_bytes = internal::checked_product_size(
      plan.weights.size(), sizeof(float),
      "fp32 quantization channel-compare bytes");
  if (plan.fp32_bytes == 0U ||
      plan.fp32_bytes != expected_fp32_bytes) {
    throw std::logic_error(
        "quantization channel-compare FP32 storage summary is not canonical");
  }
}

}  // namespace inferlab::quantization_channel_compare
