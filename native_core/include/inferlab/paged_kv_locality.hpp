#pragma once

#include <cstdint>

#include "inferlab/paged_attention.hpp"
#include "inferlab/paged_kv_cache.hpp"

namespace inferlab {

struct BlockTableLocalityMetrics {
  int token_count = 0;
  int logical_blocks = 0;
  int unique_physical_blocks = 0;
  int physical_span_blocks = 0;
  int transitions = 0;
  int contiguous_forward_transitions = 0;
  int non_contiguous_transitions = 0;
  int backward_transitions = 0;
  int max_physical_jump = 0;
  double average_physical_jump = 0.0;
  std::int64_t logical_element_reads = 0;
  std::int64_t touched_block_elements = 0;
  std::int64_t physical_span_elements = 0;
  double block_element_amplification = 0.0;
  double span_element_amplification = 0.0;
};

BlockTableLocalityMetrics analyze_block_table_locality(
    const RequestBlocks& blocks, const PagedKVShape& shape);

}  // namespace inferlab
