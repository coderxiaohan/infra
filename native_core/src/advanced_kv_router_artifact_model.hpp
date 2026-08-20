#pragma once

#include "inferlab/advanced_serving_scheduler.hpp"
#include "native_trace_context.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab::advanced_kv_router_artifact {

struct WorkloadRecord {
  std::size_t request_ordinal = 0U;
  std::string prefix_class;
  int prompt_tokens = 0;
  int output_tokens = 0;
  int total_tokens = 0;
  bool prefix_cache_eligible = false;
};

struct DecisionRecord {
  std::size_t request_ordinal = 0U;
  std::string trace_record_key;
  int worker_id = 0;
  bool cache_hit = false;
  int reused_tokens = 0;
  int recomputed_tokens = 0;
  int worker_load_after = 0;
};

struct PolicyRecord {
  std::string policy;
  int worker_count = 0;
  int request_count = 0;
  int cache_hits = 0;
  int reused_tokens = 0;
  int recomputed_tokens = 0;
  int worker_load_imbalance = 0;
  int peak_worker_load = 0;
  double prefix_hit_rate = 0.0;
  std::vector<int> final_worker_loads;
  std::vector<DecisionRecord> decisions;
};

struct Plan {
  int worker_count = 0;
  std::vector<WorkloadRecord> workload;
  std::vector<PolicyRecord> records;
  std::size_t best_prefix_hit_record_index = 0U;
  inferlab_native_trace::Projection trace_context;
};

Plan build_plan(int worker_count,
                const std::vector<AdvancedRequest>& requests);

Plan build_plan(int worker_count,
                const std::vector<AdvancedRequest>& requests,
                inferlab_native_trace::Source trace_source);

void validate_plan(const Plan& plan);

}  // namespace inferlab::advanced_kv_router_artifact
