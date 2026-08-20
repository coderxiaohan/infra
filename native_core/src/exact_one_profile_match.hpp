#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_profile_match {

struct Candidate {
  std::string identity;
  bool matches = false;
};

struct Result {
  std::string matched_identity;
  std::size_t match_count = 0U;
  bool matched = false;
  std::vector<std::string> issues;
};

Result exact_one(const std::vector<Candidate>& candidates,
                 const std::string& missing_issue,
                 const std::string& ambiguous_issue);

}  // namespace inferlab_profile_match
