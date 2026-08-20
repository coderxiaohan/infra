#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::internal {

inline void validate_all_finite(const std::vector<float>& values,
                                const char* name) {
  for (const auto value : values) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument(std::string(name) +
                                  " must contain only finite values");
    }
  }
}

inline double checked_float_vector_reduction_as_double(
    const std::vector<float>& values, const char* name,
    const bool absolute_values) {
  double total = 0.0;
  for (const auto value : values) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument(std::string(name) +
                                  " must contain only finite values");
    }
    const double term = absolute_values
                            ? std::fabs(static_cast<double>(value))
                            : static_cast<double>(value);
    total += term;
    if (!std::isfinite(total)) {
      throw std::overflow_error(
          std::string(name) +
          (absolute_values ? " absolute sum is non-finite"
                           : " sum is non-finite"));
    }
  }
  return total;
}

inline double checked_float_vector_sum_as_double(
    const std::vector<float>& values, const char* name) {
  return checked_float_vector_reduction_as_double(values, name, false);
}

inline double checked_float_vector_abs_sum_as_double(
    const std::vector<float>& values, const char* name) {
  return checked_float_vector_reduction_as_double(values, name, true);
}

inline double checked_add_finite_double(const double lhs,
                                        const double rhs,
                                        const char* name) {
  if (!std::isfinite(lhs) || !std::isfinite(rhs)) {
    throw std::invalid_argument(std::string(name) +
                                " requires finite operands");
  }
  const double sum = lhs + rhs;
  if (!std::isfinite(sum)) {
    throw std::overflow_error(std::string(name) + " is non-finite");
  }
  return sum;
}

inline int checked_add_int(const int lhs, const int rhs, const char* name) {
  const auto sum = static_cast<long long>(lhs) + static_cast<long long>(rhs);
  if (sum > static_cast<long long>(std::numeric_limits<int>::max()) ||
      sum < static_cast<long long>(std::numeric_limits<int>::min())) {
    throw std::invalid_argument(std::string(name) + " exceeds int range");
  }
  return static_cast<int>(sum);
}

inline int checked_multiply_int(const int lhs, const int rhs,
                                const char* name) {
  const auto product =
      static_cast<long long>(lhs) * static_cast<long long>(rhs);
  if (product > static_cast<long long>(std::numeric_limits<int>::max()) ||
      product < static_cast<long long>(std::numeric_limits<int>::min())) {
    throw std::invalid_argument(std::string(name) + " exceeds int range");
  }
  return static_cast<int>(product);
}

inline int checked_int_from_size(const std::size_t value, const char* name) {
  if (value > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument(std::string(name) + " exceeds int range");
  }
  return static_cast<int>(value);
}

inline int checked_int_from_i64(const std::int64_t value, const char* name) {
  if (value > static_cast<std::int64_t>(std::numeric_limits<int>::max()) ||
      value < static_cast<std::int64_t>(std::numeric_limits<int>::min())) {
    throw std::invalid_argument(std::string(name) + " exceeds int range");
  }
  return static_cast<int>(value);
}

inline int checked_ceil_div_non_negative_int(const int value,
                                             const int divisor,
                                             const char* name) {
  if (value < 0 || divisor <= 0) {
    throw std::invalid_argument(std::string(name) +
                                " requires non-negative value and positive divisor");
  }
  if (value == 0) {
    return 0;
  }
  const auto numerator = static_cast<long long>(value) +
                         static_cast<long long>(divisor) - 1LL;
  const auto result = numerator / static_cast<long long>(divisor);
  if (result > static_cast<long long>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument(std::string(name) + " exceeds int range");
  }
  return static_cast<int>(result);
}

inline int checked_non_negative_ceil_double_to_int(const double value,
                                                   const char* name) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument(std::string(name) + " must be finite");
  }
  if (value <= 0.0) {
    return 0;
  }
  const double rounded = std::ceil(value);
  if (rounded > static_cast<double>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument(std::string(name) + " exceeds int range");
  }
  return static_cast<int>(rounded);
}

inline int checked_positive_ceil_double_to_int(const double value,
                                               const char* name) {
  return std::max(1, checked_non_negative_ceil_double_to_int(value, name));
}

