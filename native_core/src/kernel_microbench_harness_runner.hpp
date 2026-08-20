#pragma once

#include "kernel_microbench_harness_model.hpp"

namespace inferlab::kernel_microbench {

BuildCapabilities compiled_harness_capabilities();

void execute_harness_plan(HarnessPlan& plan);

}  // namespace inferlab::kernel_microbench
