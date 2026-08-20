#pragma once

#include "artifact_semantic_index.hpp"

#include <cstddef>
#include <string>

namespace inferlab_artifact_profile {

bool exact_number(const std::string& document,
                  const inferlab_artifact::ArtifactSemanticIndex& index,
                  const std::string& key, double& value);
bool exact_string(const std::string& document,
                  const inferlab_artifact::ArtifactSemanticIndex& index,
                  const std::string& key, std::string& value);
bool exact_bool(const std::string& document,
                const inferlab_artifact::ArtifactSemanticIndex& index,
                const std::string& key, bool& value);
bool exact_non_negative_int(
    const std::string& document,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& key, int& value);
bool exact_size(const std::string& document,
                const inferlab_artifact::ArtifactSemanticIndex& index,
                const std::string& key, std::size_t& value);
bool empty_issues_array(const std::string& section);

}  // namespace inferlab_artifact_profile