inline int checked_non_negative_floor_double_to_int(const double value,
                                                    const char* name) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument(std::string(name) + " must be finite");
  }
  if (value <= 0.0) {
    return 0;
  }
  const double rounded = std::floor(value);
  if (rounded > static_cast<double>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument(std::string(name) + " exceeds int range");
  }
  return static_cast<int>(rounded);
}

inline int checked_positive_floor_double_to_int(const double value,
                                                const char* name) {
  return std::max(1, checked_non_negative_floor_double_to_int(value, name));
}

inline int checked_non_negative_round_double_to_int(const double value,
                                                    const char* name) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument(std::string(name) + " must be finite");
  }
  if (value <= 0.0) {
    return 0;
  }
  const double rounded = std::round(value);
  if (rounded > static_cast<double>(std::numeric_limits<int>::max())) {
    throw std::invalid_argument(std::string(name) + " exceeds int range");
  }
  return static_cast<int>(rounded);
}

inline int checked_round_to_even_double_to_int(const double value,
                                               const char* name) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument(std::string(name) + " must be finite");
  }
  const double lower = std::floor(value);
  const double fraction = value - lower;
  double rounded = lower;
  if (fraction > 0.5 ||
      (fraction == 0.5 && std::fmod(std::fabs(lower), 2.0) != 0.0)) {
    rounded = lower + 1.0;
  }
  if (rounded > static_cast<double>(std::numeric_limits<int>::max()) ||
      rounded < static_cast<double>(std::numeric_limits<int>::min())) {
    throw std::invalid_argument(std::string(name) + " exceeds int range");
  }
  return static_cast<int>(rounded);
}

inline float checked_float_from_double(const double value,
                                       const char* name) {
  if (!std::isfinite(value) ||
      value > static_cast<double>(std::numeric_limits<float>::max()) ||
      value < -static_cast<double>(std::numeric_limits<float>::max())) {
    throw std::invalid_argument(std::string(name) +
                                " exceeds finite float range");
  }
  return static_cast<float>(value);
}

template <typename Iterator, typename Predicate>
int checked_count_if(Iterator begin, Iterator end, Predicate predicate,
                     const char* name) {
  return checked_int_from_i64(
      static_cast<std::int64_t>(std::count_if(begin, end, predicate)), name);
}

inline std::int64_t checked_add_i64(const std::int64_t lhs,
                                    const std::int64_t rhs,
                                    const char* name) {
  if ((rhs > 0 && lhs > std::numeric_limits<std::int64_t>::max() - rhs) ||
      (rhs < 0 && lhs < std::numeric_limits<std::int64_t>::min() - rhs)) {
    throw std::invalid_argument(std::string(name) + " exceeds int64 range");
  }
  return lhs + rhs;
}

inline std::uint64_t checked_add_u64(const std::uint64_t lhs,
                                     const std::uint64_t rhs,
                                     const char* name) {
  if (lhs > std::numeric_limits<std::uint64_t>::max() - rhs) {
    throw std::invalid_argument(std::string(name) + " exceeds uint64 range");
  }
  return lhs + rhs;
}

inline std::uint64_t checked_product_u64(const std::uint64_t lhs,
                                         const std::uint64_t rhs,
                                         const char* name) {
  if (rhs != 0U && lhs > std::numeric_limits<std::uint64_t>::max() / rhs) {
    throw std::invalid_argument(std::string(name) + " exceeds uint64 range");
  }
  return lhs * rhs;
}

inline std::int64_t checked_product_i64(const std::int64_t lhs,
                                        const std::int64_t rhs,
                                        const char* name) {
  if (lhs < 0 || rhs < 0) {
    throw std::invalid_argument(std::string(name) + " factors must be non-negative");
  }
  if (rhs != 0 &&
      lhs > std::numeric_limits<std::int64_t>::max() / rhs) {
    throw std::invalid_argument(std::string(name) + " exceeds int64 range");
  }
  return lhs * rhs;
}

inline std::int64_t checked_product_i64(const std::int64_t first,
                                        const std::int64_t second,
                                        const std::int64_t third,
                                        const char* name) {
  return checked_product_i64(checked_product_i64(first, second, name), third,
                             name);
}

inline std::int64_t checked_product_i64(const std::int64_t first,
                                        const std::int64_t second,
                                        const std::int64_t third,
                                        const std::int64_t fourth,
                                        const char* name) {
  return checked_product_i64(
      checked_product_i64(first, second, third, name), fourth, name);
}

