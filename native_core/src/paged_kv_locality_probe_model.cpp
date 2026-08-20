#include "paged_kv_locality_probe_model.hpp"

#include "checked_index.hpp"
#include "paged_kv_probe_utils.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace inferlab::paged_kv_locality_probe {
namespace {

void validate_request(const Request& request) {
  if (request.token_count <= 0 || request.shape.num_heads <= 0 ||
      request.shape.head_dim <= 0 ||
      request.shape.block_size_tokens <= 0) {
    throw std::invalid_argument(
        "paged KV locality request values must be positive");
  }
}

RequestBlocks make_blocks(const int token_count,
                          std::vector<BlockId> table) {
  RequestBlocks blocks;
  blocks.request_id = 1;
  blocks.token_count = token_count;
  blocks.private_token_count = token_count;
  blocks.block_table = std::move(table);
  return blocks;
}

struct FixtureTables {
  std::vector<BlockId> contiguous;
  std::vector<BlockId> fragmented;
};

FixtureTables make_fixture_tables(const Request& request) {
  validate_request(request);
  const int logical_blocks =
      internal::checked_ceil_div_non_negative_int(
          request.token_count, request.shape.block_size_tokens,
          "paged KV locality probe logical blocks");
  probe::require_paged_kv_probe_capacity(
      logical_blocks, "paged KV locality logical blocks");
  const int last_logical_block = logical_blocks - 1;
  const int max_fragmented_block = internal::checked_add_int(
      internal::checked_multiply_int(
          last_logical_block, 3,
          "paged KV locality maximum fragmented block stride"),
      last_logical_block % 2,
      "paged KV locality maximum fragmented block id");
  const int fragmented_span_blocks = internal::checked_add_int(
      max_fragmented_block, 1,
      "paged KV locality fragmented span blocks");
  const auto elements_per_block = internal::checked_product_i64(
      request.shape.num_heads, request.shape.block_size_tokens,
      request.shape.head_dim,
      "paged KV locality probe elements per block");
  (void)internal::checked_product_i64(
      request.token_count, request.shape.num_heads, request.shape.head_dim,
      "paged KV locality probe logical element reads");
  (void)internal::checked_product_i64(
      fragmented_span_blocks, elements_per_block,
      "paged KV locality probe maximum span elements");

  FixtureTables tables;
  tables.contiguous.reserve(static_cast<std::size_t>(logical_blocks));
  tables.fragmented.reserve(static_cast<std::size_t>(logical_blocks));
  for (int i = 0; i < logical_blocks; ++i) {
    tables.contiguous.push_back(i);
    tables.fragmented.push_back(internal::checked_add_int(
        internal::checked_multiply_int(
            i, 3, "paged KV locality fragmented block stride"),
        i % 2, "paged KV locality fragmented block id"));
  }
  return tables;
}

bool same_metrics(const BlockTableLocalityMetrics& left,
                  const BlockTableLocalityMetrics& right) {
  return left.token_count == right.token_count &&
         left.logical_blocks == right.logical_blocks &&
         left.unique_physical_blocks == right.unique_physical_blocks &&
         left.physical_span_blocks == right.physical_span_blocks &&
         left.transitions == right.transitions &&
         left.contiguous_forward_transitions ==
             right.contiguous_forward_transitions &&
         left.non_contiguous_transitions ==
             right.non_contiguous_transitions &&
         left.backward_transitions == right.backward_transitions &&
         left.max_physical_jump == right.max_physical_jump &&
         left.average_physical_jump == right.average_physical_jump &&
         left.logical_element_reads == right.logical_element_reads &&
         left.touched_block_elements == right.touched_block_elements &&
         left.physical_span_elements == right.physical_span_elements &&
         left.block_element_amplification ==
             right.block_element_amplification &&
         left.span_element_amplification ==
             right.span_element_amplification;
}

void validate_metrics(const RequestBlocks& blocks,
                      const BlockTableLocalityMetrics& metrics,
                      const bool expect_contiguous) {
  const int table_size = internal::checked_int_from_size(
      blocks.block_table.size(),
      "paged KV locality probe block table size");
  const int expected_transitions = std::max(0, metrics.logical_blocks - 1);
  if (metrics.token_count != blocks.token_count ||
      metrics.logical_blocks <= 0 || metrics.logical_blocks != table_size ||
      metrics.transitions < 0 ||
      metrics.contiguous_forward_transitions < 0 ||
      metrics.non_contiguous_transitions < 0 ||
      metrics.transitions != expected_transitions ||
      internal::checked_add_int(
          metrics.contiguous_forward_transitions,
          metrics.non_contiguous_transitions,
          "paged KV locality transition classes") != metrics.transitions ||
      metrics.backward_transitions < 0 ||
      metrics.backward_transitions > metrics.non_contiguous_transitions ||
      metrics.max_physical_jump < 0 ||
      metrics.unique_physical_blocks < 0 ||
      metrics.unique_physical_blocks > metrics.logical_blocks ||
      metrics.logical_element_reads <= 0 ||
      metrics.touched_block_elements <= 0 ||
      metrics.physical_span_blocks <= 0 ||
      metrics.physical_span_elements <= 0 ||
      metrics.touched_block_elements > metrics.physical_span_elements ||
      !std::isfinite(metrics.average_physical_jump) ||
      !std::isfinite(metrics.block_element_amplification) ||
      !std::isfinite(metrics.span_element_amplification) ||
      metrics.block_element_amplification < 1.0 ||
      metrics.span_element_amplification <
          metrics.block_element_amplification) {
    throw std::logic_error(
        "paged KV locality metrics disagree with fixture structure");
  }
  if (expect_contiguous &&
      (metrics.contiguous_forward_transitions != metrics.transitions ||
       metrics.non_contiguous_transitions != 0 ||
       metrics.backward_transitions != 0 ||
       metrics.unique_physical_blocks != metrics.logical_blocks ||
       metrics.physical_span_blocks != metrics.logical_blocks)) {
    throw std::logic_error(
        "contiguous paged KV fixture produced non-contiguous metrics");
  }
  if (!expect_contiguous && metrics.logical_blocks > 1 &&
      (metrics.non_contiguous_transitions != metrics.transitions ||
       metrics.contiguous_forward_transitions != 0 ||
       metrics.backward_transitions != 0)) {
    throw std::logic_error(
        "fragmented paged KV fixture produced unexpected transitions");
  }
}

}  // namespace

