#pragma once

#include "scheduler_admission_policy_sweep_model.hpp"

#include <string>

namespace inferlab::scheduler_admission_policy {

std::string artifact_json(const SweepPlan& plan);

}  // namespace inferlab::scheduler_admission_policy
