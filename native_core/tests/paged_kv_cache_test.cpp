#include "inferlab/paged_kv_cache.hpp"
#include "test_error_redaction.hpp"

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

void test_request_allocates_expected_blocks() {
  inferlab::PagedKVCacheManager manager({4, 16, 128});
  require(manager.add_request(7, 17), "request allocation failed");
  const auto request = manager.request_blocks(7);
  require(request.has_value(), "request not found");
  require(request->block_table.size() == 2, "expected 2 blocks for 17 tokens");

  const auto stats = manager.stats();
  require(stats.used_blocks == 2, "used block count mismatch");
  require(stats.free_blocks == 2, "free block count mismatch");
  require(stats.total_tokens == 17, "total token count mismatch");
}

void test_append_crosses_block_boundary() {
  inferlab::PagedKVCacheManager manager({3, 4, 10});
  require(manager.add_request(1, 4), "initial allocation failed");
  require(manager.append_tokens(1, 1), "append allocation failed");

  const auto request = manager.request_blocks(1);
  require(request.has_value(), "request not found after append");
  require(request->token_count == 5, "token count mismatch after append");
  require(request->block_table.size() == 2, "append should allocate second block");
}

void test_capacity_failure_is_non_destructive() {
  inferlab::PagedKVCacheManager manager({1, 4, 10});
  require(manager.add_request(1, 4), "initial allocation failed");
  require(!manager.append_tokens(1, 1), "append should fail without free blocks");

  const auto request = manager.request_blocks(1);
  require(request.has_value(), "request missing after failed append");
  require(request->token_count == 4, "failed append changed token count");
  require(request->block_table.size() == 1, "failed append changed block table");
}

void test_release_reuses_blocks() {
  inferlab::PagedKVCacheManager manager({2, 8, 64});
  require(manager.add_request(1, 8), "request 1 allocation failed");
  const auto first = manager.request_blocks(1)->block_table;
  require(manager.release_request(1), "request release failed");
  require(manager.add_request(2, 8), "request 2 allocation failed");
  const auto second = manager.request_blocks(2)->block_table;
  require(first == second, "released block was not reused deterministically");
}

void test_utilization_accounts_for_partially_filled_blocks() {
  inferlab::PagedKVCacheManager manager({2, 8, 100});
  require(manager.add_request(1, 9), "request allocation failed");
  const auto stats = manager.stats();
  require(stats.used_bytes == 900, "used bytes mismatch");
  require(stats.reserved_bytes == 1600, "reserved bytes mismatch");
  require(stats.utilization > 0.56 && stats.utilization < 0.57,
          "utilization mismatch");
}

void test_prefix_blocks_are_shared_across_requests() {
  inferlab::PagedKVCacheManager manager({8, 4, 100});
  require(manager.create_prefix(11, 6), "prefix creation failed");
  require(manager.add_request_with_prefix(1, 11, 2), "request 1 failed");
  require(manager.add_request_with_prefix(2, 11, 5), "request 2 failed");

  const auto prefix = manager.prefix_entry(11);
  require(prefix.has_value(), "prefix missing");
  require(prefix->ref_count == 2, "prefix refcount mismatch");

  const auto request1 = manager.request_blocks(1);
  const auto request2 = manager.request_blocks(2);
  require(request1.has_value() && request2.has_value(), "requests missing");
  require(request1->block_table[0] == prefix->block_table[0],
          "request 1 does not share prefix block 0");
  require(request2->block_table[1] == prefix->block_table[1],
          "request 2 does not share prefix block 1");

  const auto stats = manager.stats();
  require(stats.total_tokens == 13,
          "shared prefix should be counted once in physical token stats");
}

void test_prefix_released_after_last_reference() {
  inferlab::PagedKVCacheManager manager({4, 4, 100});
  require(manager.create_prefix(11, 4), "prefix creation failed");
  require(manager.add_request_with_prefix(1, 11, 1), "request 1 failed");
  require(manager.add_request_with_prefix(2, 11, 1), "request 2 failed");

  require(manager.release_request(1), "release request 1 failed");
  require(manager.prefix_entry(11).has_value(),
          "prefix released while still referenced");
  require(manager.stats().used_blocks == 2,
          "only request 1 private block should be released");

  require(manager.release_request(2), "release request 2 failed");
  require(!manager.prefix_entry(11).has_value(),
          "prefix should release after last reference");
  require(manager.stats().used_blocks == 0, "all blocks should be free");
}

