#include "advanced_kv_router_artifact_model.hpp"

#include "checked_index.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab::advanced_kv_router_artifact {
namespace {

constexpr std::size_t kMaximumRequestCount = 1024U;
constexpr std::size_t kMaximumPrefixIdBytes = 256U;

std::string trace_record_key(const std::string& policy,
                             const std::size_t request_ordinal) {
  return policy + ".request_" + std::to_string(request_ordinal);
}

std::vector<std::string> trace_record_keys(const Plan& plan) {
  const std::size_t record_count =
      inferlab::internal::checked_product_size(
          plan.records.size(), plan.workload.size(),
          "advanced KV router trace record count");
  std::vector<std::string> keys;
  keys.reserve(record_count);
  for (const auto& record : plan.records) {
    for (const auto& decision : record.decisions) {
      keys.push_back(decision.trace_record_key);
    }
  }
  return keys;
}

bool valid_prefix_class(const std::string& value) {
  constexpr char kPrefix[] = "prefix_class_";
  constexpr std::size_t kPrefixSize = sizeof(kPrefix) - 1U;
  if (value.size() <= kPrefixSize || value.rfind(kPrefix, 0U) != 0U) {
    return false;
  }
  return std::all_of(
      value.begin() + static_cast<std::ptrdiff_t>(kPrefixSize),
      value.end(), [](const char ch) { return ch >= '0' && ch <= '9'; });
}

bool preferred_prefix_hit_record(const PolicyRecord& candidate,
                                 const PolicyRecord& current) {
  if (candidate.prefix_hit_rate != current.prefix_hit_rate) {
    return candidate.prefix_hit_rate > current.prefix_hit_rate;
  }
  if (candidate.cache_hits != current.cache_hits) {
    return candidate.cache_hits > current.cache_hits;
  }
  if (candidate.recomputed_tokens != current.recomputed_tokens) {
    return candidate.recomputed_tokens < current.recomputed_tokens;
  }
  if (candidate.worker_load_imbalance != current.worker_load_imbalance) {
    return candidate.worker_load_imbalance < current.worker_load_imbalance;
  }
  return candidate.peak_worker_load < current.peak_worker_load;
}

std::size_t best_prefix_hit_record_index(
    const std::vector<PolicyRecord>& records) {
  if (records.empty()) {
    throw std::invalid_argument(
        "advanced KV router requires policy records");
  }
  std::size_t best = 0U;
  for (std::size_t index = 1U; index < records.size(); ++index) {
    if (preferred_prefix_hit_record(records[index], records[best])) {
      best = index;
    }
  }
  return best;
}

std::vector<WorkloadRecord> project_workload(
    const std::vector<AdvancedRequest>& requests) {
  if (requests.empty() || requests.size() > kMaximumRequestCount) {
    throw std::invalid_argument(
        "advanced KV router workload count is outside the supported range");
  }

  std::set<int> request_ids;
  std::map<std::string, std::string> prefix_classes;
  std::vector<WorkloadRecord> workload;
  workload.reserve(requests.size());
  for (std::size_t index = 0U; index < requests.size(); ++index) {
    const auto& request = requests[index];
    if (request.request_id <= 0 ||
        !request_ids.insert(request.request_id).second) {
      throw std::invalid_argument(
          "advanced KV router request ids must be positive and unique");
    }
    if (request.prompt_tokens < 0 || request.output_tokens < 0 ||
        request.prefix_id.size() > kMaximumPrefixIdBytes) {
      throw std::invalid_argument(
          "advanced KV router request fields are outside the supported range");
    }

    WorkloadRecord record;
    record.request_ordinal = inferlab::internal::checked_add_size(
        index, 1U, "advanced KV router request ordinal");
    record.prompt_tokens = request.prompt_tokens;
    record.output_tokens = request.output_tokens;
    record.total_tokens = inferlab::internal::checked_add_int(
        request.prompt_tokens, request.output_tokens,
        "advanced KV router request token count");
    record.prefix_cache_eligible =
        request.prompt_tokens > 0 && !request.prefix_id.empty();
    if (record.prefix_cache_eligible) {
      auto found = prefix_classes.find(request.prefix_id);
      if (found == prefix_classes.end()) {
        const std::size_t class_ordinal =
            inferlab::internal::checked_add_size(
                prefix_classes.size(), 1U,
                "advanced KV router prefix class ordinal");
        found = prefix_classes
                    .emplace(request.prefix_id,
                             "prefix_class_" +
                                 std::to_string(class_ordinal))
                    .first;
      }
      record.prefix_class = found->second;
    } else {
      record.prefix_class = "not_cacheable";
    }
    workload.push_back(std::move(record));
  }
  return workload;
}

PolicyRecord project_policy_record(
    const RoutingSummary& summary,
    const std::vector<AdvancedRequest>& requests) {
  if (summary.decisions.size() != requests.size()) {
    throw std::invalid_argument(
        "advanced KV router decision cardinality drift");
  }

  PolicyRecord record;
  record.policy = summary.policy;
  record.worker_count = summary.worker_count;
  record.request_count = summary.request_count;
  record.cache_hits = summary.cache_hits;
  record.reused_tokens = summary.reused_tokens;
  record.recomputed_tokens = summary.recomputed_tokens;
  record.worker_load_imbalance = summary.worker_load_imbalance;
  record.peak_worker_load = summary.peak_worker_load;
  record.prefix_hit_rate = summary.prefix_hit_rate;
  record.final_worker_loads = summary.final_worker_loads;
  record.decisions.reserve(summary.decisions.size());
  for (std::size_t index = 0U; index < summary.decisions.size(); ++index) {
    const auto& decision = summary.decisions[index];
    if (decision.request_id != requests[index].request_id ||
        decision.policy != summary.policy) {
      throw std::invalid_argument(
          "advanced KV router decision identity drift");
    }
    const std::size_t ordinal = inferlab::internal::checked_add_size(
        index, 1U, "advanced KV router decision ordinal");
    record.decisions.push_back(
        {ordinal,
         trace_record_key(summary.policy, ordinal),
         decision.worker_id,
         decision.cache_hit,
         decision.reused_tokens,
         decision.recomputed_tokens,
         decision.worker_load_after});
  }
  return record;
}

void validate_workload_record(const WorkloadRecord& record,
                              const std::size_t index) {
  const std::size_t expected_ordinal =
      inferlab::internal::checked_add_size(
          index, 1U, "advanced KV router workload ordinal");
  const int expected_total = inferlab::internal::checked_add_int(
      record.prompt_tokens, record.output_tokens,
      "advanced KV router workload token total");
  const bool cacheable = record.prompt_tokens > 0 &&
                         record.prefix_class != "not_cacheable";
  if (record.request_ordinal != expected_ordinal ||
      record.prompt_tokens < 0 || record.output_tokens < 0 ||
      record.total_tokens != expected_total || record.prefix_class.empty() ||
      record.prefix_cache_eligible != cacheable ||
      (record.prefix_cache_eligible &&
       !valid_prefix_class(record.prefix_class)) ||
      (!record.prefix_cache_eligible &&
       record.prefix_class != "not_cacheable")) {
    throw std::invalid_argument(
        "advanced KV router workload projection is inconsistent");
  }
}

void validate_policy_record(const Plan& plan,
                            const PolicyRecord& record,
                            const std::size_t policy_index) {
  const auto& policies = kv_aware_routing_policies();
  const int request_count = inferlab::internal::checked_int_from_size(
      plan.workload.size(), "advanced KV router request count");
  if (policy_index >= policies.size() ||
      record.policy != policies[policy_index] ||
      record.worker_count != plan.worker_count ||
      record.request_count != request_count ||
      record.decisions.size() != plan.workload.size() ||
      record.final_worker_loads.size() !=
          static_cast<std::size_t>(plan.worker_count) ||
      record.cache_hits < 0 || record.reused_tokens < 0 ||
      record.recomputed_tokens < 0 || record.worker_load_imbalance < 0 ||
      record.peak_worker_load < 0 ||
      !std::isfinite(record.prefix_hit_rate) ||
      record.prefix_hit_rate < 0.0 || record.prefix_hit_rate > 1.0) {
    throw std::invalid_argument(
        "advanced KV router policy summary is inconsistent");
  }

  std::vector<int> worker_loads(
      static_cast<std::size_t>(plan.worker_count), 0);
  std::vector<std::set<std::string>> worker_prefixes(
      static_cast<std::size_t>(plan.worker_count));
  int cache_hits = 0;
  int reused_tokens = 0;
  int recomputed_tokens = 0;
  for (std::size_t index = 0U; index < record.decisions.size(); ++index) {
    const auto& decision = record.decisions[index];
    const auto& workload = plan.workload[index];
    const std::size_t expected_ordinal =
        inferlab::internal::checked_add_size(
            index, 1U, "advanced KV router validated decision ordinal");
    const std::string expected_key =
        trace_record_key(record.policy, expected_ordinal);
    const auto worker_index =
        decision.worker_id < 0
            ? worker_prefixes.size()
            : static_cast<std::size_t>(decision.worker_id);
    const bool expected_cache_hit =
        worker_index < worker_prefixes.size() &&
        workload.prefix_cache_eligible &&
        worker_prefixes[worker_index].find(workload.prefix_class) !=
            worker_prefixes[worker_index].end();
    if (decision.request_ordinal != expected_ordinal ||
        decision.trace_record_key != expected_key ||
        decision.worker_id < 0 ||
        decision.worker_id >= plan.worker_count ||
        decision.reused_tokens < 0 || decision.recomputed_tokens < 0 ||
        decision.worker_load_after < 0 ||
        decision.cache_hit != expected_cache_hit ||
        decision.reused_tokens !=
            (decision.cache_hit ? workload.prompt_tokens : 0) ||
        decision.recomputed_tokens !=
            (decision.cache_hit ? 0 : workload.prompt_tokens)) {
      throw std::invalid_argument(
          "advanced KV router decision projection is inconsistent");
    }

    if (workload.prefix_cache_eligible) {
      worker_prefixes[worker_index].insert(workload.prefix_class);
    }
    const int expected_load = inferlab::internal::checked_add_int(
        worker_loads[worker_index], workload.total_tokens,
        "advanced KV router worker load reconciliation");
    if (decision.worker_load_after != expected_load) {
      throw std::invalid_argument(
          "advanced KV router worker load history is inconsistent");
    }
    worker_loads[worker_index] = expected_load;
    cache_hits = inferlab::internal::checked_add_int(
        cache_hits, decision.cache_hit ? 1 : 0,
        "advanced KV router cache hit reconciliation");
    reused_tokens = inferlab::internal::checked_add_int(
        reused_tokens, decision.reused_tokens,
        "advanced KV router reused token reconciliation");
    recomputed_tokens = inferlab::internal::checked_add_int(
        recomputed_tokens, decision.recomputed_tokens,
        "advanced KV router recomputed token reconciliation");
  }

  const auto minmax =
      std::minmax_element(worker_loads.begin(), worker_loads.end());
  const int peak_worker_load = *minmax.second;
  const int worker_load_imbalance = inferlab::internal::checked_int_from_i64(
      static_cast<std::int64_t>(*minmax.second) -
          static_cast<std::int64_t>(*minmax.first),
      "advanced KV router worker load imbalance reconciliation");
  const double expected_hit_rate =
      static_cast<double>(cache_hits) /
      static_cast<double>(plan.workload.size());
  if (record.final_worker_loads != worker_loads ||
      record.cache_hits != cache_hits ||
      record.reused_tokens != reused_tokens ||
      record.recomputed_tokens != recomputed_tokens ||
      record.peak_worker_load != peak_worker_load ||
      record.worker_load_imbalance != worker_load_imbalance ||
      std::abs(record.prefix_hit_rate - expected_hit_rate) > 1e-12) {
    throw std::invalid_argument(
        "advanced KV router policy aggregate drift");
  }
}

}  // namespace

