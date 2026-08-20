#pragma once

#include "disaggregated_serving_model.hpp"

#include <string>

namespace inferlab::disaggregated_serving {

std::string artifact_json(const Plan& plan);

}  // namespace inferlab::disaggregated_serving