inline std::int64_t checked_packed_bytes_for_bits(
    const std::int64_t element_count, const int bits_per_element,
    const char* name) {
  if (element_count < 0 || bits_per_element <= 0) {
    throw std::invalid_argument(std::string(name) +
                                " requires non-negative elements and positive bits");
  }
  const auto total_bits =
      checked_product_i64(element_count, bits_per_element, name);
  return total_bits / 8 + (total_bits % 8 == 0 ? 0 : 1);
}

inline std::size_t checked_product_size(const int lhs, const int rhs,
                                        const char* name) {
  if (lhs <= 0 || rhs <= 0) {
    throw std::invalid_argument(std::string(name) + " dimensions must be positive");
  }
  const auto left = static_cast<std::size_t>(lhs);
  const auto right = static_cast<std::size_t>(rhs);
  if (left > std::numeric_limits<std::size_t>::max() / right) {
    throw std::invalid_argument(std::string(name) + " exceeds size_t range");
  }
  return left * right;
}

inline std::size_t checked_product_size(const int first, const int second,
                                        const int third, const char* name) {
  const auto first_second = checked_product_size(first, second, name);
  if (third <= 0) {
    throw std::invalid_argument(std::string(name) + " dimensions must be positive");
  }
  const auto third_size = static_cast<std::size_t>(third);
  if (first_second > std::numeric_limits<std::size_t>::max() / third_size) {
    throw std::invalid_argument(std::string(name) + " exceeds size_t range");
  }
  return first_second * third_size;
}

inline std::size_t checked_product_size(const std::size_t lhs, const int rhs,
                                        const char* name) {
  if (rhs <= 0) {
    throw std::invalid_argument(std::string(name) + " dimensions must be positive");
  }
  const auto right = static_cast<std::size_t>(rhs);
  if (lhs > std::numeric_limits<std::size_t>::max() / right) {
    throw std::invalid_argument(std::string(name) + " exceeds size_t range");
  }
  return lhs * right;
}

inline std::size_t checked_product_size(const std::size_t lhs,
                                        const std::size_t rhs,
                                        const char* name) {
  if (rhs != 0U && lhs > std::numeric_limits<std::size_t>::max() / rhs) {
    throw std::invalid_argument(std::string(name) + " exceeds size_t range");
  }
  return lhs * rhs;
}

inline std::size_t checked_add_size(const std::size_t lhs,
                                    const std::size_t rhs,
                                    const char* name) {
  if (lhs > std::numeric_limits<std::size_t>::max() - rhs) {
    throw std::invalid_argument(std::string(name) + " exceeds size_t range");
  }
  return lhs + rhs;
}

inline std::size_t checked_sum_size(
    const std::initializer_list<std::size_t> values,
    const char* name) {
  std::size_t total = 0U;
  for (const auto value : values) {
    total = checked_add_size(total, value, name);
  }
  return total;
}

inline int checked_floor_percentage(const std::size_t part,
                                    const std::size_t total,
                                    const char* name,
                                    const int empty_percentage = 100) {
  if (empty_percentage < 0 || empty_percentage > 100) {
    throw std::invalid_argument(std::string(name) +
                                " empty percentage must be in [0, 100]");
  }
  if (total == 0U) {
    if (part != 0U) {
      throw std::invalid_argument(std::string(name) +
                                  " part must be zero when total is zero");
    }
    return empty_percentage;
  }
  if (part > total) {
    throw std::invalid_argument(std::string(name) +
                                " part must not exceed total");
  }

  // Accumulate one percentage point at a time while keeping the remainder
  // below total. This computes floor(part * 100 / total) without multiplying
  // two size_t values.
  std::size_t remainder = 0U;
  int percentage = 0;
  const std::size_t wrap_threshold = total - part;
  for (int point = 0; point < 100; ++point) {
    if (remainder >= wrap_threshold) {
      remainder -= wrap_threshold;
      ++percentage;
    } else {
      remainder += part;
    }
  }
  return percentage;
}

inline std::size_t checked_product_size(const int first, const int second,
                                        const int third, const int fourth,
                                        const char* name) {
  return checked_product_size(
      checked_product_size(first, second, third, name), fourth, name);
}