Plan build_plan(const Request& request) {
  const auto tables = make_fixture_tables(request);
  Plan plan;
  plan.request = request;
  plan.contiguous =
      make_blocks(request.token_count, tables.contiguous);
  plan.fragmented =
      make_blocks(request.token_count, tables.fragmented);
  plan.contiguous_metrics =
      analyze_block_table_locality(plan.contiguous, request.shape);
  plan.fragmented_metrics =
      analyze_block_table_locality(plan.fragmented, request.shape);
  plan.fragmented_vs_contiguous_span_amplification_delta =
      plan.fragmented_metrics.span_element_amplification -
      plan.contiguous_metrics.span_element_amplification;
  validate_plan(plan);
  return plan;
}

void validate_plan(const Plan& plan) {
  validate_request(plan.request);
  const auto tables = make_fixture_tables(plan.request);
  if (plan.contiguous.request_id != 1 ||
      plan.fragmented.request_id != 1 ||
      plan.contiguous.token_count != plan.request.token_count ||
      plan.fragmented.token_count != plan.request.token_count ||
      plan.contiguous.private_token_count != plan.request.token_count ||
      plan.fragmented.private_token_count != plan.request.token_count ||
      plan.contiguous.private_block_start != 0 ||
      plan.fragmented.private_block_start != 0 ||
      plan.contiguous.shared_prefix_tokens != 0 ||
      plan.fragmented.shared_prefix_tokens != 0 ||
      plan.contiguous.prefix_id.has_value() ||
      plan.fragmented.prefix_id.has_value() ||
      plan.contiguous.block_table != tables.contiguous ||
      plan.fragmented.block_table != tables.fragmented) {
    throw std::logic_error(
        "paged KV locality block-table fixtures are not canonical");
  }

  validate_metrics(plan.contiguous, plan.contiguous_metrics, true);
  validate_metrics(plan.fragmented, plan.fragmented_metrics, false);
  const auto expected_contiguous =
      analyze_block_table_locality(plan.contiguous, plan.request.shape);
  const auto expected_fragmented =
      analyze_block_table_locality(plan.fragmented, plan.request.shape);
  const double expected_delta =
      expected_fragmented.span_element_amplification -
      expected_contiguous.span_element_amplification;
  if (!same_metrics(plan.contiguous_metrics, expected_contiguous) ||
      !same_metrics(plan.fragmented_metrics, expected_fragmented) ||
      !std::isfinite(expected_delta) || expected_delta < 0.0 ||
      plan.fragmented_vs_contiguous_span_amplification_delta !=
          expected_delta ||
      plan.contiguous_metrics.block_element_amplification !=
          plan.fragmented_metrics.block_element_amplification ||
      plan.fragmented_metrics.span_element_amplification <
          plan.contiguous_metrics.span_element_amplification) {
    throw std::logic_error(
        "paged KV locality plan metrics are not canonical");
  }
}

}  // namespace inferlab::paged_kv_locality_probe
