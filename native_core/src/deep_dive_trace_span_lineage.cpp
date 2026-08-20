#include "deep_dive_trace_span_lineage.hpp"

#include "checked_index.hpp"
#include "trace_correlation_contract.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab_deep_dive_trace {
namespace {

struct RequestRoot {
  std::size_t source_index = 0U;
  std::size_t request_ordinal = 0U;
};

void append_unique(std::vector<std::string>& values,
                   const std::string& value) {
  if (value.empty()) return;
  if (std::find(values.begin(), values.end(), value) == values.end()) {
    values.push_back(value);
  }
}

void increment(std::size_t& value, const char* context) {
  value = inferlab::internal::checked_add_size(value, 1U, context);
}

bool lineage_sources_ready(const TraceTokenJoinProjection& projection) {
  return !projection.sources.empty() &&
         std::all_of(
             projection.sources.begin(), projection.sources.end(),
             [](const TraceTokenSourceObservation& source) {
               return source.span_lineage_contract_evaluated &&
                      source.span_lineage_contract_ready;
             });
}

void set_unresolved(NativeTraceReference& record,
                    const std::string& relation) {
  record.same_request_lineage_proven = false;
  record.matched_product_source_index = 0U;
  record.matched_product_request_ordinal = 0U;
  record.request_lineage_depth = 0U;
  record.request_lineage_relation = relation;
}

void set_resolved(NativeTraceReference& record,
                  const RequestRoot& root,
                  const std::size_t depth) {
  record.same_request_lineage_proven = true;
  record.matched_product_source_index = root.source_index;
  record.matched_product_request_ordinal = root.request_ordinal;
  record.request_lineage_depth = depth;
  record.request_lineage_relation =
      depth == 0U ? "request_root_span"
                  : depth == 1U ? "direct_child_of_request_span"
                                : "descendant_of_request_span";
}

bool resolve_request_root(
    const std::string& token,
    const std::map<std::string, std::vector<RequestRoot>>& roots,
    RequestRoot& root, bool& ambiguous) {
  const auto found = roots.find(token);
  if (found == roots.end()) return false;
  ambiguous = found->second.size() != 1U;
  if (!ambiguous) root = found->second.front();
  return true;
}

void resolve_native_record(
    TraceTokenCandidateGroup& group, const std::size_t record_index,
    const std::map<std::string, std::vector<RequestRoot>>& roots,
    const std::map<std::string, std::vector<std::size_t>>& native_nodes) {
  auto& output = group.native_records.at(record_index);
  const auto duplicate = native_nodes.find(output.span_join_token);
  if (duplicate == native_nodes.end() || duplicate->second.size() != 1U) {
    set_unresolved(output, "duplicate_or_missing_native_span_token");
    increment(group.lineage_conflict_native_record_count,
              "trace lineage duplicate native span count");
    return;
  }

  std::set<std::size_t> visited;
  std::size_t current_index = record_index;
  std::size_t depth = 0U;
  while (visited.insert(current_index).second) {
    const auto& current = group.native_records.at(current_index);
    RequestRoot root;
    bool ambiguous_root = false;
    if (resolve_request_root(current.span_join_token, roots, root,
                             ambiguous_root)) {
      if (ambiguous_root) {
        set_unresolved(output, "ambiguous_product_request_span");
        increment(group.lineage_conflict_native_record_count,
                  "trace lineage ambiguous request root count");
      } else {
        set_resolved(output, root, depth);
        increment(group.lineage_resolved_native_record_count,
                  "trace lineage resolved native record count");
      }
      return;
    }
    if (!current.parent_span_present) {
      set_unresolved(output, "native_span_has_no_request_bound_parent");
      increment(group.lineage_unresolved_native_record_count,
                "trace lineage unbound native root count");
      return;
    }
    depth = inferlab::internal::checked_add_size(
        depth, 1U, "trace lineage parent depth");
    if (depth > group.native_records.size()) {
      throw std::logic_error("trace lineage depth exceeds native inventory");
    }
    if (resolve_request_root(current.parent_span_join_token, roots, root,
                             ambiguous_root)) {
      if (ambiguous_root) {
        set_unresolved(output, "ambiguous_product_request_span");
        increment(group.lineage_conflict_native_record_count,
                  "trace lineage ambiguous parent root count");
      } else {
        set_resolved(output, root, depth);
        increment(group.lineage_resolved_native_record_count,
                  "trace lineage resolved native record count");
      }
      return;
    }
    const auto parent = native_nodes.find(current.parent_span_join_token);
    if (parent == native_nodes.end()) {
      set_unresolved(output, "parent_span_not_present_in_candidate_group");
      increment(group.lineage_unresolved_native_record_count,
                "trace lineage missing parent count");
      return;
    }
    if (parent->second.size() != 1U) {
      set_unresolved(output, "parent_span_token_is_ambiguous");
      increment(group.lineage_conflict_native_record_count,
                "trace lineage ambiguous parent span count");
      return;
    }
    current_index = parent->second.front();
  }

  set_unresolved(output, "native_span_parent_cycle");
  increment(group.lineage_conflict_native_record_count,
            "trace lineage cycle count");
}

void evaluate_group(TraceTokenCandidateGroup& group) {
  std::map<std::string, std::vector<RequestRoot>> roots;
  for (const auto& request : group.product_requests) {
    if (!request.request_span_binding_present) continue;
    if (!inferlab_trace::valid_span_context_join_token(
            request.request_span_join_token)) {
      throw std::logic_error(
          "trace lineage received invalid Product request-span token");
    }
    roots[request.request_span_join_token].push_back(
        {request.source_index, request.request_ordinal});
    increment(group.request_span_bound_product_request_count,
              "trace lineage bound Product request count");
  }
  if (roots.empty()) {
    group.span_lineage_status = "pending_request_span_binding";
    for (auto& record : group.native_records) {
      set_unresolved(record, "request_span_binding_not_available");
      increment(group.lineage_unresolved_native_record_count,
                "trace lineage pending binding native count");
    }
    return;
  }

  std::map<std::string, std::vector<std::size_t>> native_nodes;
  for (std::size_t index = 0U; index < group.native_records.size();
       ++index) {
    const auto& record = group.native_records[index];
    if (!inferlab_trace::valid_span_context_join_token(
            record.span_join_token) ||
        (record.parent_span_present
             ? !inferlab_trace::valid_span_context_join_token(
                   record.parent_span_join_token)
             : record.parent_span_join_token != "none")) {
      throw std::logic_error(
          "trace lineage received invalid native span contract");
    }
    native_nodes[record.span_join_token].push_back(index);
  }

  group.span_lineage_evaluated = true;
  for (std::size_t index = 0U; index < group.native_records.size();
       ++index) {
    resolve_native_record(group, index, roots, native_nodes);
  }
  const std::size_t reconciled = inferlab::internal::checked_add_size(
      inferlab::internal::checked_add_size(
          group.lineage_resolved_native_record_count,
          group.lineage_unresolved_native_record_count,
          "trace lineage group resolved/unresolved partition"),
      group.lineage_conflict_native_record_count,
      "trace lineage group native partition");
  if (reconciled != group.native_records.size()) {
    throw std::logic_error("trace lineage group partition drift");
  }
  group.same_request_proven =
      group.one_product_request &&
      group.lineage_resolved_native_record_count ==
          group.native_records.size() &&
      group.lineage_unresolved_native_record_count == 0U &&
      group.lineage_conflict_native_record_count == 0U;
  group.span_lineage_status =
      group.same_request_proven
          ? "same_request_lineage_proven"
          : group.lineage_resolved_native_record_count ==
                    group.native_records.size()
                ? "native_records_resolved_to_explicit_request_spans"
                : group.lineage_resolved_native_record_count > 0U
                      ? "partial_request_span_lineage"
                      : group.lineage_conflict_native_record_count > 0U
                            ? "request_span_lineage_conflicted"
                            : "request_span_lineage_unresolved";
}

TraceSpanLineageDecision derive_lineage_decision(
    const TraceTokenJoinProjection& projection,
    const bool sources_ready) {
  if (projection.matched_groups.empty()) {
    return TraceSpanLineageDecision::PendingTraceCandidateGroups;
  }
  if (!sources_ready) {
    return TraceSpanLineageDecision::BlockedLineageContract;
  }
  if (projection.summary.request_span_bound_product_request_count == 0U) {
    return TraceSpanLineageDecision::PendingRequestSpanBindings;
  }
  if (projection.summary.same_request_lineage_proven_group_count ==
          projection.summary.matched_trace_group_count &&
      projection.summary.lineage_resolved_native_record_count ==
          projection.summary.matched_native_record_count) {
    return TraceSpanLineageDecision::SameRequestLineageEvidenceReady;
  }
  return TraceSpanLineageDecision::PartialSpanLineageEvidence;
}

bool is_conflict_relation(const std::string& relation) {
  return relation == "duplicate_or_missing_native_span_token" ||
         relation == "ambiguous_product_request_span" ||
         relation == "parent_span_token_is_ambiguous" ||
         relation == "native_span_parent_cycle";
}

bool is_unresolved_relation(const std::string& relation) {
  return relation == "native_span_has_no_request_bound_parent" ||
         relation == "parent_span_not_present_in_candidate_group" ||
         relation == "request_span_binding_not_available";
}

std::string expected_group_lineage_status(
    const TraceTokenCandidateGroup& group, const bool all_sources_ready,
    const std::size_t bound_requests, const std::size_t resolved_native,
    const std::size_t unresolved_native,
    const std::size_t conflict_native) {
  if (!all_sources_ready) return "blocked_lineage_source_contract";
  if (bound_requests == 0U) return "pending_request_span_binding";
  if (group.one_product_request &&
      resolved_native == group.native_records.size() &&
      unresolved_native == 0U && conflict_native == 0U) {
    return "same_request_lineage_proven";
  }
  if (resolved_native == group.native_records.size()) {
    return "native_records_resolved_to_explicit_request_spans";
  }
  if (resolved_native > 0U) return "partial_request_span_lineage";
  if (conflict_native > 0U) return "request_span_lineage_conflicted";
  return "request_span_lineage_unresolved";
}

}  // namespace

