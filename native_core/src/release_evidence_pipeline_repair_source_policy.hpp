#pragma once

#include <string>
#include <vector>

namespace inferlab_release_pipeline {

std::vector<std::string> repair_path_source_locations_for(
    const std::string& category,
    const std::string& source);

}  // namespace inferlab_release_pipeline
