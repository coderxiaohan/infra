#include "quantization_sweep_model.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <stdexcept>
#include <vector>

namespace inferlab::quantization_sweep {
namespace {

const std::vector<int>& canonical_group_size_inventory() {
  static const std::vector<int> inventory = {2, 4, 8, 16};
  return inventory;
}

void validate_config(const Config& config) {
  if (config.rows <= 0 || config.cols <= 0) {
    throw std::invalid_argument(
        "quantization sweep dimensions must be positive");
  }
  static_cast<void>(inferlab_plan::checked_element_count(
      "quantization sweep weights", config.rows, config.cols));
}

std::vector<float> build_weights(const Config& config) {
  return quantization_reference::build_periodic_fixture(
      config.rows, config.cols, {19, 11, 25, 12, 12.0F},
      "quantization sweep weights");
}

std::vector<Record> execute(const Config& config,
                            const std::vector<float>& weights) {
  std::vector<Record> records;
  for (const int group_size : canonical_group_size_inventory()) {
    if (group_size > config.cols) {
      continue;
    }
    const auto result = quantization_reference::run_group_reference(
        weights, config.rows, config.cols, group_size,
        QuantizationBits::kInt4);
    records.push_back(
        {group_size, quantization_reference::summarize(result)});
  }
  return records;
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
  plan.group_size_inventory = canonical_group_size_inventory();
  plan.weights = build_weights(config);
  plan.records = execute(config, plan.weights);
  plan.fp32_bytes = internal::checked_product_size(
      plan.weights.size(), sizeof(float),
      "fp32 quantization sweep bytes");
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_config(plan.config);
  if (plan.group_size_inventory != canonical_group_size_inventory()) {
    throw std::logic_error(
        "quantization sweep group-size inventory is not canonical");
  }
  if (plan.weights != build_weights(plan.config)) {
    throw std::logic_error(
        "quantization sweep fixture is not canonical");
  }

  const auto expected_records = execute(plan.config, plan.weights);
  if (plan.records.size() != expected_records.size()) {
    throw std::logic_error(
        "quantization sweep record count is not canonical");
  }
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    if (!same_record(plan.records[index], expected_records[index])) {
      throw std::logic_error(
          "quantization sweep record is not canonical");
    }
  }

  const auto expected_fp32_bytes = internal::checked_product_size(
      plan.weights.size(), sizeof(float),
      "fp32 quantization sweep bytes");
  if (plan.fp32_bytes == 0U ||
      plan.fp32_bytes != expected_fp32_bytes) {
    throw std::logic_error(
        "quantization sweep FP32 storage summary is not canonical");
  }
}

}  // namespace inferlab::quantization_sweep