void test_locate_token_maps_logical_position_to_physical_block() {
  inferlab::PagedKVCacheManager manager({4, 4, 100});
  require(manager.add_request(1, 6), "request allocation failed");

  const auto request = manager.request_blocks(1);
  const auto location0 = manager.locate_token(1, 0);
  const auto location5 = manager.locate_token(1, 5);
  require(location0.has_value(), "token 0 missing");
  require(location5.has_value(), "token 5 missing");
  require(location0->block_id == request->block_table[0],
          "token 0 block mismatch");
  require(location0->block_offset == 0, "token 0 offset mismatch");
  require(location5->block_id == request->block_table[1],
          "token 5 block mismatch");
  require(location5->block_offset == 1, "token 5 offset mismatch");
  require(!manager.locate_token(1, 6).has_value(),
          "out-of-range token should not locate");
}

void test_locate_token_with_shared_prefix() {
  inferlab::PagedKVCacheManager manager({8, 4, 100});
  require(manager.create_prefix(11, 6), "prefix creation failed");
  require(manager.add_request_with_prefix(1, 11, 3), "request failed");

  const auto prefix = manager.prefix_entry(11);
  const auto prefix_location = manager.locate_token(1, 5);
  const auto suffix_location = manager.locate_token(1, 8);
  require(prefix_location.has_value(), "prefix token missing");
  require(suffix_location.has_value(), "suffix token missing");
  require(prefix_location->block_id == prefix->block_table[1],
          "prefix token block mismatch");
  require(prefix_location->block_offset == 1, "prefix token offset mismatch");
  require(suffix_location->block_offset == 2, "suffix token offset mismatch");
}

void test_prefix_cache_traffic_profile_counts_savings() {
  const auto profile = inferlab::profile_prefix_cache_traffic(4, 16, 8, 8, 128);

  require(profile.total_tokens_per_request == 24,
          "profile total tokens per request mismatch");
  require(profile.prefix_blocks == 2, "profile prefix blocks mismatch");
  require(profile.suffix_blocks_per_request == 1,
          "profile suffix blocks mismatch");
  require(profile.no_prefix_blocks_per_request == 3,
          "profile no-prefix blocks mismatch");
  require(profile.no_prefix_physical_tokens == 96,
          "profile no-prefix physical tokens mismatch");
  require(profile.shared_prefix_physical_tokens == 48,
          "profile shared physical tokens mismatch");
  require(profile.physical_tokens_saved == 48,
          "profile physical tokens saved mismatch");
  require(profile.no_prefix_prefill_write_bytes == 12288,
          "profile no-prefix prefill bytes mismatch");
  require(profile.shared_prefix_prefill_write_bytes == 6144,
          "profile shared-prefix prefill bytes mismatch");
  require(profile.prefix_write_bytes_saved == 6144,
          "profile prefix write bytes saved mismatch");
  require(profile.no_prefix_reserved_bytes == 12288,
          "profile no-prefix reserved bytes mismatch");
  require(profile.shared_prefix_reserved_bytes == 6144,
          "profile shared-prefix reserved bytes mismatch");
  require(profile.reserved_bytes_saved == 6144,
          "profile reserved bytes saved mismatch");
  require(profile.no_prefix_request_block_table_entries == 12,
          "profile no-prefix block table entries mismatch");
  require(profile.shared_request_block_table_entries == 12,
          "profile shared request block table entries mismatch");
  require(profile.shared_prefix_entry_block_table_entries == 2,
          "profile shared prefix entry table entries mismatch");
  require(profile.decode_attention_kv_read_bytes_per_step == 12288,
          "profile decode attention read bytes mismatch");
  require(profile.decode_attention_shared_prefix_read_bytes_per_step == 8192,
          "profile shared-prefix decode read bytes mismatch");
}

}  // namespace

int main() {
  try {
    test_request_allocates_expected_blocks();
    test_append_crosses_block_boundary();
    test_capacity_failure_is_non_destructive();
    test_release_reuses_blocks();
    test_utilization_accounts_for_partially_filled_blocks();
    test_prefix_blocks_are_shared_across_requests();
    test_prefix_released_after_last_reference();
    test_locate_token_maps_logical_position_to_physical_block();
    test_locate_token_with_shared_prefix();
    test_prefix_cache_traffic_profile_counts_savings();
  } catch (const std::exception& error) {
    std::cerr << "paged_kv_cache_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
