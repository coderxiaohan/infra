#pragma once

#include "release_kv_budget_profile.hpp"

#include <string>

namespace inferlab_release_kv_budget {

std::string binding_json(const Binding& binding, int indent);

}  // namespace inferlab_release_kv_budget
