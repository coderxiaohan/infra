#include "inferlab/advanced_serving_scheduler.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "advanced_serving_internal.hpp"
#include "checked_index.hpp"

namespace inferlab {
namespace {

int checked_size_to_int(const std::size_t value, const char* name) {
  return internal::checked_int_from_size(value, name);
}

int checked_non_negative_token_sum(const int prompt_tokens,
                                   const int output_tokens,
                                   const char* name) {
  if (prompt_tokens < 0 || output_tokens < 0) {
    throw std::invalid_argument(
        "routing token counts must be non-negative");
  }
  return internal::checked_add_int(prompt_tokens, output_tokens, name);
}

int choose_least_loaded(const std::vector<int>& loads) {
  if (loads.empty()) {
    throw std::invalid_argument("routing worker loads must be nonempty");
  }
  return checked_size_to_int(
      static_cast<std::size_t>(std::min_element(loads.begin(), loads.end()) -
                               loads.begin()),
      "least loaded worker index");
}

int choose_prefix_hit_worker(const std::vector<std::set<std::string>>& caches,
                             const std::vector<int>& loads,
                             const std::string& prefix_id) {
  if (caches.size() != loads.size()) {
    throw std::invalid_argument(
        "routing cache and worker-load cardinalities must match");
  }
  if (prefix_id.empty()) {
    return -1;
  }
  int best = -1;
  int best_load = std::numeric_limits<int>::max();
  for (std::size_t i = 0; i < caches.size(); ++i) {
    if (caches[i].find(prefix_id) == caches[i].end()) {
      continue;
    }
    if (loads[i] < best_load) {
      best = checked_size_to_int(i, "prefix hit worker index");
      best_load = loads[i];
    }
  }
  return best;
}

int choose_worker(const std::string& policy, const int request_index,
                  const AdvancedRequest& request,
                  const std::vector<std::set<std::string>>& caches,
                  const std::vector<int>& loads) {
  if (caches.size() != loads.size() || loads.empty()) {
    throw std::invalid_argument(
        "routing worker state must be nonempty and cardinality-aligned");
  }
  if (policy == "round_robin") {
    return request_index % checked_size_to_int(loads.size(), "worker load size");
  }
  if (policy == "least_loaded") {
    return choose_least_loaded(loads);
  }
  if (policy == "prefix_hit_aware") {
    const int hit_worker = request.prompt_tokens > 0
                               ? choose_prefix_hit_worker(
                                     caches, loads, request.prefix_id)
                               : -1;
    return hit_worker >= 0 ? hit_worker : choose_least_loaded(loads);
  }
  if (policy == "hybrid") {
    const int hit_worker = request.prompt_tokens > 0
                               ? choose_prefix_hit_worker(
                                     caches, loads, request.prefix_id)
                               : -1;
    const int least_loaded = choose_least_loaded(loads);
    if (hit_worker < 0) {
      return least_loaded;
    }
    const int allowed_skew = request.prompt_tokens / 2;
    const int skewed_least_loaded = internal::checked_add_int(
        loads[static_cast<std::size_t>(least_loaded)], allowed_skew,
        "hybrid worker load skew");
    return loads[static_cast<std::size_t>(hit_worker)] <= skewed_least_loaded
               ? hit_worker
               : least_loaded;
  }
  throw std::invalid_argument("unknown routing policy: " + policy);
}

}  // namespace

std::vector<AdvancedRequest> default_advanced_requests() {
  return {
      {1, "chat-system-a", 512, 128},
      {2, "chat-system-b", 384, 128},
      {3, "chat-system-a", 768, 256},
      {4, "tool-schema-order", 1024, 128},
      {5, "chat-system-c", 256, 64},
      {6, "chat-system-a", 512, 512},
      {7, "tool-schema-order", 1536, 256},
      {8, "chat-system-b", 384, 64},
      {9, "long-rag-prefix", 4096, 256},
      {10, "chat-system-a", 512, 64},
      {11, "long-rag-prefix", 4096, 512},
      {12, "chat-system-d", 640, 128},
  };
}

const std::vector<std::string>& kv_aware_routing_policies() {
  static const std::vector<std::string> policies = [] {
    std::vector<std::string> values = {
        "round_robin", "least_loaded", "prefix_hit_aware", "hybrid"};
    advanced_serving_internal::require_unique_nonempty_strings(
        values, "KV-aware routing policies");
    return values;
  }();
  return policies;
}

int maximum_kv_aware_routing_workers() {
  return 4096;
}

RoutingSummary simulate_kv_aware_routing(
    const std::string& policy, const int worker_count,
    const std::vector<AdvancedRequest>& requests) {
  const auto& policies = kv_aware_routing_policies();
  if (std::find(policies.begin(), policies.end(), policy) ==
      policies.end()) {
    throw std::invalid_argument("unknown routing policy: " + policy);
  }
  if (worker_count < 0 ||
      worker_count > maximum_kv_aware_routing_workers()) {
    throw std::invalid_argument(
        "routing worker count is outside the supported range");
  }
  const int workers = worker_count;
  if (workers == 0 && !requests.empty()) {
    throw std::invalid_argument(
        "routing worker count must be positive when requests exist");
  }
  const auto worker_slots = static_cast<std::size_t>(workers);
  std::vector<std::set<std::string>> caches(worker_slots);
  std::vector<int> loads(worker_slots, 0);
  RoutingSummary summary;
  summary.policy = policy;
  summary.worker_count = workers;
  summary.request_count = checked_size_to_int(requests.size(), "routing requests");
  summary.decisions.reserve(requests.size());

  for (std::size_t i = 0; i < requests.size(); ++i) {
    const auto& request = requests[i];
    if (request.prompt_tokens < 0 || request.output_tokens < 0) {
      throw std::invalid_argument(
          "routing request token counts must be non-negative");
    }
    const int worker = choose_worker(
        policy, checked_size_to_int(i, "request index"), request, caches, loads);
    const auto worker_index = static_cast<std::size_t>(worker);
    const int prompt_tokens = request.prompt_tokens;
    const int output_tokens = request.output_tokens;
    const bool cacheable = prompt_tokens > 0 && !request.prefix_id.empty();
    const bool cache_hit =
        cacheable && caches[worker_index].find(request.prefix_id) !=
                         caches[worker_index].end();
    const int reused_tokens = cache_hit ? prompt_tokens : 0;
    const int recomputed_tokens = cache_hit ? 0 : prompt_tokens;
    if (cacheable) {
      caches[worker_index].insert(request.prefix_id);
    }
    loads[worker_index] = internal::checked_add_int(
        loads[worker_index],
        checked_non_negative_token_sum(prompt_tokens, output_tokens,
                                       "routing request tokens"),
        "routing worker load");

    summary.cache_hits = internal::checked_add_int(
        summary.cache_hits, cache_hit ? 1 : 0, "routing cache hits");
    summary.reused_tokens = internal::checked_add_int(
        summary.reused_tokens, reused_tokens, "routing reused tokens");
    summary.recomputed_tokens = internal::checked_add_int(
        summary.recomputed_tokens, recomputed_tokens,
        "routing recomputed tokens");
    summary.decisions.push_back(
        {request.request_id, policy, worker, cache_hit, reused_tokens,
         recomputed_tokens, loads[worker_index]});
  }

  if (!loads.empty()) {
    const auto minmax = std::minmax_element(loads.begin(), loads.end());
    const auto load_imbalance = static_cast<long long>(*minmax.second) -
                                static_cast<long long>(*minmax.first);
    if (load_imbalance >
        static_cast<long long>(std::numeric_limits<int>::max())) {
      throw std::invalid_argument(
          "routing worker load imbalance exceeds int range");
    }
    summary.worker_load_imbalance = static_cast<int>(load_imbalance);
    summary.peak_worker_load = *minmax.second;
  }
  summary.prefix_hit_rate =
      requests.empty()
          ? 0.0
          : static_cast<double>(summary.cache_hits) /
                static_cast<double>(requests.size());
  summary.final_worker_loads = std::move(loads);
  return summary;
}

}  // namespace inferlab
