#pragma once

#include "scheduler_workload_sweep_model.hpp"

#include <string>

namespace inferlab::scheduler_workload_sweep {

std::string artifact_json(const SweepPlan& plan);

}  // namespace inferlab::scheduler_workload_sweep
