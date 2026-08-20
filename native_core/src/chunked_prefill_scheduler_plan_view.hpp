#pragma once

#include "chunked_prefill_scheduler_plan_model.hpp"

#include <string>

namespace inferlab::chunked_prefill_scheduler_plan {

std::string artifact_json(const Plan& plan);

}  // namespace inferlab::chunked_prefill_scheduler_plan
