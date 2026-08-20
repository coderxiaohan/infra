#include "inferlab/vector_add.hpp"
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

void test_vector_add_cpu_matches_reference() {
  const std::vector<float> a = {1.0F, -2.0F, 3.5F};
  const std::vector<float> b = {0.5F, 4.0F, -1.5F};
  const auto output = inferlab::vector_add_cpu(a, b);
  require(output == std::vector<float>({1.5F, 2.0F, 2.0F}),
          "vector add output mismatch");
}

void test_vector_add_cpu_rejects_shape_mismatch() {
  bool threw = false;
  try {
    (void)inferlab::vector_add_cpu({1.0F}, {1.0F, 2.0F});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "shape mismatch did not throw");
}

}  // namespace

int main() {
  try {
    test_vector_add_cpu_matches_reference();
    test_vector_add_cpu_rejects_shape_mismatch();
  } catch (const std::exception& error) {
    std::cerr << "vector_add_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
