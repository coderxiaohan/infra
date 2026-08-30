#include "inferlab/paged_attention.hpp"
#include "paged_kv_runtime.hpp"
#include "test_error_redaction.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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

std::vector<float> build_physical_cache(const int block_count,
                                        const inferlab::PagedKVShape& shape) {
  std::vector<float> cache(static_cast<std::size_t>(
      block_count * shape.num_heads * shape.block_size_tokens * shape.head_dim),
                           0.0F);
  for (int block = 0; block < block_count; ++block) {
    for (int head = 0; head < shape.num_heads; ++head) {
      for (int offset = 0; offset < shape.block_size_tokens; ++offset) {
        for (int dim = 0; dim < shape.head_dim; ++dim) {
          cache[physical_index(block, head, offset, dim, shape)] =
              static_cast<float>(block * 1000 + head * 100 + offset * 10 + dim);
        }
      }
    }
  }
  return cache;
}

void require_close(const std::vector<float>& actual,
                   const std::vector<float>& expected) {
  require(actual.size() == expected.size(), "CUDA paged KV output size mismatch");
  for (std::size_t i = 0; i < actual.size(); ++i) {
    require(std::fabs(actual[i] - expected[i]) < 1e-6F,
            "CUDA paged KV output mismatch");
  }
}

void test_cuda_gather_matches_cpu_for_fragmented_blocks() {
  const inferlab::PagedKVShape shape{3, 4, 5};
  inferlab::RequestBlocks blocks;
  blocks.request_id = 7;
  blocks.token_count = 11;
  blocks.block_table = {4, 1, 3};

  const auto physical_cache = build_physical_cache(5, shape);
  const auto expected =
      inferlab::gather_paged_kv_cache(physical_cache, blocks, shape);
  const auto profiled = inferlab::gather_paged_kv_cache_cuda_runtime_profiled(
      physical_cache, blocks, shape);
  require_close(profiled.output, expected);
  require(profiled.elapsed_ms >= 0.0F,
          "CUDA paged KV elapsed time invalid");
}

void test_cuda_gather_handles_empty_request() {
  const inferlab::PagedKVShape shape{2, 8, 4};
  inferlab::RequestBlocks blocks;
  blocks.request_id = 8;
  blocks.token_count = 0;
  blocks.block_table = {};

  const auto profiled =
      inferlab::gather_paged_kv_cache_cuda_runtime_profiled({}, blocks, shape);
  require(profiled.output.empty(),
          "empty paged KV gather should return empty output");
  require(profiled.elapsed_ms == 0.0F,
          "empty paged KV gather should report zero elapsed time");
}

void test_cuda_gather_matches_cpu_for_unaligned_shared_prefix() {
  const inferlab::PagedKVShape shape{3, 17, 4};
  inferlab::RequestBlocks blocks;
  blocks.request_id = 9;
  blocks.token_count = 10;
  blocks.shared_prefix_tokens = 3;
  blocks.private_token_count = 7;
  blocks.private_block_start = 2;
  blocks.block_table = {5, 1, 4, 0};

  const auto physical_cache = build_physical_cache(6, shape);
  const auto expected =
      inferlab::gather_paged_kv_cache(physical_cache, blocks, shape);
  const auto actual = inferlab::gather_paged_kv_cache_cuda_runtime(
      physical_cache, blocks, shape);

  require_close(actual, expected);
}

void test_cuda_gather_rejects_any_negative_block_id() {
  const inferlab::PagedKVShape shape{1, 2, 4};
  inferlab::RequestBlocks blocks;
  blocks.token_count = 5;
  blocks.block_table = {2, -1};

  bool threw = false;
  try {
    (void)inferlab::gather_paged_kv_cache_cuda_runtime(
        build_physical_cache(3, shape), blocks, shape);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "CUDA paged KV gather accepted a negative block id");
}

}  // namespace

int main() {
  try {
    test_cuda_gather_matches_cpu_for_fragmented_blocks();
    test_cuda_gather_handles_empty_request();
    test_cuda_gather_matches_cpu_for_unaligned_shared_prefix();
    test_cuda_gather_rejects_any_negative_block_id();
  } catch (const std::exception& error) {
    std::cerr << "paged_kv_cuda_test failed: " << inferlab_cuda_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
