#pragma once

#include "inferlab/advanced_serving_runtime_communication.hpp"

namespace inferlab::kv_transport_replay_contract {

inline constexpr double kComparisonTolerance = 1e-9;

void validate_assumptions(
    const KvTransportReplayAssumptions& assumptions);

KvTransportReplayResult build_result(
    const KvTransportReplayAssumptions& assumptions,
    const KvTransportReplayScenario& scenario);

}  // namespace inferlab::kv_transport_replay_contract
