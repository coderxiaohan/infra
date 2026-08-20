#pragma once

#include "native_core_audit_register_model.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace inferlab_native_audit {

const std::vector<IssueSpec>& issue_specs();

bool audit_has_issue(const FileAudit& audit, const std::string& issue);
std::size_t file_completion_blocker_count_for(const FileAudit& audit);
std::string file_completion_state_for(const FileAudit& audit);
std::string file_completion_blocker_category_for(const FileAudit& audit);

int review_priority_rank(const FileAudit& audit);
std::string review_priority_label(int rank);
std::string review_lane_for(const FileAudit& audit);
std::string audit_phase_for(const FileAudit& audit);

std::string primary_issue_for(const FileAudit& audit);
std::string current_repo_action_for_file(const FileAudit& audit);
std::string target_machine_action_for_file(const FileAudit& audit);
bool is_open_review_item(const FileAudit& audit);
bool open_review_item_pointer_less(const FileAudit* lhs,
                                   const FileAudit* rhs);

}  // namespace inferlab_native_audit
