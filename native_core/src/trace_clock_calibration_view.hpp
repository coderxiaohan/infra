#pragma once

#include "trace_clock_calibration.hpp"

#include <string>

namespace inferlab_trace_clock {

std::string clock_calibration_usage_json(
    const CalibrationUsageProjection& projection, int indent);

}  // namespace inferlab_trace_clock
