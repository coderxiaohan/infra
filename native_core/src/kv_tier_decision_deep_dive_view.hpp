#pragma once

#include "kv_tier_decision_deep_dive_model.hpp"

#include <string>
#include <vector>

namespace inferlab::kv_tier_decision_deep_dive {

std::string tier_candidate_json(const TierCandidate& candidate, int indent);
std::string tier_candidates_json(
    const std::vector<TierCandidate>& candidates, int indent);
std::string artifact_json(const Plan& plan);

}  // namespace inferlab::kv_tier_decision_deep_dive
