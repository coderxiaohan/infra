#include "inferlab/paged_kv_locality.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>

#include "checked_index.hpp"

namespace inferlab {
namespace {

int required_logical_blocks(const RequestBlocks& blocks,
                            const PagedKVShape& shape) {
  return internal::checked_ceil_div_non_negative_int(
      blocks.token_count, shape.block_size_tokens,
      "paged KV locality required blocks");
}

void validate_inputs(const RequestBlocks& blocks, const PagedKVShape& shape) {
  if (blocks.token_count < 0) {
    throw std::invalid_argument("token count must be non-negative");
  }
  if (shape.num_heads <= 0 || shape.head_dim <= 0 ||
      shape.block_size_tokens <= 0) {
    throw std::invalid_argument("paged KV shape dimensions must be positive");
  }
  const int required_blocks = required_logical_blocks(blocks, shape);
  if (required_blocks >
      internal::checked_int_from_size(blocks.block_table.size(),
                                      "paged KV block table size")) {
    throw std::invalid_argument("block table missing logical token block");
  }
  for (int i = 0; i < required_blocks; ++i) {
    if (blocks.block_table[static_cast<std::size_t>(i)] < 0) {
      throw std::invalid_argument("block id must be non-negative");
    }
  }
}

}  // namespace

BlockTableLocalityMetrics analyze_block_table_locality(
    const RequestBlocks& blocks, const PagedKVShape& shape) {
  validate_inputs(blocks, shape);

  BlockTableLocalityMetrics metrics;
  metrics.token_count = blocks.token_count;
  metrics.logical_blocks = required_logical_blocks(blocks, shape);
  metrics.logical_element_reads = internal::checked_product_i64(
      blocks.token_count, shape.num_heads, shape.head_dim,
      "paged KV locality logical element reads");

  if (metrics.logical_blocks == 0) {
    return metrics;
  }

  std::set<BlockId> unique_blocks;
  int min_block = blocks.block_table[0];
  int max_block = blocks.block_table[0];
  std::int64_t total_jump = 0;
  for (int i = 0; i < metrics.logical_blocks; ++i) {
    const auto block = blocks.block_table[static_cast<std::size_t>(i)];
    unique_blocks.insert(block);
    min_block = std::min(min_block, block);
    max_block = std::max(max_block, block);
    if (i == 0) {
      continue;
    }
    const auto previous = blocks.block_table[static_cast<std::size_t>(i - 1)];
    const auto jump =
        static_cast<long long>(block) - static_cast<long long>(previous);
    const auto abs_jump = jump < 0 ? -jump : jump;
    if (abs_jump > static_cast<long long>(std::numeric_limits<int>::max())) {
      throw std::invalid_argument("paged KV physical jump exceeds int range");
    }
    total_jump =
        internal::checked_add_i64(total_jump, abs_jump, "paged KV total jump");
    const auto abs_jump_int = static_cast<int>(abs_jump);
    metrics.max_physical_jump =
        std::max(metrics.max_physical_jump, abs_jump_int);
    ++metrics.transitions;
    if (jump == 1) {
      ++metrics.contiguous_forward_transitions;
    } else {
      ++metrics.non_contiguous_transitions;
    }
    if (jump < 0) {
      ++metrics.backward_transitions;
    }
  }

  metrics.unique_physical_blocks = internal::checked_int_from_size(
      unique_blocks.size(), "paged KV unique physical blocks");
  const auto physical_span =
      static_cast<long long>(max_block) - static_cast<long long>(min_block) + 1LL;
  if (physical_span > static_cast<long long>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument("paged KV physical span exceeds int range");
  }
  metrics.physical_span_blocks = static_cast<int>(physical_span);
  if (metrics.transitions > 0) {
    metrics.average_physical_jump =
        static_cast<double>(total_jump) / metrics.transitions;
  }

  const auto elements_per_block = internal::checked_product_i64(
      shape.num_heads, shape.block_size_tokens, shape.head_dim,
      "paged KV locality elements per block");
  metrics.touched_block_elements =
      internal::checked_product_i64(metrics.unique_physical_blocks,
                                    elements_per_block,
                                    "paged KV touched block elements");
  metrics.physical_span_elements =
      internal::checked_product_i64(metrics.physical_span_blocks,
                                    elements_per_block,
                                    "paged KV span block elements");
  if (metrics.logical_element_reads > 0) {
    metrics.block_element_amplification =
        static_cast<double>(metrics.touched_block_elements) /
        metrics.logical_element_reads;
    metrics.span_element_amplification =
        static_cast<double>(metrics.physical_span_elements) /
        metrics.logical_element_reads;
  }
  return metrics;
}

}  // namespace inferlab
