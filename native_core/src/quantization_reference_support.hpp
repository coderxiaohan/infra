#pragma once

#include "inferlab/quantization.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab::quantization_reference {

struct PeriodicFixtureSpec {
  int row_factor = 0;
  int col_factor = 0;
  int period = 1;
  int center = 0;
  float divisor = 1.0F;
};

struct Result {
  QuantizedMatrix matrix;
  QuantizationErrorStats error;
  std::size_t storage_bytes = 0U;
};

struct Summary {
  QuantizationErrorStats error;
  std::size_t scale_count = 0U;
  std::size_t storage_bytes = 0U;
};

std::vector<float> build_periodic_fixture(
    int rows, int cols, const PeriodicFixtureSpec& spec,
    const std::string& label);

Result run_group_reference(const std::vector<float>& weights,
                           int rows, int cols, int group_size,
                           QuantizationBits bits);
Result run_channel_reference(const std::vector<float>& weights,
                             int rows, int cols,
                             QuantizationBits bits);

bool same_error_stats(const QuantizationErrorStats& lhs,
                      const QuantizationErrorStats& rhs);
bool same_result(const Result& lhs, const Result& rhs);
Summary summarize(const Result& result);
bool same_summary(const Summary& lhs, const Summary& rhs);

}  // namespace inferlab::quantization_reference
