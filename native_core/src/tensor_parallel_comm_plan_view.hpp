#pragma once

#include "tensor_parallel_comm_plan_model.hpp"

#include <string>

namespace inferlab::tensor_parallel_comm {

std::string artifact_json(const Plan& plan);

}  // namespace inferlab::tensor_parallel_comm