inline std::size_t checked_non_negative_product_size(const int lhs,
                                                     const int rhs,
                                                     const char* name) {
  if (lhs < 0 || rhs < 0) {
    throw std::invalid_argument(std::string(name) + " dimensions must be non-negative");
  }
  const auto left = static_cast<std::size_t>(lhs);
  const auto right = static_cast<std::size_t>(rhs);
  if (right != 0U && left > std::numeric_limits<std::size_t>::max() / right) {
    throw std::invalid_argument(std::string(name) + " exceeds size_t range");
  }
  return left * right;
}

inline std::size_t checked_non_negative_product_size(const int first,
                                                     const int second,
                                                     const int third,
                                                     const char* name) {
  const auto first_second =
      checked_non_negative_product_size(first, second, name);
  if (third < 0) {
    throw std::invalid_argument(std::string(name) + " dimensions must be non-negative");
  }
  const auto third_size = static_cast<std::size_t>(third);
  if (third_size != 0U &&
      first_second > std::numeric_limits<std::size_t>::max() / third_size) {
    throw std::invalid_argument(std::string(name) + " exceeds size_t range");
  }
  return first_second * third_size;
}

inline std::size_t checked_flat_index(const int major, const int stride,
                                      const int minor, const char* name) {
  if (major < 0 || stride <= 0 || minor < 0 || minor >= stride) {
    throw std::invalid_argument(std::string(name) + " index dimensions are invalid");
  }
  const auto major_size = static_cast<std::size_t>(major);
  const auto stride_size = static_cast<std::size_t>(stride);
  const auto minor_size = static_cast<std::size_t>(minor);
  if (major_size > std::numeric_limits<std::size_t>::max() / stride_size) {
    throw std::invalid_argument(std::string(name) + " index exceeds size_t range");
  }
  const auto base = major_size * stride_size;
  if (base > std::numeric_limits<std::size_t>::max() - minor_size) {
    throw std::invalid_argument(std::string(name) + " index exceeds size_t range");
  }
  return base + minor_size;
}

inline std::size_t checked_flat_index_size(const std::size_t major,
                                           const int stride, const int minor,
                                           const char* name) {
  if (stride <= 0 || minor < 0 || minor >= stride) {
    throw std::invalid_argument(std::string(name) + " index dimensions are invalid");
  }
  const auto stride_size = static_cast<std::size_t>(stride);
  const auto minor_size = static_cast<std::size_t>(minor);
  if (major > std::numeric_limits<std::size_t>::max() / stride_size) {
    throw std::invalid_argument(std::string(name) + " index exceeds size_t range");
  }
  const auto base = major * stride_size;
  if (base > std::numeric_limits<std::size_t>::max() - minor_size) {
    throw std::invalid_argument(std::string(name) + " index exceeds size_t range");
  }
  return base + minor_size;
}

inline std::size_t checked_flat_index3(const int first, const int second_stride,
                                       const int second, const int third_stride,
                                       const int third, const char* name) {
  const auto first_second =
      checked_flat_index(first, second_stride, second, name);
  if (third_stride <= 0 || third < 0 || third >= third_stride) {
    throw std::invalid_argument(std::string(name) + " index dimensions are invalid");
  }
  const auto third_stride_size = static_cast<std::size_t>(third_stride);
  const auto third_size = static_cast<std::size_t>(third);
  if (first_second >
      std::numeric_limits<std::size_t>::max() / third_stride_size) {
    throw std::invalid_argument(std::string(name) + " index exceeds size_t range");
  }
  const auto base = first_second * third_stride_size;
  if (base > std::numeric_limits<std::size_t>::max() - third_size) {
    throw std::invalid_argument(std::string(name) + " index exceeds size_t range");
  }
  return base + third_size;
}

inline std::size_t checked_pair_count(const std::size_t element_count,
                                      const char* name) {
  (void)name;
  return element_count / 2U + element_count % 2U;
}

inline std::ptrdiff_t checked_ptrdiff_offset(const std::size_t offset,
                                             const char* name) {
  if (offset > static_cast<std::size_t>(
                   std::numeric_limits<std::ptrdiff_t>::max())) {
    throw std::invalid_argument(std::string(name) + " exceeds ptrdiff_t range");
  }
  return static_cast<std::ptrdiff_t>(offset);
}

}  // namespace inferlab::internal
