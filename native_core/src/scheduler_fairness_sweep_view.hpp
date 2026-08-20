#pragma once

#include "scheduler_fairness_sweep_model.hpp"

#include <string>

namespace inferlab::scheduler_fairness {

std::string artifact_json(const SweepPlan& plan);

}  // namespace inferlab::scheduler_fairness
