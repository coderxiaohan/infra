#pragma once

#include "scheduler_pressure_sweep_model.hpp"

#include <string>

namespace inferlab::scheduler_pressure_sweep {

std::string artifact_json(const SweepPlan& plan);

}  // namespace inferlab::scheduler_pressure_sweep
