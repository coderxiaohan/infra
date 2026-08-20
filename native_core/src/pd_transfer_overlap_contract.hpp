#pragma once

#include "inferlab/advanced_serving_runtime_communication.hpp"

namespace inferlab::pd_transfer_overlap_contract {

inline constexpr double kComparisonTolerance = 1e-9;

void validate_assumptions(
    const PdTransferOverlapAssumptions& assumptions);

PdTransferOverlapScenario validate_scenario(
    PdTransferOverlapScenario scenario);

bool same_scenario(const PdTransferOverlapScenario& left,
                   const PdTransferOverlapScenario& right);

int chunk_tokens_at(int total_tokens,
                    int chunk_tokens,
                    int chunk_index,
                    const char* name);

void validate_result(const PdTransferOverlapResult& result);

}  // namespace inferlab::pd_transfer_overlap_contract
