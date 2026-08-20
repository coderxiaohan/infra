#pragma once

#include "advanced_serving_internal.hpp"

namespace inferlab::runtime_communication_internal {

constexpr double kBitsPerByte = 8.0;
constexpr double kBytesPerMib = 1024.0 * 1024.0;
constexpr double kBytesPerGib = 1024.0 * 1024.0 * 1024.0;
constexpr double kBitsPerGigabit = 1000.0 * 1000.0 * 1000.0;
constexpr double kMillisecondsPerSecond = 1000.0;

inline double bytes_to_gib(const double transfer_bytes,
                           const char* name) {
  const double bounded_bytes =
      advanced_serving_internal::require_non_negative_finite(
          transfer_bytes, name);
  return advanced_serving_internal::require_finite(
      bounded_bytes / kBytesPerGib, name);
}

inline double gib_payload_gigabit_milliseconds(
    const double transfer_gib,
    const char* name) {
  const double bounded_transfer =
      advanced_serving_internal::require_non_negative_finite(
          transfer_gib, name);
  const double transfer_bytes = advanced_serving_internal::finite_product(
      bounded_transfer, kBytesPerGib, name);
  const double transfer_bits = advanced_serving_internal::finite_product(
      transfer_bytes, kBitsPerByte, name);
  const double transfer_gigabits =
      advanced_serving_internal::require_finite(
          transfer_bits / kBitsPerGigabit, name);
  return advanced_serving_internal::finite_product(
      transfer_gigabits, kMillisecondsPerSecond, name);
}

inline double transfer_milliseconds(const double transfer_gib,
                                    const double bandwidth_gbps,
                                    const char* name) {
  const double bounded_transfer =
      advanced_serving_internal::require_non_negative_finite(
          transfer_gib, name);
  if (bounded_transfer <= 0.0) {
    return 0.0;
  }
  const double bounded_bandwidth =
      advanced_serving_internal::require_positive_finite(
          bandwidth_gbps, name);
  const double gigabit_milliseconds =
      gib_payload_gigabit_milliseconds(bounded_transfer, name);
  return advanced_serving_internal::require_finite(
      gigabit_milliseconds / bounded_bandwidth, name);
}

inline double transfer_milliseconds_from_bytes(
    const double transfer_bytes,
    const double bandwidth_gbps,
    const char* name) {
  return transfer_milliseconds(
      bytes_to_gib(transfer_bytes, name), bandwidth_gbps, name);
}

}  // namespace inferlab::runtime_communication_internal
