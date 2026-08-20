#pragma once

#include "evidence_query_record_policy.hpp"

#include <string>
#include <vector>

namespace inferlab_evidence_query {

struct QueryOperation;

struct PositionalQueryArgs {
  std::string operation;
  std::string selector;
  std::string target;
};

PositionalQueryArgs parse_positional_query_args(
    const std::vector<std::string>& args);

QueryOperation parse_query_operation(const std::string& query,
                                     const std::string& artifact_filter,
                                     const std::string& blocker_filter);

}  // namespace inferlab_evidence_query
