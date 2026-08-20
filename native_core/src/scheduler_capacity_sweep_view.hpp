#pragma once

#include "scheduler_capacity_sweep_model.hpp"

#include <string>

namespace inferlab::scheduler_capacity_sweep {

std::string artifact_json(const SweepPlan& plan);

}  // namespace inferlab::scheduler_capacity_sweep
