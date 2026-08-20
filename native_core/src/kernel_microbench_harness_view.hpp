#pragma once

#include "kernel_microbench_harness_model.hpp"

#include <string>

namespace inferlab::kernel_microbench {

std::string harness_artifact_json(const HarnessPlan& plan);

}  // namespace inferlab::kernel_microbench
