#pragma once

#include "speculative_decode_plan_model.hpp"

#include <string>

namespace inferlab::speculative_decode_plan {

std::string artifact_json(const Plan& plan);

}  // namespace inferlab::speculative_decode_plan
