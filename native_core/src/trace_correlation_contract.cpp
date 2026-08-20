#include "trace_correlation_contract.hpp"

#include "plan_json_utils.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_trace {
namespace {

constexpr std::size_t kW3cTraceIdLength = 32U;
constexpr std::size_t kW3cSpanIdLength = 16U;
constexpr std::size_t kFingerprintHexLength = 32U;
constexpr char kFingerprintPrefix[] = "fnv1a64x2:";
constexpr std::size_t kFingerprintPrefixLength =
    sizeof(kFingerprintPrefix) - 1U;
constexpr char kTraceJoinTokenPrefix[] = "trace-fnv1a64x2:";
constexpr std::size_t kTraceJoinTokenPrefixLength =
    sizeof(kTraceJoinTokenPrefix) - 1U;
constexpr char kSpanJoinTokenPrefix[] = "span-fnv1a64x2:";
constexpr std::size_t kSpanJoinTokenPrefixLength =
    sizeof(kSpanJoinTokenPrefix) - 1U;

bool lowercase_hex(const char value) noexcept {
  return (value >= '0' && value <= '9') ||
         (value >= 'a' && value <= 'f');
}

bool valid_nonzero_lowercase_hex(const std::string& value,
                                 const std::size_t expected_length) noexcept {
  if (value.size() != expected_length ||
      !std::all_of(value.begin(), value.end(), lowercase_hex)) {
    return false;
  }
  return std::any_of(value.begin(), value.end(),
                     [](const char digit) { return digit != '0'; });
}

std::string framed_identity_token(const std::string& value,
                                  const std::string& domain,
                                  const char* prefix) {
  const std::string payload =
      domain + "|length=" + std::to_string(value.size()) + "|" + value;
  const std::string primary = domain + ".primary|" + payload;
  const std::string secondary = domain + ".secondary|" + payload;
  return std::string(prefix) +
         inferlab_plan::hex_u64(inferlab_plan::fnv1a64(primary)) +
         inferlab_plan::hex_u64(inferlab_plan::fnv1a64(secondary));
}

std::string framed_span_context_token(const std::string& trace_id,
                                      const std::string& span_id) {
  const std::string domain =
      "inferlab.span-context-join-token.v1";
  const std::string payload =
      domain + "|trace_length=" + std::to_string(trace_id.size()) +
      "|" + trace_id + "|span_length=" +
      std::to_string(span_id.size()) + "|" + span_id;
  const std::string primary = domain + ".primary|" + payload;
  const std::string secondary = domain + ".secondary|" + payload;
  return std::string(kSpanJoinTokenPrefix) +
         inferlab_plan::hex_u64(inferlab_plan::fnv1a64(primary)) +
         inferlab_plan::hex_u64(inferlab_plan::fnv1a64(secondary));
}

bool valid_join_token_shape(const std::string& token,
                            const char* prefix,
                            const std::size_t prefix_length) noexcept {
  if (token.size() != prefix_length + kFingerprintHexLength ||
      token.compare(0U, prefix_length, prefix, prefix_length) != 0) {
    return false;
  }
  return std::all_of(
      token.begin() + static_cast<std::ptrdiff_t>(prefix_length),
      token.end(), lowercase_hex);
}

std::string canonical_trace_set_payload(
    std::vector<std::string> trace_ids) {
  for (const auto& trace_id : trace_ids) {
    if (!valid_w3c_trace_id(trace_id)) {
      throw std::invalid_argument(
          "trace context set contains an invalid W3C trace id");
    }
  }
  std::sort(trace_ids.begin(), trace_ids.end());
  trace_ids.erase(std::unique(trace_ids.begin(), trace_ids.end()),
                  trace_ids.end());

  std::string payload = "inferlab.trace-context-set.v1|count=" +
                        std::to_string(trace_ids.size()) + "|";
  for (const auto& trace_id : trace_ids) {
    payload += std::to_string(trace_id.size());
    payload.push_back(':');
    payload += trace_id;
    payload.push_back('|');
  }
  return payload;
}

}  // namespace

const std::string& trace_context_fingerprint_algorithm() {
  static const std::string algorithm =
      "fnv1a64x2_length_framed_v1_noncryptographic";
  return algorithm;
}

const std::string& trace_context_join_token_algorithm() {
  static const std::string algorithm =
      "fnv1a64x2_length_framed_join_token_v1_noncryptographic";
  return algorithm;
}

bool valid_w3c_trace_id(const std::string& trace_id) noexcept {
  return valid_nonzero_lowercase_hex(trace_id, kW3cTraceIdLength);
}

bool valid_w3c_span_id(const std::string& span_id) noexcept {
  return valid_nonzero_lowercase_hex(span_id, kW3cSpanIdLength);
}

std::string trace_context_set_fingerprint(
    const std::vector<std::string>& trace_ids) {
  if (trace_ids.empty()) return "none";
  const std::string payload = canonical_trace_set_payload(trace_ids);
  const std::string primary = "inferlab.trace-context.primary|" + payload;
  const std::string secondary =
      "inferlab.trace-context.secondary|" + payload;
  return std::string(kFingerprintPrefix) +
         inferlab_plan::hex_u64(inferlab_plan::fnv1a64(primary)) +
         inferlab_plan::hex_u64(inferlab_plan::fnv1a64(secondary));
}

bool valid_trace_context_set_fingerprint(
    const std::string& fingerprint) noexcept {
  if (fingerprint == "none") return true;
  if (fingerprint.size() !=
          kFingerprintPrefixLength + kFingerprintHexLength ||
      fingerprint.compare(0U, kFingerprintPrefixLength,
                          kFingerprintPrefix,
                          kFingerprintPrefixLength) != 0) {
    return false;
  }
  return std::all_of(fingerprint.begin() +
                         static_cast<std::ptrdiff_t>(
                             kFingerprintPrefixLength),
                     fingerprint.end(), lowercase_hex);
}

std::string trace_context_join_token(const std::string& trace_id) {
  if (!valid_w3c_trace_id(trace_id)) {
    throw std::invalid_argument(
        "trace context join token requires a valid W3C trace id");
  }
  return framed_identity_token(
      trace_id, "inferlab.trace-context-join-token.v1",
      kTraceJoinTokenPrefix);
}

bool valid_trace_context_join_token(const std::string& token) noexcept {
  return valid_join_token_shape(token, kTraceJoinTokenPrefix,
                                kTraceJoinTokenPrefixLength);
}

std::string span_context_join_token(const std::string& trace_id,
                                    const std::string& span_id) {
  if (!valid_w3c_trace_id(trace_id) || !valid_w3c_span_id(span_id)) {
    throw std::invalid_argument(
        "span context join token requires valid W3C trace and span ids");
  }
  return framed_span_context_token(trace_id, span_id);
}

bool valid_span_context_join_token(const std::string& token) noexcept {
  return valid_join_token_shape(token, kSpanJoinTokenPrefix,
                                kSpanJoinTokenPrefixLength);
}

}  // namespace inferlab_trace
