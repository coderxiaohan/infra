#pragma once

#include <string>
#include <vector>

namespace inferlab {

struct CostAccountingScenario {
  std::string name;
  double request_qps = 1.0;
  int prompt_tokens = 0;
  int output_tokens = 0;
  int replicas = 1;
  double gpu_hour_cost = 3.0;
  double cache_hit_rate = 0.0;
  double tokens_per_second_per_replica = 1000.0;
};

struct CostAccountingResult {
  CostAccountingScenario scenario;
  double billable_tokens_per_second = 0.0;
  double effective_tokens_per_second = 0.0;
  double hourly_cost = 0.0;
  double daily_cost = 0.0;
  double cost_per_million_tokens = 0.0;
  double utilization = 0.0;
};

CostAccountingResult estimate_cost_accounting(
    const CostAccountingScenario& scenario);

std::vector<CostAccountingScenario> default_cost_accounting_scenarios();

}  // namespace inferlab
