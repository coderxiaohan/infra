#include "inferlab/shape_trace.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <array>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace inferlab {
namespace {

std::string vector_json(const std::vector<int>& values) {
  std::ostringstream out;
  out << "[";
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      out << ", ";
    }
    out << values[i];
  }
  out << "]";
  return out.str();
}

void require_vector_equal(const std::vector<int>& actual,
                          const std::vector<int>& expected,
                          const char* name) {
  if (actual != expected) {
    throw std::invalid_argument(std::string(name) +
                                " does not match trace config");
  }
}

void require_kv_estimate_equal(const KVCacheEstimate& actual,
                               const KVCacheEstimate& expected,
                               const char* name) {
  if (actual.key_shape != expected.key_shape ||
      actual.value_shape != expected.value_shape ||
      actual.bytes_per_token_per_layer != expected.bytes_per_token_per_layer ||
      actual.total_bytes != expected.total_bytes) {
    throw std::invalid_argument(std::string(name) +
                                " does not match trace config");
  }
}

KVCacheEstimate expected_kv_estimate(const TransformerShapeConfig& config,
                                     const int sequence_length) {
  return estimate_kv_cache(KVCacheConfig{
      config.batch_size,
      sequence_length,
      config.num_layers,
      config.num_heads,
      config.head_dim,
      config.dtype,
  });
}

void validate_trace_consistency(const ShapeTrace& trace) {
  validate(trace.config);
  require_vector_equal(
      trace.input_shape,
      {trace.config.batch_size, trace.config.seq_len,
       trace.config.hidden_size},
      "input shape");
  require_vector_equal(
      trace.qkv_projection_shape,
      {trace.config.batch_size, trace.config.seq_len,
       internal::checked_multiply_int(3, trace.config.hidden_size,
                                      "QKV projection hidden size")},
      "QKV projection shape");
  require_vector_equal(
      trace.attention_score_shape,
      {trace.config.batch_size, trace.config.num_heads, trace.config.seq_len,
       trace.config.seq_len},
      "attention score shape");
  require_kv_estimate_equal(
      trace.prefill_kv_cache,
      expected_kv_estimate(trace.config, trace.config.seq_len),
      "prefill KV estimate");

  if (trace.decode.size() !=
      static_cast<std::size_t>(trace.config.decode_steps)) {
    throw std::invalid_argument(
        "decode trace size does not match configured decode steps");
  }
  for (std::size_t i = 0; i < trace.decode.size(); ++i) {
    const int step = internal::checked_int_from_size(
        internal::checked_add_size(i, 1U, "decode step"), "decode step");
    const int sequence_length = internal::checked_add_int(
        trace.config.seq_len, step, "decode sequence length");
    const auto& row = trace.decode[i];
    if (row.step != step || row.kv_sequence_length != sequence_length) {
      throw std::invalid_argument(
          "decode step metadata does not match trace config");
    }
    require_vector_equal(
        row.query_shape,
        {trace.config.batch_size, 1, trace.config.hidden_size},
        "decode query shape");
    const auto expected = expected_kv_estimate(trace.config, sequence_length);
    if (row.kv_total_bytes != expected.total_bytes) {
      throw std::invalid_argument(
          "decode KV bytes do not match trace config");
    }
  }
}

std::string kv_estimate_json(const KVCacheEstimate& estimate,
                             const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  std::ostringstream out;
  out << "{\n";
  out << nested << "\"bytes_per_token_per_layer\": "
      << estimate.bytes_per_token_per_layer << ",\n";
  out << nested << "\"key_shape\": " << vector_json(estimate.key_shape) << ",\n";
  out << nested << "\"total_bytes\": " << estimate.total_bytes << ",\n";
  out << nested << "\"total_mib\": " << std::fixed << std::setprecision(6)
      << estimate.total_mib() << ",\n";
  out << nested << "\"value_shape\": " << vector_json(estimate.value_shape) << "\n";
  out << pad << "}";
  return out.str();
}

}  // namespace

