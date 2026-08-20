#include "inferlab/attention.hpp"
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

void test_attention_prefers_matching_key() {
  const inferlab::AttentionShape shape{1, 1, 2, 2};
  const std::vector<float> query = {1.0F, 0.0F};
  const std::vector<float> key = {
      1.0F, 0.0F,
      0.0F, 1.0F,
  };
  const std::vector<float> value = {
      10.0F, 0.0F,
      0.0F, 20.0F,
  };
  const auto output =
      inferlab::scaled_dot_product_attention_cpu(query, key, value, shape);
  require(output.size() == 2, "attention output size mismatch");
  require(output[0] > output[1], "attention did not prefer matching key value");
}

void test_attention_handles_multiple_heads() {
  const inferlab::AttentionShape shape{2, 1, 1, 2};
  const std::vector<float> query = {
      1.0F, 0.0F,
      0.0F, 1.0F,
  };
  const std::vector<float> key = query;
  const std::vector<float> value = {
      3.0F, 4.0F,
      5.0F, 6.0F,
  };
  const auto output =
      inferlab::scaled_dot_product_attention_cpu(query, key, value, shape);
  require(output == value, "single-key attention should return value");
}

void test_attention_rejects_bad_shape() {
  bool threw = false;
  try {
    (void)inferlab::scaled_dot_product_attention_cpu(
        {1.0F}, {1.0F}, {1.0F}, inferlab::AttentionShape{1, 2, 1, 1});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  require(threw, "bad attention shape did not throw");
}

void test_causal_attention_masks_future_tokens() {
  const inferlab::AttentionShape shape{1, 2, 2, 1};
  const std::vector<float> query = {1.0F, 1.0F};
  const std::vector<float> key = {1.0F, 1.0F};
  const std::vector<float> value = {10.0F, 20.0F};
  const auto causal =
      inferlab::scaled_dot_product_attention_cpu(query, key, value, shape, true);
  const auto non_causal =
      inferlab::scaled_dot_product_attention_cpu(query, key, value, shape, false);

  require(std::fabs(causal[0] - 10.0F) < 1e-6F,
          "first token should only attend to itself");
  require(std::fabs(causal[1] - non_causal[1]) < 1e-6F,
          "last token can attend to all previous tokens");
}

void test_attention_read_profile_counts_dense_reads() {
  const auto profile =
      inferlab::profile_attention_reads(inferlab::AttentionShape{2, 3, 5, 7},
                                        false);

  require(profile.score_elements == 30, "dense score element count mismatch");
  require(profile.unmasked_score_elements == 30,
          "dense unmasked score count mismatch");
  require(profile.masked_score_elements == 0,
          "dense masked score count mismatch");
  require(profile.query_element_reads_for_scores == 210,
          "dense query read count mismatch");
  require(profile.key_element_reads_for_scores == 210,
          "dense key read count mismatch");
  require(profile.value_element_reads_dense == 210,
          "dense value read count mismatch");
  require(profile.kv_element_reads_dense == 420,
          "dense K/V read count mismatch");
  require(profile.total_intermediate_bytes == 240,
          "dense intermediate byte count mismatch");
  require(profile.output_write_elements == 42,
          "dense output write count mismatch");
}

void test_attention_read_profile_counts_causal_savings() {
  const auto profile =
      inferlab::profile_attention_reads(inferlab::AttentionShape{2, 4, 4, 8},
                                        true);

  require(profile.score_elements == 32, "causal score element count mismatch");
  require(profile.unmasked_score_elements == 20,
          "causal unmasked score count mismatch");
  require(profile.masked_score_elements == 12,
          "causal masked score count mismatch");
  require(profile.query_element_reads_for_scores == 160,
          "causal query read count mismatch");
  require(profile.key_element_reads_for_scores == 160,
          "causal key read count mismatch");
  require(profile.value_element_reads_dense == 256,
          "causal dense value read count mismatch");
  require(profile.value_element_reads_mask_aware == 160,
          "causal mask-aware value read count mismatch");
  require(profile.value_multiply_add_ops_dense == 512,
          "causal dense value op count mismatch");
  require(profile.value_multiply_add_ops_mask_aware == 320,
          "causal mask-aware value op count mismatch");
  require(std::fabs(profile.causal_mask_sparsity - 0.375) < 1e-9,
          "causal mask sparsity mismatch");
}

}  // namespace

int main() {
  try {
    test_attention_prefers_matching_key();
    test_attention_handles_multiple_heads();
    test_attention_rejects_bad_shape();
    test_causal_attention_masks_future_tokens();
    test_attention_read_profile_counts_dense_reads();
    test_attention_read_profile_counts_causal_savings();
  } catch (const std::exception& error) {
    std::cerr << "attention_test failed: " << inferlab_test::redact_test_error(error.what()) << "\n";
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
