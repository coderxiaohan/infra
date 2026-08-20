#pragma once

#include <cstddef>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::advanced_serving_internal {

inline void require_nonempty(const std::string& value, const char* name) {
  if (value.empty()) {
    throw std::invalid_argument(std::string(name) + " must not be empty");
  }
}

inline void require_unique_nonempty_strings(
    const std::vector<std::string>& values,
    const char* collection_name) {
  if (values.empty()) {
    throw std::invalid_argument(
        std::string(collection_name) + " must not be empty");
  }
  for (std::size_t index = 0; index < values.size(); ++index) {
    require_nonempty(values[index], collection_name);
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (values[prior] == values[index]) {
        throw std::invalid_argument(
            std::string(collection_name) + " values must be unique");
      }
    }
  }
}

inline int require_non_negative_int(const int value, const char* name) {
  if (value < 0) {
    throw std::invalid_argument(std::string(name) + " must be non-negative");
  }
  return value;
}

inline int require_positive_int(const int value, const char* name) {
  if (value <= 0) {
    throw std::invalid_argument(std::string(name) + " must be positive");
  }
  return value;
}

inline int positive_or_one(const int value) {
  return value <= 0 ? 1 : value;
}

inline double positive_or_default(const double value, const double fallback) {
  if (!std::isfinite(fallback) || fallback <= 0.0) {
    throw std::invalid_argument(
        "advanced serving fallback must be finite and positive");
  }
  return std::isfinite(value) && value > 0.0 ? value : fallback;
}

inline double require_finite(const double value, const char* name) {
  if (!std::isfinite(value)) {
    throw std::invalid_argument(std::string(name) + " must be finite");
  }
  return value;
}

inline double require_positive_finite(const double value, const char* name) {
  const double finite_value = require_finite(value, name);
  if (finite_value <= 0.0) {
    throw std::invalid_argument(std::string(name) + " must be positive");
  }
  return finite_value;
}

inline double require_non_negative_finite(const double value,
                                          const char* name) {
  const double finite_value = require_finite(value, name);
  if (finite_value < 0.0) {
    throw std::invalid_argument(std::string(name) + " must be non-negative");
  }
  return finite_value;
}

inline double require_unit_interval(const double value, const char* name) {
  const double finite_value = require_finite(value, name);
  if (finite_value < 0.0 || finite_value > 1.0) {
    throw std::invalid_argument(std::string(name) + " must be in [0, 1]");
  }
  return finite_value;
}

inline double positive_finite_or_default(const double value,
                                         const double fallback,
                                         const char* name) {
  return positive_or_default(require_finite(value, name), fallback);
}

inline double finite_product(const double lhs,
                             const double rhs,
                             const char* name) {
  return require_finite(lhs * rhs, name);
}

inline double non_negative_finite_or_zero(const double value,
                                          const char* name) {
  const double finite_value = require_finite(value, name);
  return finite_value > 0.0 ? finite_value : 0.0;
}

inline double non_negative_or_zero(const double value) {
  return std::isfinite(value) && value > 0.0 ? value : 0.0;
}

inline double clamp_unit_interval(const double value) {
  if (!std::isfinite(value) || value <= 0.0) {
    return 0.0;
  }
  return value >= 1.0 ? 1.0 : value;
}

inline double finite_unit_interval(const double value, const char* name) {
  return clamp_unit_interval(require_finite(value, name));
}

}  // namespace inferlab::advanced_serving_internal
