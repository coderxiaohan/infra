#pragma once

#include "kserve_llmd_plan_model.hpp"

#include <string>
#include <vector>

namespace inferlab_kserve_view {

const std::vector<std::string>& field_metric_mapping();

std::vector<std::string> migration_commands(
    const inferlab::KserveLlmdManifestResult& result);

std::string scenario_json(
    const inferlab::KserveLlmdManifestScenario& scenario,
    int indent);

std::string record_json(
    const inferlab::KserveLlmdManifestResult& result,
    int indent);

std::string artifact_json(
    const inferlab::kserve_llmd_plan::Plan& plan);

std::string deployment_report(
    const inferlab::kserve_llmd_plan::Plan& plan);

}  // namespace inferlab_kserve_view