const char* trace_span_lineage_decision_name(
    const TraceSpanLineageDecision decision) {
  switch (decision) {
    case TraceSpanLineageDecision::PendingTraceCandidateGroups:
      return "pending_trace_candidate_groups";
    case TraceSpanLineageDecision::PendingRequestSpanBindings:
      return "pending_request_span_bindings";
    case TraceSpanLineageDecision::BlockedLineageContract:
      return "blocked_lineage_contract";
    case TraceSpanLineageDecision::PartialSpanLineageEvidence:
      return "partial_span_lineage_evidence";
    case TraceSpanLineageDecision::SameRequestLineageEvidenceReady:
      return "same_request_lineage_evidence_ready";
  }
  throw std::logic_error("unknown trace span-lineage decision");
}

void evaluate_trace_span_lineage(TraceTokenJoinProjection& projection) {
  projection.span_lineage_limitations = {
      "Request-to-span lineage requires explicit Product request_span_id/request_span_ids bindings and a unique native parent chain within one anonymous trace candidate group.",
      "Join tokens are non-cryptographic, invocation-scoped equality hints; lineage evidence is not authentication or globally unique identity.",
      "Declared parent-span reachability does not prove span start/end timing, causal direction, latency attribution, product execution, or release readiness."};
  projection.span_lineage_recommended_actions = {
      "emit stable request_span_id or aligned request_span_ids with Product Replay trace context",
      "ensure every native child span has a unique parent chain ending at the explicit Product request span",
      "retain causal and latency claims as blocked until span lifecycle timestamps and ordering contracts are available"};

  bool sources_ready = lineage_sources_ready(projection);
  for (const auto& source : projection.sources) {
    projection.summary.span_lineage_source_contract_issue_count =
        inferlab::internal::checked_add_size(
            projection.summary.span_lineage_source_contract_issue_count,
            source.span_lineage_issues.size(),
            "trace lineage source contract issue count");
    if (source.span_lineage_contract_ready) {
      increment(projection.summary.span_lineage_ready_source_count,
                "trace lineage ready source count");
    }
  }
  if (sources_ready) {
    for (auto& group : projection.matched_groups) {
      evaluate_group(group);
      projection.summary.request_span_bound_product_request_count =
          inferlab::internal::checked_add_size(
              projection.summary.request_span_bound_product_request_count,
              group.request_span_bound_product_request_count,
              "trace lineage Product binding total");
      projection.summary.lineage_resolved_native_record_count =
          inferlab::internal::checked_add_size(
              projection.summary.lineage_resolved_native_record_count,
              group.lineage_resolved_native_record_count,
              "trace lineage resolved native total");
      projection.summary.lineage_unresolved_native_record_count =
          inferlab::internal::checked_add_size(
              projection.summary.lineage_unresolved_native_record_count,
              group.lineage_unresolved_native_record_count,
              "trace lineage unresolved native total");
      projection.summary.lineage_conflict_native_record_count =
          inferlab::internal::checked_add_size(
              projection.summary.lineage_conflict_native_record_count,
              group.lineage_conflict_native_record_count,
              "trace lineage conflict native total");
      if (group.span_lineage_evaluated) {
        increment(projection.summary.span_lineage_evaluated_group_count,
                  "trace lineage evaluated group count");
      }
      if (group.same_request_proven) {
        increment(
            projection.summary.same_request_lineage_proven_group_count,
            "trace lineage same-request group count");
      }
    }
  } else {
    for (auto& group : projection.matched_groups) {
      group.span_lineage_status = "blocked_lineage_source_contract";
      for (auto& record : group.native_records) {
        set_unresolved(record, "lineage_source_contract_not_ready");
      }
    }
  }

  projection.summary.span_lineage_evaluation_performed =
      projection.summary.span_lineage_evaluated_group_count > 0U;
  projection.summary.span_lineage_decision =
      derive_lineage_decision(projection, sources_ready);
  if (!sources_ready) {
    append_unique(projection.blockers,
                  "span_lineage_source_contract_invalid");
    append_unique(projection.recommended_actions,
                  "refresh_product_and_native_trace_artifacts_with_request_span_lineage_contract");
  } else if (projection.summary.request_span_bound_product_request_count ==
             0U) {
    append_unique(projection.blockers,
                  "product_request_span_binding_missing");
  }
  if (projection.summary.lineage_unresolved_native_record_count > 0U) {
    append_unique(projection.blockers,
                  "native_spans_do_not_reach_an_explicit_product_request_span");
  }
  if (projection.summary.lineage_conflict_native_record_count > 0U) {
    append_unique(projection.blockers,
                  "native_span_lineage_is_ambiguous_or_cyclic");
  }
}

