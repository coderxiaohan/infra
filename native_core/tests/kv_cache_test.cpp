#include "inferlab/kv_cache.hpp"
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

void test_estimate_matches_formula() {
  const auto estimate = inferlab::estimate_kv_cache(inferlab::KVCacheConfig{
      2,
      4,
      3,
      5,
      8,
      inferlab::DType::kFloat16,
  });
  const std::int64_t expected = 3LL * 2 * 2 * 5 * 4 * 8 * 2;
  require(estimate.total_bytes == expected, "total bytes formula mismatch");
  require(estimate.key_shape == std::vector<int>({2, 5, 4, 8}),
          "key shape mismatch");
  require(estimate.value_shape == std::vector<int>({2, 5, 4, 8}),
          "value shape mismatch");
}

void test_invalid_config_throws() {
  bool threw = false;
  try {
    (void)inferlab::estimate_kv_cache(inferlab::KVCacheConfig{
        0,
        1,
        1,
        1,
        1,
        inferlab::DType::kFloat16,
    });
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "invalid config did not throw");
}

void test_dtype_roundtrip() {
  require(inferlab::parse_dtype("fp16") == inferlab::DType::kFloat16,
          "fp16 parse mismatch");
  require(inferlab::dtype_name(inferlab::DType::kInt4) == "int4",
          "int4 name mismatch");
}

}  // namespace

int main() {
  try {
    test_estimate_matches_formula();
    test_invalid_config_throws();
    test_dtype_roundtrip();
  } catch (const std::exception& error) {
    std::cerr << "kv_cache_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
