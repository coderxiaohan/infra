#include "inferlab/static_kv_cache.hpp"
#include "test_error_redaction.hpp"

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

void test_static_allocation_reserves_max_sequence() {
  inferlab::StaticKVCacheManager manager({2, 16, 100});
  require(manager.add_request(1, 4), "request allocation failed");

  const auto stats = manager.stats();
  require(stats.used_bytes == 400, "used bytes mismatch");
  require(stats.reserved_bytes == 1600, "reserved bytes mismatch");
  require(stats.utilization == 0.25, "utilization mismatch");
}

void test_append_cannot_exceed_max_sequence() {
  inferlab::StaticKVCacheManager manager({1, 4, 10});
  require(manager.add_request(1, 3), "request allocation failed");
  require(!manager.append_tokens(1, 2), "append should exceed max sequence");
  require(manager.request(1)->token_count == 3, "failed append changed tokens");
}

void test_release_reuses_slot_capacity() {
  inferlab::StaticKVCacheManager manager({1, 4, 10});
  require(manager.add_request(1, 1), "request 1 allocation failed");
  require(!manager.add_request(2, 1), "second concurrent request should fail");
  require(manager.release_request(1), "request release failed");
  require(manager.add_request(2, 1), "slot should be reused");
}

}  // namespace

int main() {
  try {
    test_static_allocation_reserves_max_sequence();
    test_append_cannot_exceed_max_sequence();
    test_release_reuses_slot_capacity();
  } catch (const std::exception& error) {
    std::cerr << "static_kv_cache_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