void validate(const TransformerShapeConfig& config) {
  const std::array<std::pair<const char*, int>, 7> fields = {{
      {"batch_size", config.batch_size},
      {"seq_len", config.seq_len},
      {"decode_steps", config.decode_steps},
      {"hidden_size", config.hidden_size},
      {"num_layers", config.num_layers},
      {"num_heads", config.num_heads},
      {"head_dim", config.head_dim},
  }};
  for (const auto& [name, value] : fields) {
    if (value <= 0) {
      throw std::invalid_argument(std::string(name) + " must be positive");
    }
  }
  const int expected_hidden_size = internal::checked_multiply_int(
      config.num_heads, config.head_dim, "num_heads * head_dim");
  if (config.hidden_size != expected_hidden_size) {
    throw std::invalid_argument("hidden_size must equal num_heads * head_dim");
  }
  (void)internal::checked_multiply_int(
      3, config.hidden_size, "QKV projection hidden size");
  const int final_sequence_length = internal::checked_add_int(
      config.seq_len, config.decode_steps, "final decode sequence length");
  (void)expected_kv_estimate(config, final_sequence_length);
}

ShapeTrace build_trace(const TransformerShapeConfig& config) {
  validate(config);

  ShapeTrace trace;
  trace.config = config;
  trace.input_shape = {config.batch_size, config.seq_len, config.hidden_size};
  trace.qkv_projection_shape = {
      config.batch_size,
      config.seq_len,
      internal::checked_multiply_int(3, config.hidden_size,
                                     "QKV projection hidden size"),
  };
  trace.attention_score_shape = {
      config.batch_size,
      config.num_heads,
      config.seq_len,
      config.seq_len,
  };
  trace.prefill_kv_cache = expected_kv_estimate(config, config.seq_len);

  trace.decode.reserve(static_cast<std::size_t>(config.decode_steps));
  for (int offset = 0; offset < config.decode_steps; ++offset) {
    const int step = internal::checked_add_int(offset, 1, "decode step");
    const int total_seq =
        internal::checked_add_int(config.seq_len, step, "decode sequence length");
    const auto estimate = expected_kv_estimate(config, total_seq);
    trace.decode.push_back(DecodeTraceRow{
        step,
        {config.batch_size, 1, config.hidden_size},
        total_seq,
        estimate.total_bytes,
    });
  }
  return trace;
}

std::string to_json(const ShapeTrace& trace) {
  validate_trace_consistency(trace);
  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_native_probe_contract(
      out, "native_shape_probe",
      "Executed deterministic CPU shape/KV reference calculation only; not "
      "a product runtime trace, GPU benchmark, latency result, or production "
      "capacity guarantee.");
  out << "  \"config\": {\n";
  out << "    \"batch_size\": " << trace.config.batch_size << ",\n";
  out << "    \"decode_steps\": " << trace.config.decode_steps << ",\n";
  out << "    \"dtype\": \"" << dtype_name(trace.config.dtype) << "\",\n";
  out << "    \"head_dim\": " << trace.config.head_dim << ",\n";
  out << "    \"hidden_size\": " << trace.config.hidden_size << ",\n";
  out << "    \"num_heads\": " << trace.config.num_heads << ",\n";
  out << "    \"num_layers\": " << trace.config.num_layers << ",\n";
  out << "    \"seq_len\": " << trace.config.seq_len << "\n";
  out << "  },\n";
  out << "  \"decode\": [\n";
  for (std::size_t i = 0; i < trace.decode.size(); ++i) {
    const auto& row = trace.decode[i];
    out << "    {\n";
    out << "      \"kv_sequence_length\": " << row.kv_sequence_length << ",\n";
    out << "      \"kv_total_bytes\": " << row.kv_total_bytes << ",\n";
    out << "      \"kv_total_mib\": " << std::fixed << std::setprecision(6)
        << KVCacheEstimate::mib_for_bytes(row.kv_total_bytes) << ",\n";
    out << "      \"query_shape\": " << vector_json(row.query_shape) << ",\n";
    out << "      \"step\": " << row.step << "\n";
    out << "    }" << (i + 1 == trace.decode.size() ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"prefill\": {\n";
  out << "    \"attention_score_shape\": "
      << vector_json(trace.attention_score_shape) << ",\n";
  out << "    \"input_shape\": " << vector_json(trace.input_shape) << ",\n";
  out << "    \"kv_cache\": " << kv_estimate_json(trace.prefill_kv_cache, 4)
      << ",\n";
  out << "    \"qkv_projection_shape\": "
      << vector_json(trace.qkv_projection_shape) << "\n";
  out << "  }\n";
  out << "}\n";
  return out.str();
}

}  // namespace inferlab
