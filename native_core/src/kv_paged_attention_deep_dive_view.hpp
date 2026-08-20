#pragma once

#include "kv_paged_attention_deep_dive_model.hpp"

#include <string>

namespace inferlab::kv_paged_attention_deep_dive {

std::string mapping_samples_json(
    const std::vector<MappingSample>& samples, int indent);
std::string artifact_json(const Plan& plan);

}  // namespace inferlab::kv_paged_attention_deep_dive