Plan build_plan(const int worker_count,
                const std::vector<AdvancedRequest>& requests,
                inferlab_native_trace::Source trace_source) {
  if (worker_count <= 0 ||
      worker_count > maximum_kv_aware_routing_workers()) {
    throw std::invalid_argument(
        "advanced KV router worker count is outside the supported range");
  }

  Plan plan;
  plan.worker_count = worker_count;
  plan.workload = project_workload(requests);
  const auto& policies = kv_aware_routing_policies();
  plan.records.reserve(policies.size());
  for (const auto& policy : policies) {
    plan.records.push_back(project_policy_record(
        simulate_kv_aware_routing(policy, worker_count, requests),
        requests));
  }
  plan.best_prefix_hit_record_index =
      best_prefix_hit_record_index(plan.records);
  plan.trace_context = inferlab_native_trace::bind_trace_context(
      inferlab_native_trace::Domain::kv_aware_routing,
      trace_record_keys(plan), std::move(trace_source));
  validate_plan(plan);
  return plan;
}

Plan build_plan(const int worker_count,
                const std::vector<AdvancedRequest>& requests) {
  return build_plan(worker_count, requests,
                    inferlab_native_trace::Source{});
}

void validate_plan(const Plan& plan) {
  const auto& policies = kv_aware_routing_policies();
  if (plan.worker_count <= 0 ||
      plan.worker_count > maximum_kv_aware_routing_workers() ||
      plan.workload.empty() ||
      plan.workload.size() > kMaximumRequestCount ||
      plan.records.size() != policies.size() ||
      plan.best_prefix_hit_record_index >= plan.records.size()) {
    throw std::invalid_argument(
        "advanced KV router artifact plan is incomplete");
  }
  for (std::size_t index = 0U; index < plan.workload.size(); ++index) {
    validate_workload_record(plan.workload[index], index);
  }
  for (std::size_t index = 0U; index < plan.records.size(); ++index) {
    validate_policy_record(plan, plan.records[index], index);
  }
  if (plan.best_prefix_hit_record_index !=
      best_prefix_hit_record_index(plan.records)) {
    throw std::invalid_argument(
        "advanced KV router best-prefix policy index drift");
  }
  inferlab_native_trace::validate_trace_context_alignment(
      plan.trace_context,
      inferlab_native_trace::Domain::kv_aware_routing,
      trace_record_keys(plan));
}

}  // namespace inferlab::advanced_kv_router_artifact
