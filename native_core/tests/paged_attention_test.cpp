#include "inferlab/paged_attention.hpp"
#include "test_error_redaction.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "inferlab/attention.hpp"

namespace {

void require(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::size_t physical_index(const int block_id, const int head,
                           const int block_offset, const int dim,
                           const inferlab::PagedKVShape& shape) {
  return static_cast<std::size_t>(
      ((block_id * shape.num_heads + head) * shape.block_size_tokens +
       block_offset) *
          shape.head_dim +
      dim);
}

std::vector<float> build_physical_cache(const int num_blocks,
                                        const inferlab::PagedKVShape& shape) {
  std::vector<float> cache(static_cast<std::size_t>(
      num_blocks * shape.num_heads * shape.block_size_tokens * shape.head_dim),
                           0.0F);
  for (int block = 0; block < num_blocks; ++block) {
    for (int head = 0; head < shape.num_heads; ++head) {
      for (int offset = 0; offset < shape.block_size_tokens; ++offset) {
        for (int dim = 0; dim < shape.head_dim; ++dim) {
          cache[physical_index(block, head, offset, dim, shape)] =
              static_cast<float>(block * 100 + head * 20 + offset * 3 + dim);
        }
      }
    }
  }
  return cache;
}

void test_gather_follows_block_table_order() {
  const inferlab::PagedKVShape shape{2, 2, 2};
  inferlab::RequestBlocks blocks;
  blocks.request_id = 1;
  blocks.token_count = 3;
  blocks.block_table = {2, 0};
  const auto physical = build_physical_cache(3, shape);
  const auto gathered = inferlab::gather_paged_kv_cache(physical, blocks, shape);

  require(gathered.size() == 12, "gathered cache size mismatch");
  require(std::fabs(gathered[0] - 200.0F) < 1e-6F,
          "token 0 should come from block 2 offset 0");
  require(std::fabs(gathered[2] - 203.0F) < 1e-6F,
          "token 1 should come from block 2 offset 1");
  require(std::fabs(gathered[4] - 0.0F) < 1e-6F,
          "token 2 should come from block 0 offset 0");
}

void test_gathered_cache_matches_contiguous_attention() {
  const inferlab::PagedKVShape shape{1, 2, 2};
  inferlab::RequestBlocks blocks;
  blocks.request_id = 1;
  blocks.token_count = 3;
  blocks.block_table = {1, 0};
  std::vector<float> physical(static_cast<std::size_t>(2 * 1 * 2 * 2), 0.0F);
  const std::vector<float> contiguous_key = {
      1.0F, 0.0F,
      0.0F, 1.0F,
      1.0F, 1.0F,
  };
  const std::vector<float> contiguous_value = {
      10.0F, 0.0F,
      0.0F, 20.0F,
      30.0F, 40.0F,
  };

  const std::vector<int> logical_blocks = {1, 1, 0};
  const std::vector<int> block_offsets = {0, 1, 0};
  for (int token = 0; token < 3; ++token) {
    for (int dim = 0; dim < 2; ++dim) {
      physical[physical_index(logical_blocks[static_cast<std::size_t>(token)],
                              0, block_offsets[static_cast<std::size_t>(token)],
                              dim, shape)] =
          contiguous_key[static_cast<std::size_t>(token * 2 + dim)];
    }
  }
  const auto gathered_key =
      inferlab::gather_paged_kv_cache(physical, blocks, shape);

  std::fill(physical.begin(), physical.end(), 0.0F);
  for (int token = 0; token < 3; ++token) {
    for (int dim = 0; dim < 2; ++dim) {
      physical[physical_index(logical_blocks[static_cast<std::size_t>(token)],
                              0, block_offsets[static_cast<std::size_t>(token)],
                              dim, shape)] =
          contiguous_value[static_cast<std::size_t>(token * 2 + dim)];
    }
  }
  const auto gathered_value =
      inferlab::gather_paged_kv_cache(physical, blocks, shape);

  const std::vector<float> query = {1.0F, 1.0F};
  const inferlab::AttentionShape attention_shape{1, 1, 3, 2};
  const auto contiguous = inferlab::scaled_dot_product_attention_cpu(
      query, contiguous_key, contiguous_value, attention_shape, false);
  const auto paged = inferlab::scaled_dot_product_attention_cpu(
      query, gathered_key, gathered_value, attention_shape, false);

  require(contiguous.size() == paged.size(), "attention output size mismatch");
  for (std::size_t i = 0; i < contiguous.size(); ++i) {
    require(std::fabs(contiguous[i] - paged[i]) < 1e-6F,
            "paged gathered attention output mismatch");
  }
}

void test_gather_rejects_missing_physical_block() {
  bool threw = false;
  try {
    const inferlab::PagedKVShape shape{1, 2, 2};
    inferlab::RequestBlocks blocks;
    blocks.token_count = 1;
    blocks.block_table = {3};
    (void)inferlab::gather_paged_kv_cache(std::vector<float>(4, 0.0F), blocks,
                                          shape);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "missing physical block did not throw");
}

}  // namespace

int main() {
  try {
    test_gather_follows_block_table_order();
    test_gathered_cache_matches_contiguous_attention();
    test_gather_rejects_missing_physical_block();
  } catch (const std::exception& error) {
    std::cerr << "paged_attention_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