void validate_trace_span_lineage_projection(
    const TraceTokenJoinProjection& projection) {
  std::size_t source_issue_count = 0U;
  std::size_t ready_source_count = 0U;
  for (const auto& source : projection.sources) {
    if (source.span_lineage_contract_ready &&
        (!source.span_lineage_contract_evaluated ||
         !source.token_contract_ready ||
         !source.span_lineage_issues.empty())) {
      throw std::logic_error("trace lineage source readiness drift");
    }
    source_issue_count = inferlab::internal::checked_add_size(
        source_issue_count, source.span_lineage_issues.size(),
        "trace lineage validated source issue count");
    if (source.span_lineage_contract_ready) {
      increment(ready_source_count,
                "trace lineage validated ready source count");
    }
  }
  const bool all_sources_ready =
      !projection.sources.empty() &&
      ready_source_count == projection.sources.size();

  std::size_t bound_requests = 0U;
  std::size_t evaluated_groups = 0U;
  std::size_t proven_groups = 0U;
  std::size_t resolved_native = 0U;
  std::size_t unresolved_native = 0U;
  std::size_t conflict_native = 0U;
  for (const auto& group : projection.matched_groups) {
    std::set<std::pair<std::size_t, std::size_t>> product_references;
    std::size_t group_bound_requests = 0U;
    for (const auto& request : group.product_requests) {
      if (!product_references
               .emplace(request.source_index, request.request_ordinal)
               .second) {
        throw std::logic_error(
            "trace lineage Product reference is duplicated within a group");
      }
      if (all_sources_ready && request.request_span_binding_present) {
        increment(group_bound_requests,
                  "trace lineage validated group Product binding count");
      }
      if (all_sources_ready &&
          (request.request_span_binding_present !=
               (request.request_span_join_token != "none") ||
           (request.request_span_binding_present &&
            !inferlab_trace::valid_span_context_join_token(
                request.request_span_join_token)))) {
        throw std::logic_error("trace lineage Product reference drift");
      }
    }

    std::size_t group_resolved_native = 0U;
    std::size_t group_unresolved_native = 0U;
    std::size_t group_conflict_native = 0U;
    for (const auto& record : group.native_records) {
      if (all_sources_ready &&
          (!inferlab_trace::valid_span_context_join_token(
               record.span_join_token) ||
           (record.parent_span_present !=
            (record.parent_span_join_token != "none")) ||
           (record.parent_span_present &&
            !inferlab_trace::valid_span_context_join_token(
                record.parent_span_join_token)))) {
        throw std::logic_error("trace lineage native span contract drift");
      }
      if (record.same_request_lineage_proven) {
        const auto reference = std::make_pair(
            record.matched_product_source_index,
            record.matched_product_request_ordinal);
        if (record.matched_product_request_ordinal == 0U ||
            product_references.find(reference) == product_references.end() ||
            record.request_lineage_depth > group.native_records.size() ||
            record.request_lineage_relation !=
                (record.request_lineage_depth == 0U
                     ? "request_root_span"
                     : record.request_lineage_depth == 1U
                           ? "direct_child_of_request_span"
                           : "descendant_of_request_span")) {
          throw std::logic_error(
              "trace lineage resolved native reference drift");
        }
        increment(group_resolved_native,
                  "trace lineage validated group resolved count");
      } else {
        if (record.matched_product_source_index != 0U ||
            record.matched_product_request_ordinal != 0U ||
            record.request_lineage_depth != 0U) {
          throw std::logic_error(
              "trace lineage unresolved native reference drift");
        }
        if (!all_sources_ready) {
          if (record.request_lineage_relation !=
              "lineage_source_contract_not_ready") {
            throw std::logic_error(
                "trace lineage blocked-source relation drift");
          }
        } else if (is_conflict_relation(record.request_lineage_relation)) {
          increment(group_conflict_native,
                    "trace lineage validated group conflict count");
        } else if (is_unresolved_relation(record.request_lineage_relation)) {
          increment(group_unresolved_native,
                    "trace lineage validated group unresolved count");
        } else {
          throw std::logic_error(
              "trace lineage unresolved relation is not recognized");
        }
      }
    }

    const bool expected_evaluated =
        all_sources_ready && group_bound_requests > 0U;
    const bool expected_same_request =
        expected_evaluated && group.one_product_request &&
        group_resolved_native == group.native_records.size() &&
        group_unresolved_native == 0U && group_conflict_native == 0U;
    if (group.request_span_bound_product_request_count !=
            group_bound_requests ||
        group.lineage_resolved_native_record_count !=
            group_resolved_native ||
        group.lineage_unresolved_native_record_count !=
            group_unresolved_native ||
        group.lineage_conflict_native_record_count !=
            group_conflict_native ||
        group.span_lineage_evaluated != expected_evaluated ||
        group.same_request_proven != expected_same_request ||
        group.span_lineage_status != expected_group_lineage_status(
            group, all_sources_ready, group_bound_requests,
            group_resolved_native, group_unresolved_native,
            group_conflict_native)) {
      throw std::logic_error("trace lineage group reconciliation failed");
    }

    bound_requests = inferlab::internal::checked_add_size(
        bound_requests, group.request_span_bound_product_request_count,
        "trace lineage validated Product binding total");
    resolved_native = inferlab::internal::checked_add_size(
        resolved_native, group.lineage_resolved_native_record_count,
        "trace lineage validated resolved total");
    unresolved_native = inferlab::internal::checked_add_size(
        unresolved_native, group.lineage_unresolved_native_record_count,
        "trace lineage validated unresolved total");
    conflict_native = inferlab::internal::checked_add_size(
        conflict_native, group.lineage_conflict_native_record_count,
        "trace lineage validated conflict total");
    if (group.span_lineage_evaluated) {
      increment(evaluated_groups,
                "trace lineage validated evaluated group count");
    }
    if (group.same_request_proven) {
      increment(proven_groups,
                "trace lineage validated proven group count");
    }
  }

  if (source_issue_count !=
          projection.summary.span_lineage_source_contract_issue_count ||
      ready_source_count !=
          projection.summary.span_lineage_ready_source_count ||
      bound_requests !=
          projection.summary.request_span_bound_product_request_count ||
      evaluated_groups !=
          projection.summary.span_lineage_evaluated_group_count ||
      proven_groups !=
          projection.summary.same_request_lineage_proven_group_count ||
      resolved_native !=
          projection.summary.lineage_resolved_native_record_count ||
      unresolved_native !=
          projection.summary.lineage_unresolved_native_record_count ||
      conflict_native !=
          projection.summary.lineage_conflict_native_record_count ||
      projection.summary.span_lineage_evaluation_performed !=
          (evaluated_groups > 0U) ||
      projection.summary.span_lineage_decision !=
          derive_lineage_decision(projection, all_sources_ready)) {
    throw std::logic_error("trace lineage summary reconciliation failed");
  }
}

}  // namespace inferlab_deep_dive_trace
