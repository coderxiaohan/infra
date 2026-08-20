#pragma once

#include "inferlab/paged_kv_locality.hpp"

namespace inferlab::paged_kv_locality_probe {

struct Request {
  int token_count = 9;
  PagedKVShape shape{4, 8, 4};
};

struct Plan {
  Request request;
  RequestBlocks contiguous;
  BlockTableLocalityMetrics contiguous_metrics;
  RequestBlocks fragmented;
  BlockTableLocalityMetrics fragmented_metrics;
  double fragmented_vs_contiguous_span_amplification_delta = 0.0;
};

Plan build_plan(const Request& request);

void validate_plan(const Plan& plan);

}  // namespace inferlab::paged_kv_locality_probe
