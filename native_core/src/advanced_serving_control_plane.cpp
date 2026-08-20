#include "inferlab/advanced_serving_control_plane.hpp"

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

#include <algorithm>
#include <stdexcept>

namespace inferlab {
namespace {

using advanced_serving_internal::finite_product;
using advanced_serving_internal::non_negative_finite_or_zero;
using advanced_serving_internal::positive_finite_or_default;
using advanced_serving_internal::require_finite;

double bounded_fraction(const double value, const char* name) {
  return std::clamp(require_finite(value, name), 0.0, 1.0);
}

double bounded_percentage(const double value, const char* name) {
  return std::clamp(require_finite(value, name), 0.0, 100.0);
}

}  // namespace

CostAccountingResult estimate_cost_accounting(
    const CostAccountingScenario& scenario) {
  const double request_qps = non_negative_finite_or_zero(
      scenario.request_qps, "cost-accounting request QPS");
  const int prompt_tokens = std::max(0, scenario.prompt_tokens);
  const int output_tokens = std::max(0, scenario.output_tokens);
  const int replicas = std::max(0, scenario.replicas);
  const double gpu_hour_cost = non_negative_finite_or_zero(
      scenario.gpu_hour_cost, "cost-accounting GPU-hour cost");
  const double cache_hit_rate = bounded_fraction(
      scenario.cache_hit_rate, "cost-accounting cache hit rate");
  const double tokens_per_second_per_replica = positive_finite_or_default(
      scenario.tokens_per_second_per_replica, 1000.0,
      "cost-accounting tokens per second per replica");
  const int billable_tokens_per_request = internal::checked_add_int(
      prompt_tokens, output_tokens, "cost-accounting billable tokens");
  const double billable_tokens_per_second = finite_product(
      request_qps, static_cast<double>(billable_tokens_per_request),
      "cost-accounting billable tokens per second");
  const double uncached_prompt_tokens = finite_product(
      static_cast<double>(prompt_tokens), 1.0 - cache_hit_rate,
      "cost-accounting uncached prompt tokens");
  const double effective_tokens_per_request = require_finite(
      uncached_prompt_tokens + static_cast<double>(output_tokens),
      "cost-accounting effective tokens per request");
  const double effective_tokens_per_second = finite_product(
      request_qps, effective_tokens_per_request,
      "cost-accounting effective tokens per second");
  if (effective_tokens_per_second > 0.0 && replicas == 0) {
    throw std::invalid_argument(
        "cost-accounting active token demand requires a positive replica count");
  }
  const double hourly_cost = finite_product(
      static_cast<double>(replicas), gpu_hour_cost,
      "cost-accounting hourly cost");
  const double daily_cost =
      finite_product(hourly_cost, 24.0, "cost-accounting daily cost");
  double cost_per_million_tokens = 0.0;
  if (billable_tokens_per_second > 0.0) {
    const double billable_million_tokens_per_hour = require_finite(
        finite_product(billable_tokens_per_second, 3600.0,
                       "cost-accounting billable tokens per hour") /
            1000000.0,
        "cost-accounting million tokens per hour");
    cost_per_million_tokens = require_finite(
        hourly_cost / billable_million_tokens_per_hour,
        "cost-accounting cost per million tokens");
  }
  const double capacity_tokens_per_second = finite_product(
      static_cast<double>(replicas), tokens_per_second_per_replica,
      "cost-accounting capacity tokens per second");
  const double utilization = capacity_tokens_per_second > 0.0
      ? require_finite(
            effective_tokens_per_second / capacity_tokens_per_second,
            "cost-accounting utilization")
      : 0.0;

  CostAccountingResult result;
  result.scenario = scenario;
  result.scenario.request_qps = request_qps;
  result.scenario.prompt_tokens = prompt_tokens;
  result.scenario.output_tokens = output_tokens;
  result.scenario.replicas = replicas;
  result.scenario.gpu_hour_cost = gpu_hour_cost;
  result.scenario.cache_hit_rate = cache_hit_rate;
  result.scenario.tokens_per_second_per_replica =
      tokens_per_second_per_replica;
  result.billable_tokens_per_second = billable_tokens_per_second;
  result.effective_tokens_per_second = effective_tokens_per_second;
  result.hourly_cost = hourly_cost;
  result.daily_cost = daily_cost;
  result.cost_per_million_tokens = cost_per_million_tokens;
  result.utilization = utilization;
  return result;
}

std::vector<CostAccountingScenario> default_cost_accounting_scenarios() {
  return {
      {"chat_small", 80.0, 1024, 256, 8, 3.20, 0.20, 24000.0},
      {"rag_long_prompt", 35.0, 8192, 512, 16, 4.80, 0.55, 22000.0},
      {"reasoning_large", 12.0, 4096, 2048, 24, 6.50, 0.10, 14000.0},
      {"batch_embeddings", 180.0, 512, 32, 4, 2.40, 0.00, 42000.0},
  };
}

}  // namespace inferlab
