#include "inferlab/paged_kv_locality.hpp"
#include "test_error_redaction.hpp"

#include <cmath>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void test_contiguous_blocks_have_no_span_amplification() {
  inferlab::RequestBlocks blocks;
  blocks.token_count = 8;
  blocks.block_table = {2, 3};
  const inferlab::PagedKVShape shape{2, 4, 4};

  const auto metrics = inferlab::analyze_block_table_locality(blocks, shape);

  require(metrics.logical_blocks == 2, "logical block count mismatch");
  require(metrics.unique_physical_blocks == 2, "unique block count mismatch");
  require(metrics.physical_span_blocks == 2, "span block count mismatch");
  require(metrics.contiguous_forward_transitions == 1,
          "contiguous transition mismatch");
  require(metrics.non_contiguous_transitions == 0,
          "non-contiguous transition mismatch");
  require(std::fabs(metrics.block_element_amplification - 1.0) < 1e-6,
          "block amplification mismatch");
  require(std::fabs(metrics.span_element_amplification - 1.0) < 1e-6,
          "span amplification mismatch");
}

void test_fragmented_blocks_increase_span_amplification() {
  inferlab::RequestBlocks blocks;
  blocks.token_count = 8;
  blocks.block_table = {1, 7};
  const inferlab::PagedKVShape shape{1, 2, 4};

  const auto metrics = inferlab::analyze_block_table_locality(blocks, shape);

  require(metrics.logical_blocks == 2, "logical block count mismatch");
  require(metrics.unique_physical_blocks == 2, "unique block count mismatch");
  require(metrics.physical_span_blocks == 7, "span block count mismatch");
  require(metrics.non_contiguous_transitions == 1,
          "non-contiguous transition mismatch");
  require(metrics.max_physical_jump == 6, "max jump mismatch");
  require(std::fabs(metrics.block_element_amplification - 1.0) < 1e-6,
          "block amplification mismatch");
  require(std::fabs(metrics.span_element_amplification - 3.5) < 1e-6,
          "span amplification mismatch");
}

void test_partial_tail_block_has_block_amplification() {
  inferlab::RequestBlocks blocks;
  blocks.token_count = 5;
  blocks.block_table = {0, 1};
  const inferlab::PagedKVShape shape{1, 2, 4};

  const auto metrics = inferlab::analyze_block_table_locality(blocks, shape);

  require(metrics.logical_element_reads == 10, "logical read mismatch");
  require(metrics.touched_block_elements == 16, "touched block mismatch");
  require(std::fabs(metrics.block_element_amplification - 1.6) < 1e-6,
          "partial tail amplification mismatch");
}

void test_missing_block_table_entry_throws() {
  bool threw = false;
  try {
    inferlab::RequestBlocks blocks;
    blocks.token_count = 5;
    blocks.block_table = {0};
    const inferlab::PagedKVShape shape{1, 2, 4};
    (void)inferlab::analyze_block_table_locality(blocks, shape);
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "missing block table entry did not throw");
}

}  // namespace

int main() {
  try {
    test_contiguous_blocks_have_no_span_amplification();
    test_fragmented_blocks_increase_span_amplification();
    test_partial_tail_block_has_block_amplification();
    test_missing_block_table_entry_throws();
  } catch (const std::exception& error) {
    std::cerr << "paged_kv_locality_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
