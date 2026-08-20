#include "inferlab/shape_trace.hpp"
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

void test_decode_grows_kv_length() {
  const auto trace = inferlab::build_trace(inferlab::TransformerShapeConfig{
      1,
      8,
      3,
      16,
      2,
      4,
      4,
      inferlab::DType::kFloat16,
  });
  require(trace.decode.size() == 3, "decode size mismatch");
  require(trace.decode[0].kv_sequence_length == 9, "step 1 kv length mismatch");
  require(trace.decode[1].kv_sequence_length == 10, "step 2 kv length mismatch");
  require(trace.decode[2].kv_sequence_length == 11, "step 3 kv length mismatch");
}

void test_hidden_size_validation() {
  bool threw = false;
  try {
    (void)inferlab::build_trace(inferlab::TransformerShapeConfig{
        1,
        8,
        1,
        15,
        1,
        4,
        4,
        inferlab::DType::kFloat16,
    });
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "hidden size validation did not throw");
}

void test_json_contains_prefill_and_decode() {
  const auto trace = inferlab::build_trace(inferlab::TransformerShapeConfig{});
  const auto json = inferlab::to_json(trace);
  require(json.find("\"prefill\"") != std::string::npos, "missing prefill JSON");
  require(json.find("\"decode\"") != std::string::npos, "missing decode JSON");
  require(json.find("\"kv_cache\"") != std::string::npos,
          "missing kv_cache JSON");
}

}  // namespace

int main() {
  try {
    test_decode_grows_kv_length();
    test_hidden_size_validation();
    test_json_contains_prefill_and_decode();
  } catch (const std::exception& error) {
    std::cerr << "shape_trace_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
