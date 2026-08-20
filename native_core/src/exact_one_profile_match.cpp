#include "exact_one_profile_match.hpp"

#include "checked_index.hpp"
#include "plan_json_utils.hpp"

#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace inferlab_profile_match {

Result exact_one(const std::vector<Candidate>& candidates,
                 const std::string& missing_issue,
                 const std::string& ambiguous_issue) {
  if (candidates.empty() || missing_issue.empty() || ambiguous_issue.empty() ||
      missing_issue == ambiguous_issue) {
    throw std::invalid_argument(
        "exact-one profile match contract is invalid");
  }
  Result result;
  std::unordered_set<std::string> identities;
  identities.reserve(candidates.size());
  for (const auto& candidate : candidates) {
    if (candidate.identity.empty() ||
        !identities.insert(candidate.identity).second) {
      throw std::invalid_argument(
          "exact-one profile candidate identities must be unique");
    }
    if (!candidate.matches) continue;
    result.match_count = inferlab::internal::checked_add_size(
        result.match_count, 1U, "exact-one profile match count");
    if (result.matched_identity.empty()) {
      result.matched_identity = candidate.identity;
    }
  }
  result.matched = result.match_count == 1U;
  if (result.match_count == 0U) {
    inferlab_plan::append_unique_nonempty_string(result.issues,
                                                  missing_issue);
  } else if (result.match_count > 1U) {
    inferlab_plan::append_unique_nonempty_string(result.issues,
                                                  ambiguous_issue);
  }
  if (!result.matched) result.matched_identity.clear();
  return result;
}

}  // namespace inferlab_profile_match
