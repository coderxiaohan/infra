#include "inferlab/vector_add.hpp"

#include <stdexcept>

#include "checked_index.hpp"

namespace inferlab {

std::vector<float> vector_add_cpu(const std::vector<float>& a,
                                  const std::vector<float>& b) {
  if (a.size() != b.size()) {
    throw std::invalid_argument("vector_add_cpu inputs must have equal sizes");
  }
  internal::validate_all_finite(a, "vector_add_cpu lhs");
  internal::validate_all_finite(b, "vector_add_cpu rhs");
  std::vector<float> output(a.size(), 0.0F);
  for (std::size_t i = 0; i < a.size(); ++i) {
    output[i] = internal::checked_float_from_double(
        static_cast<double>(a[i]) + static_cast<double>(b[i]),
        "vector_add_cpu output");
  }
  return output;
}

}  // namespace inferlab
