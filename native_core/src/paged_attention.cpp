#include "inferlab/paged_attention.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

#include "checked_index.hpp"

namespace inferlab {
namespace {

struct LogicalBlockLocation {
  int logical_block = 0;
  int block_offset = 0;
};

void validate_shape(const PagedKVShape& shape) {
  if (shape.num_heads <= 0 || shape.head_dim <= 0 ||
      shape.block_size_tokens <= 0) {
    throw std::invalid_argument("paged KV shape dimensions must be positive");
  }
}

void validate_request_layout(const RequestBlocks& blocks,
                             const PagedKVShape& shape) {
  if (blocks.token_count < 0 || blocks.shared_prefix_tokens < 0 ||
      blocks.shared_prefix_tokens > blocks.token_count ||
      blocks.private_block_start < 0) {
    throw std::invalid_argument("paged KV request layout is invalid");
  }
  const int required_prefix_blocks =
      internal::checked_ceil_div_non_negative_int(
          blocks.shared_prefix_tokens, shape.block_size_tokens,
          "paged KV shared prefix blocks");
  if (required_prefix_blocks > blocks.private_block_start) {
    throw std::invalid_argument(
        "paged KV private block segment overlaps shared prefix blocks");
  }
  if (blocks.private_block_start >
      internal::checked_int_from_size(blocks.block_table.size(),
                                      "paged KV block table size")) {
    throw std::invalid_argument(
        "paged KV private block segment starts outside block table");
  }
}

LogicalBlockLocation logical_location(const RequestBlocks& blocks,
                                      const int token,
                                      const PagedKVShape& shape) {
  if (token < blocks.shared_prefix_tokens) {
    return LogicalBlockLocation{token / shape.block_size_tokens,
                                token % shape.block_size_tokens};
  }
  const int private_token = token - blocks.shared_prefix_tokens;
  return LogicalBlockLocation{
      internal::checked_add_int(
          blocks.private_block_start,
          private_token / shape.block_size_tokens,
          "paged KV private logical block"),
      private_token % shape.block_size_tokens};
}

std::size_t physical_index(const int block_id, const int head,
                           const int block_offset, const int dim,
                           const PagedKVShape& shape) {
  const auto block_head_token = internal::checked_flat_index3(
      block_id, shape.num_heads, head, shape.block_size_tokens, block_offset,
      "paged KV physical token");
  return internal::checked_flat_index_size(block_head_token, shape.head_dim, dim,
                                           "paged KV physical");
}

std::size_t logical_index(const int head, const int token, const int dim,
                          const int token_count,
                          const PagedKVShape& shape) {
  return internal::checked_flat_index3(head, token_count, token, shape.head_dim,
                                       dim, "paged KV logical");
}

}  // namespace

std::vector<float> gather_paged_kv_cache(
    const std::vector<float>& physical_cache, const RequestBlocks& blocks,
    const PagedKVShape& shape) {
  validate_shape(shape);
  validate_request_layout(blocks, shape);

  const auto block_elements = internal::checked_product_size(
      shape.num_heads, shape.block_size_tokens, shape.head_dim,
      "paged KV block elements");
  for (const auto block_id : blocks.block_table) {
    if (block_id < 0) {
      throw std::invalid_argument("block id must be non-negative");
    }
    const auto end = internal::checked_product_size(
        static_cast<std::size_t>(block_id) + 1U, block_elements,
        "paged KV block end");
    if (end > physical_cache.size()) {
      throw std::invalid_argument("physical cache too small for block table");
    }
  }

  std::vector<float> output(
      internal::checked_non_negative_product_size(
          shape.num_heads, blocks.token_count, shape.head_dim,
          "paged KV output"),
      0.0F);
  for (int token = 0; token < blocks.token_count; ++token) {
    const auto location = logical_location(blocks, token, shape);
    if (location.logical_block < 0 || location.logical_block >=
        internal::checked_int_from_size(blocks.block_table.size(),
                                        "paged attention block table size")) {
      throw std::invalid_argument("block table missing logical token block");
    }
    const int block_id =
        blocks.block_table[static_cast<std::size_t>(location.logical_block)];
    for (int head = 0; head < shape.num_heads; ++head) {
      for (int dim = 0; dim < shape.head_dim; ++dim) {
        const float value =
            physical_cache[physical_index(block_id, head, location.block_offset,
                                          dim, shape)];
        if (!std::isfinite(value)) {
          throw std::invalid_argument("paged KV gathered value must be finite");
        }
        output[logical_index(head, token, dim, blocks.token_count, shape)] =
            value;
      }
    }
  }
  return output;
}

}  // namespace inferlab
