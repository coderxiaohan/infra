#!/usr/bin/env python3
"""Typed Final Report projections for release-facing audit evidence."""

from __future__ import annotations

from dataclasses import dataclass
from functools import partial
from typing import Any, Callable, Dict, Mapping, Sequence, Tuple

from evidence_projection import (
    dict_value,
    first_present,
    list_value,
    missing_preserving_default,
)


NATIVE_AUDIT_COVERAGE_CLAIM_BOUNDARY = (
    "Root-cause native-core audit coverage review is static source-review "
    "metadata only; it does not mark files reviewed, run native binaries, "
    "or prove target-machine readiness."
)
NATIVE_AUDIT_MANIFEST_CLAIM_BOUNDARY = (
    "Root-cause native-core audit manifest review is static manifest-policy "
    "metadata only; it does not execute reviewed sources, run native "
    "binaries, or prove target-machine readiness."
)
CATALOG_FLOW_SHORTEST_PATH_CLAIM_BOUNDARY = (
    "Root-cause catalog fallback-removal shortest-path review is "
    "release-gate metadata only; it does not delete fallback lists, "
    "refresh artifacts, run validation, or prove downstream parity."
)
CATALOG_FLOW_PARITY_GATE_CLAIM_BOUNDARY = (
    "Root-cause catalog fallback-removal parity gate is release-review "
    "metadata only; it does not remove fallback lists, refresh artifacts, "
    "run validation, or prove downstream parity."
)
CATALOG_FLOW_GOVERNANCE_CLAIM_BOUNDARY = (
    "Root-cause catalog-flow governance is release-review metadata only; "
    "it does not repair catalog wiring, remove fallback surfaces, refresh "
    "artifacts, run validation, or prove downstream parity."
)


_REPAIR_PATH_DIAGNOSTIC_SCOPES = (
    ("all", ()),
    ("catalog_flow", ("catalog_flow_discrepancy",)),
    (
        "catalog_flow_fallback_removal_shortest_path",
        ("catalog_flow_fallback_removal_shortest_path",),
    ),
    (
        "catalog_flow_fallback_removal_parity_gate",
        ("catalog_flow_fallback_removal_parity_gate",),
    ),
    (
        "evidence_query_snapshot_diff",
        ("evidence_query_snapshot_diff_review",),
    ),
    (
        "native_core_audit",
        ("native_core_audit_backlog", "native_core_audit_scope_blocked"),
    ),
    ("native_core_audit_scope", ("native_core_audit_scope_blocked",)),
)

_repair_path_diagnostic_scope_names = tuple(
    name for name, _ in _REPAIR_PATH_DIAGNOSTIC_SCOPES
)
if len(_repair_path_diagnostic_scope_names) != len(
    set(_repair_path_diagnostic_scope_names)
):
    raise ValueError("repair-path diagnostic scopes contain duplicate names")
if _REPAIR_PATH_DIAGNOSTIC_SCOPES[0] != ("all", ()):
    raise ValueError("repair-path diagnostic scopes must start with all")
if any(
    not name
    or (name != "all" and not categories)
    or any(not category for category in categories)
    for name, categories in _REPAIR_PATH_DIAGNOSTIC_SCOPES
):
    raise ValueError("repair-path diagnostic scopes contain invalid metadata")


# scope, output field, closure field, release-summary field, diagnostic field
_REPAIR_PATH_RELEASE_FIELD_SPECS = (
    (
        "all",
        "repair_path_count",
        "repair_path_count",
        "non_gpu_repair_path_count",
        "path_count",
    ),
    (
        "all",
        "repair_path_source_location_count",
        "",
        "non_gpu_repair_path_source_location_count",
        "source_location_path_count",
    ),
    (
        "all",
        "repair_path_source_location_gap_count",
        "",
        "non_gpu_repair_path_source_location_gap_count",
        "source_location_gap_count",
    ),
    (
        "all",
        "repair_path_evidence_counter_count",
        "",
        "non_gpu_repair_path_evidence_counter_count",
        "evidence_counter_count",
    ),
    (
        "all",
        "first_repair_path_source_location",
        "",
        "",
        "first_source_location",
    ),
    (
        "catalog_flow",
        "catalog_flow_repair_path_count",
        "",
        "catalog_flow_repair_path_count",
        "path_count",
    ),
    (
        "catalog_flow",
        "catalog_flow_repair_path_source_location_count",
        "",
        "catalog_flow_repair_path_source_location_count",
        "source_location_path_count",
    ),
    (
        "catalog_flow",
        "catalog_flow_repair_path_source_location_gap_count",
        "",
        "catalog_flow_repair_path_source_location_gap_count",
        "source_location_gap_count",
    ),
    (
        "catalog_flow",
        "catalog_flow_repair_path_evidence_counter_count",
        "",
        "catalog_flow_repair_path_evidence_counter_count",
        "evidence_counter_count",
    ),
    (
        "catalog_flow",
        "catalog_flow_first_source_location",
        "",
        "",
        "first_source_location",
    ),
    (
        "catalog_flow_fallback_removal_shortest_path",
        "catalog_flow_fallback_removal_shortest_path_repair_path_count",
        "",
        "catalog_flow_fallback_removal_shortest_path_repair_path_count",
        "path_count",
    ),
    (
        "catalog_flow_fallback_removal_shortest_path",
        "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_count",
        "",
        "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_count",
        "source_location_path_count",
    ),
    (
        "catalog_flow_fallback_removal_shortest_path",
        "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_gap_count",
        "",
        "catalog_flow_fallback_removal_shortest_path_repair_path_source_location_gap_count",
        "source_location_gap_count",
    ),
    (
        "catalog_flow_fallback_removal_shortest_path",
        "catalog_flow_fallback_removal_shortest_path_repair_path_evidence_counter_count",
        "",
        "catalog_flow_fallback_removal_shortest_path_repair_path_evidence_counter_count",
        "evidence_counter_count",
    ),
    (
        "catalog_flow_fallback_removal_shortest_path",
        "catalog_flow_fallback_removal_shortest_path_first_source_location",
        "",
        "",
        "first_source_location",
    ),
    (
        "catalog_flow_fallback_removal_parity_gate",
        "catalog_flow_fallback_removal_parity_gate_repair_path_count",
        "",
        "catalog_flow_fallback_removal_parity_gate_repair_path_count",
        "path_count",
    ),
    (
        "catalog_flow_fallback_removal_parity_gate",
        "catalog_flow_fallback_removal_parity_gate_repair_path_source_location_count",
        "",
        "catalog_flow_fallback_removal_parity_gate_repair_path_source_location_count",
        "source_location_path_count",
    ),
    (
        "catalog_flow_fallback_removal_parity_gate",
        "catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count",
        "",
        "catalog_flow_fallback_removal_parity_gate_repair_path_source_location_gap_count",
        "source_location_gap_count",
    ),
    (
        "catalog_flow_fallback_removal_parity_gate",
        "catalog_flow_fallback_removal_parity_gate_repair_path_evidence_counter_count",
        "",
        "catalog_flow_fallback_removal_parity_gate_repair_path_evidence_counter_count",
        "evidence_counter_count",
    ),
    (
        "catalog_flow_fallback_removal_parity_gate",
        "catalog_flow_fallback_removal_parity_gate_first_source_location",
        "",
        "",
        "first_source_location",
    ),
    (
        "evidence_query_snapshot_diff",
        "evidence_query_snapshot_diff_repair_path_count",
        "",
        "evidence_query_snapshot_diff_repair_path_count",
        "path_count",
    ),
    (
        "evidence_query_snapshot_diff",
        "evidence_query_snapshot_diff_repair_path_source_location_count",
        "",
        "evidence_query_snapshot_diff_repair_path_source_location_count",
        "source_location_path_count",
    ),
    (
        "evidence_query_snapshot_diff",
        "evidence_query_snapshot_diff_repair_path_source_location_gap_count",
        "",
        "evidence_query_snapshot_diff_repair_path_source_location_gap_count",
        "source_location_gap_count",
    ),
    (
        "evidence_query_snapshot_diff",
        "evidence_query_snapshot_diff_repair_path_evidence_counter_count",
        "",
        "evidence_query_snapshot_diff_repair_path_evidence_counter_count",
        "evidence_counter_count",
    ),
    (
        "evidence_query_snapshot_diff",
        "evidence_query_snapshot_diff_first_source_location",
        "",
        "",
        "first_source_location",
    ),
    (
        "native_core_audit",
        "native_core_audit_repair_path_count",
        "",
        "native_core_audit_repair_path_count",
        "path_count",
    ),
    (
        "native_core_audit",
        "native_core_audit_repair_path_source_location_count",
        "",
        "native_core_audit_repair_path_source_location_count",
        "source_location_path_count",
    ),
    (
        "native_core_audit",
        "native_core_audit_repair_path_source_location_gap_count",
        "",
        "native_core_audit_repair_path_source_location_gap_count",
        "source_location_gap_count",
    ),
    (
        "native_core_audit",
        "native_core_audit_repair_path_evidence_counter_count",
        "",
        "native_core_audit_repair_path_evidence_counter_count",
        "evidence_counter_count",
    ),
    (
        "native_core_audit",
        "native_core_audit_first_source_location",
        "",
        "",
        "first_source_location",
    ),
    (
        "native_core_audit_scope",
        "native_core_audit_scope_repair_path_count",
        "",
        "native_core_audit_scope_repair_path_count",
        "path_count",
    ),
    (
        "native_core_audit_scope",
        "native_core_audit_scope_repair_path_source_location_gap_count",
        "",
        "native_core_audit_scope_repair_path_source_location_gap_count",
        "source_location_gap_count",
    ),
    (
        "native_core_audit_scope",
        "native_core_audit_scope_repair_path_evidence_counter_count",
        "",
        "native_core_audit_scope_repair_path_evidence_counter_count",
        "evidence_counter_count",
    ),
)

_repair_path_release_output_fields = tuple(
    output_field
    for _, output_field, _, _, _ in _REPAIR_PATH_RELEASE_FIELD_SPECS
)
if len(_repair_path_release_output_fields) != len(
    set(_repair_path_release_output_fields)
):
    raise ValueError("repair-path release fields contain duplicate outputs")
_repair_path_diagnostic_field_names = {
    "path_count",
    "source_location_path_count",
    "source_location_gap_count",
    "first_source_location",
    "evidence_counter_count",
}
if any(
    scope_name not in _repair_path_diagnostic_scope_names
    or not output_field
    or diagnostic_field not in _repair_path_diagnostic_field_names
    for scope_name, output_field, _, _, diagnostic_field
    in _REPAIR_PATH_RELEASE_FIELD_SPECS
):
    raise ValueError("repair-path release fields contain invalid metadata")


@dataclass(frozen=True)
class RepairPathDiagnostic:
    path_count: int
    source_location_path_count: int
    source_location_count: int
    source_location_gap_count: int
    first_source_location: Any
    evidence_counter_path_count: int
    evidence_counter_count: int
    evidence_counter_gap_count: int

    def to_dict(self) -> Dict[str, Any]:
        return {
            "path_count": self.path_count,
            "source_location_path_count": self.source_location_path_count,
            "source_location_count": self.source_location_count,
            "source_location_gap_count": self.source_location_gap_count,
            "first_source_location": self.first_source_location,
            "evidence_counter_path_count": self.evidence_counter_path_count,
            "evidence_counter_count": self.evidence_counter_count,
            "evidence_counter_gap_count": self.evidence_counter_gap_count,
        }


@dataclass(frozen=True)
class FinalReportRepairPathDiagnosticsProjection:
    scopes: Mapping[str, RepairPathDiagnostic]

    def scope(self, name: str) -> RepairPathDiagnostic:
        try:
            return self.scopes[name]
        except KeyError as exc:
            raise ValueError(f"unknown repair-path diagnostic scope: {name}") from exc

    def to_dict(self) -> Dict[str, Dict[str, Any]]:
        return {
            name: self.scope(name).to_dict()
            for name, _ in _REPAIR_PATH_DIAGNOSTIC_SCOPES
        }

    def to_release_pipeline_dict(
        self,
        release_summary: Mapping[str, Any],
        closure: Mapping[str, Any],
    ) -> Dict[str, Any]:
        """Project canonical release-facing repair-path diagnostic fields."""
        projected: Dict[str, Any] = {}
        for (
            scope_name,
            output_field,
            closure_field,
            release_field,
            diagnostic_field,
        ) in _REPAIR_PATH_RELEASE_FIELD_SPECS:
            value = getattr(self.scope(scope_name), diagnostic_field)
            if release_field:
                value = first_present(release_summary.get(release_field), value)
            if closure_field:
                value = first_present(closure.get(closure_field), value)
            projected[output_field] = value
        return projected


def project_final_report_repair_path_diagnostics(
    repair_paths: Sequence[Any],
) -> FinalReportRepairPathDiagnosticsProjection:
    """Project all Final Report repair-path diagnostics in one bounded pass."""
    accumulators: Dict[str, Dict[str, Any]] = {
        name: {
            "path_count": 0,
            "source_location_path_count": 0,
            "source_location_count": 0,
            "first_source_location": None,
            "evidence_counter_path_count": 0,
            "evidence_counter_count": 0,
        }
        for name, _ in _REPAIR_PATH_DIAGNOSTIC_SCOPES
    }

    for item in repair_paths:
        if not isinstance(item, dict):
            continue
        category = item.get("category")
        source_locations = [
            str(value) for value in list_value(item.get("source_locations"))
        ]
        evidence_counters = [
            value
            for value in list_value(item.get("evidence_counters"))
            if isinstance(value, dict)
        ]
        for scope_name, categories in _REPAIR_PATH_DIAGNOSTIC_SCOPES:
            if categories and category not in categories:
                continue
            accumulator = accumulators[scope_name]
            accumulator["path_count"] += 1
            if source_locations:
                accumulator["source_location_path_count"] += 1
                accumulator["source_location_count"] += len(source_locations)
                if accumulator["first_source_location"] is None:
                    accumulator["first_source_location"] = source_locations[0]
            if evidence_counters:
                accumulator["evidence_counter_path_count"] += 1
                accumulator["evidence_counter_count"] += len(evidence_counters)

    diagnostics: Dict[str, RepairPathDiagnostic] = {}
    for scope_name, _ in _REPAIR_PATH_DIAGNOSTIC_SCOPES:
        accumulator = accumulators[scope_name]
        path_count = accumulator["path_count"]
        source_location_path_count = accumulator["source_location_path_count"]
        evidence_counter_path_count = accumulator["evidence_counter_path_count"]
        diagnostics[scope_name] = RepairPathDiagnostic(
            path_count=path_count,
            source_location_path_count=source_location_path_count,
            source_location_count=accumulator["source_location_count"],
            source_location_gap_count=max(
                path_count - source_location_path_count,
                0,
            ),
            first_source_location=accumulator["first_source_location"],
            evidence_counter_path_count=evidence_counter_path_count,
            evidence_counter_count=accumulator["evidence_counter_count"],
            evidence_counter_gap_count=max(
                path_count - evidence_counter_path_count,
                0,
            ),
        )
    return FinalReportRepairPathDiagnosticsProjection(diagnostics)


@dataclass(frozen=True)
class FinalReportRepairPathGroupProjection:
    fields: Mapping[str, Any]

    def to_release_pipeline_dict(self) -> Dict[str, Any]:
        return dict(self.fields)


def project_final_report_repair_path_groups(
    closure: Mapping[str, Any],
    release_summary: Mapping[str, Any],
    repair_path_groups: Sequence[Any],
    repair_path_group_plan: Sequence[Any],
    *,
    truthy_coercer: Callable[[Any], bool],
) -> FinalReportRepairPathGroupProjection:
    """Project repair-path group and plan counts with one fallback owner."""

    def group_count(*, grouping: str = "", action_field: str = "") -> int:
        count = 0
        for group in repair_path_groups:
            if not isinstance(group, dict):
                continue
            if grouping == "category":
                matches = group.get("grouping") in {"category", "category_family"}
            elif grouping:
                matches = group.get("grouping") == grouping
            else:
                matches = truthy_coercer(group.get(action_field))
            if matches:
                count += 1
        return count

    def plan_count(action_field: str) -> int:
        return sum(
            1
            for step in repair_path_group_plan
            if isinstance(step, dict) and truthy_coercer(step.get(action_field))
        )

    fields = {
        "repair_path_group_count": first_present(
            closure.get("repair_path_group_count"),
            release_summary.get("non_gpu_repair_path_group_count"),
            len(repair_path_groups),
        ),
        "repair_path_environment_group_count": first_present(
            release_summary.get("non_gpu_repair_path_environment_group_count"),
            group_count(grouping="environment"),
        ),
        "repair_path_category_group_count": first_present(
            release_summary.get("non_gpu_repair_path_category_group_count"),
            group_count(grouping="category"),
        ),
        "repair_path_current_repo_action_group_count": first_present(
            closure.get("repair_path_current_repo_action_group_count"),
            release_summary.get(
                "non_gpu_repair_path_current_repo_action_group_count"
            ),
            group_count(action_field="current_repo_action_required"),
        ),
        "repair_path_target_or_validation_action_group_count": first_present(
            closure.get("repair_path_target_or_validation_action_group_count"),
            release_summary.get(
                "non_gpu_repair_path_target_or_validation_action_group_count"
            ),
            group_count(action_field="target_or_validation_action_required"),
        ),
        "repair_path_group_plan_step_count": first_present(
            closure.get("repair_path_group_plan_step_count"),
            release_summary.get("non_gpu_repair_path_group_plan_step_count"),
            len(repair_path_group_plan),
        ),
        "repair_path_group_plan_current_repo_step_count": first_present(
            closure.get("repair_path_group_plan_current_repo_step_count"),
            release_summary.get(
                "non_gpu_repair_path_group_plan_current_repo_step_count"
            ),
            plan_count("current_repo_action_required"),
        ),
        "repair_path_group_plan_target_or_validation_step_count": first_present(
            closure.get(
                "repair_path_group_plan_target_or_validation_step_count"
            ),
            release_summary.get(
                "non_gpu_repair_path_group_plan_target_or_validation_step_count"
            ),
            plan_count("target_or_validation_action_required"),
        ),
        "first_repair_path_group_plan_step": (
            repair_path_group_plan[0].get("group_id")
            if repair_path_group_plan
            and isinstance(repair_path_group_plan[0], dict)
            else None
        ),
        "first_repair_path_group": (
            repair_path_groups[0].get("id")
            if repair_path_groups and isinstance(repair_path_groups[0], dict)
            else None
        ),
    }
    return FinalReportRepairPathGroupProjection(fields)

_ROOT_CAUSE_CATALOG_FLOW_GOVERNANCE_FIELD_SPECS = (
    ("catalog_flow_root_cause_count", "count", 0),
    ("catalog_flow_discrepancy_review_present", "bool", False),
    ("catalog_flow_discrepancy_count", "count", 0),
    ("catalog_flow_input_gap_count", "count", 0),
    ("catalog_flow_single_source_repair_card_count", "count", 0),
    (
        "catalog_flow_single_source_parity_refresh_required_count",
        "count",
        0,
    ),
    ("catalog_flow_surface_migration_ready_count", "count", 0),
    ("catalog_flow_surface_migration_fallback_count", "count", 0),
    ("catalog_flow_surface_parity_refresh_required_count", "count", 0),
    ("catalog_flow_surface_current_repo_action_count", "count", 0),
    ("catalog_flow_surface_target_machine_action_count", "count", 0),
    ("catalog_flow_surface_consumer_migration_plan_count", "count", 0),
    (
        "catalog_flow_surface_consumer_fallback_removal_blocked_count",
        "count",
        0,
    ),
    (
        "catalog_flow_surface_consumer_parity_refresh_required_count",
        "count",
        0,
    ),
    ("catalog_flow_surface_consumer_current_repo_action_count", "count", 0),
    ("catalog_flow_surface_consumer_target_machine_action_count", "count", 0),
    ("catalog_metadata_policy_gap_root_cause_count", "count", 0),
    ("catalog_flow_metadata_policy_card_count", "count", 0),
    ("catalog_flow_metadata_derived_policy_artifact_count", "count", 0),
    ("catalog_flow_metadata_derived_policy_field_count", "count", 0),
    ("catalog_flow_repair_card_count", "count", 0),
)

_CATALOG_FLOW_REMOVAL_HEAD_FIELD_SPECS = (
    ("source", "text", "not_reported"),
    ("direct_artifact_present", "bool", False),
    ("root_cause_count", "count", 0),
    ("present", "bool", False),
)
_CATALOG_FLOW_REMOVAL_TAIL_FIELD_SPECS = (
    ("first_surface", "possibly_empty_text", ""),
    ("blocks_removal", "bool", False),
    ("release_repair_path_count", "count", 0),
    ("release_repair_path_source_location_gap_count", "count", 0),
)
_CATALOG_FLOW_SHORTEST_PATH_FIELD_SPECS = (
    *_CATALOG_FLOW_REMOVAL_HEAD_FIELD_SPECS,
    ("state", "text", "not_reported"),
    ("step_count", "count", 0),
    ("current_repo_step_count", "count", 0),
    ("target_machine_step_count", "count", 0),
    ("source_location_gap_count", "count", 0),
    ("primary_environment", "text", "not_applicable"),
    *_CATALOG_FLOW_REMOVAL_TAIL_FIELD_SPECS,
    ("claim_boundary", "text", CATALOG_FLOW_SHORTEST_PATH_CLAIM_BOUNDARY),
)
_CATALOG_FLOW_PARITY_GATE_FIELD_SPECS = (
    *_CATALOG_FLOW_REMOVAL_HEAD_FIELD_SPECS,
    ("decision", "text", "not_reported"),
    ("removal_allowed", "bool", False),
    ("blocker_count", "count", 0),
    ("blocking_surface_count", "count", 0),
    ("fallback_surface_count", "count", 0),
    ("parity_refresh_required_count", "count", 0),
    ("current_repo_action_count", "count", 0),
    ("target_machine_action_count", "count", 0),
    ("source_location_gap_count", "count", 0),
    *_CATALOG_FLOW_REMOVAL_TAIL_FIELD_SPECS,
    ("claim_boundary", "text", CATALOG_FLOW_PARITY_GATE_CLAIM_BOUNDARY),
)

_CATALOG_FLOW_REMOVAL_NON_COMPATIBILITY_FIELDS = frozenset(
    {"source", "direct_artifact_present", "claim_boundary"}
)
_CATALOG_FLOW_FIELD_SUFFIX_OVERRIDES = {
    "release_repair_path_count": "repair_path_count",
    "release_repair_path_source_location_gap_count": (
        "repair_path_source_location_gap_count"
    ),
}
_CATALOG_FLOW_SHORTEST_PATH_COMPATIBILITY_FIELDS = tuple(
    field_name
    for field_name, _, _ in _CATALOG_FLOW_SHORTEST_PATH_FIELD_SPECS
    if field_name not in _CATALOG_FLOW_REMOVAL_NON_COMPATIBILITY_FIELDS
)
_CATALOG_FLOW_PARITY_GATE_COMPATIBILITY_FIELDS = tuple(
    field_name
    for field_name, _, _ in _CATALOG_FLOW_PARITY_GATE_FIELD_SPECS
    if field_name not in _CATALOG_FLOW_REMOVAL_NON_COMPATIBILITY_FIELDS
)
_CATALOG_FLOW_SHORTEST_PATH_ROOT_CAUSE_REVIEW_FIELDS = tuple(
    field_name for field_name in _CATALOG_FLOW_SHORTEST_PATH_COMPATIBILITY_FIELDS
    if field_name not in _CATALOG_FLOW_FIELD_SUFFIX_OVERRIDES
)
_CATALOG_FLOW_PARITY_GATE_ROOT_CAUSE_REVIEW_FIELDS = tuple(
    field_name for field_name in _CATALOG_FLOW_PARITY_GATE_COMPATIBILITY_FIELDS
    if field_name not in _CATALOG_FLOW_FIELD_SUFFIX_OVERRIDES
)


@dataclass(frozen=True)
class RootCauseNativeAuditCoverageProjection:
    source: Any
    direct_artifact_present: Any
    root_cause_count: Any
    present: Any
    state: Any
    open_file_count: Any
    high_priority_open_file_count: Any
    current_repo_action_count: Any
    target_machine_action_count: Any
    source_location_gap_count: Any
    primary_blocker: Any
    primary_environment: Any
    first_open_file: Any
    next_action: Any
    blocks_full_audit_closure: Any
    release_repair_path_count: Any
    release_repair_path_source_location_gap_count: Any
    claim_boundary: Any

    def to_closure_dict(self) -> Dict[str, Any]:
        return {
            "source": self.source,
            "direct_artifact_present": self.direct_artifact_present,
            "root_cause_count": self.root_cause_count,
            "present": self.present,
            "state": self.state,
            "open_file_count": self.open_file_count,
            "high_priority_open_file_count": (
                self.high_priority_open_file_count
            ),
            "current_repo_action_count": self.current_repo_action_count,
            "target_machine_action_count": self.target_machine_action_count,
            "source_location_gap_count": self.source_location_gap_count,
            "primary_blocker": self.primary_blocker,
            "primary_environment": self.primary_environment,
            "first_open_file": self.first_open_file,
            "next_action": self.next_action,
            "blocks_full_audit_closure": self.blocks_full_audit_closure,
            "release_repair_path_count": self.release_repair_path_count,
            "release_repair_path_source_location_gap_count": (
                self.release_repair_path_source_location_gap_count
            ),
            "claim_boundary": self.claim_boundary,
        }

    def to_final_report_scorecard_dict(self) -> Dict[str, Any]:
        return {
            "release_pipeline_root_cause_native_core_audit_review_coverage_gate_root_cause_count": self.root_cause_count,
            "release_pipeline_root_cause_native_core_audit_review_coverage_open_file_count": self.open_file_count,
            "release_pipeline_root_cause_native_core_audit_review_coverage_high_priority_open_file_count": self.high_priority_open_file_count,
            "release_pipeline_root_cause_native_core_audit_review_coverage_source_location_gap_count": self.source_location_gap_count,
            "release_pipeline_root_cause_native_core_audit_review_coverage_blocks_full_audit_closure": self.blocks_full_audit_closure,
            "release_pipeline_root_cause_native_core_audit_review_coverage_repair_path_count": self.release_repair_path_count,
            "release_pipeline_root_cause_native_core_audit_review_coverage_repair_path_source_location_gap_count": self.release_repair_path_source_location_gap_count,
        }

    def to_final_report_blocker_counts_dict(self) -> Dict[str, Any]:
        return self.to_final_report_scorecard_dict()


@dataclass(frozen=True)
class RootCauseNativeAuditManifestProjection:
    source: Any
    root_cause_count: Any
    invalid_override_count: Any
    unmatched_override_count: Any
    release_repair_path_count: Any
    claim_boundary: Any

    def to_closure_dict(self) -> Dict[str, Any]:
        return {
            "source": self.source,
            "root_cause_count": self.root_cause_count,
            "invalid_override_count": self.invalid_override_count,
            "unmatched_override_count": self.unmatched_override_count,
            "release_repair_path_count": self.release_repair_path_count,
            "claim_boundary": self.claim_boundary,
        }

    def to_final_report_scorecard_dict(self) -> Dict[str, Any]:
        return {
            "root_cause_native_core_audit_review_manifest_root_cause_count": self.root_cause_count,
            "root_cause_native_core_audit_review_manifest_invalid_override_count": self.invalid_override_count,
            "root_cause_native_core_audit_review_manifest_unmatched_override_count": self.unmatched_override_count,
            "root_cause_native_core_audit_review_manifest_repair_path_count": self.release_repair_path_count,
            "release_pipeline_root_cause_native_core_audit_review_manifest_root_cause_count": self.root_cause_count,
            "release_pipeline_root_cause_native_core_audit_review_manifest_invalid_override_count": self.invalid_override_count,
            "release_pipeline_root_cause_native_core_audit_review_manifest_unmatched_override_count": self.unmatched_override_count,
            "release_pipeline_root_cause_native_core_audit_review_manifest_repair_path_count": self.release_repair_path_count,
        }

    def to_final_report_blocker_counts_dict(self) -> Dict[str, Any]:
        scorecard = self.to_final_report_scorecard_dict()
        return {
            key: value
            for key, value in scorecard.items()
            if key.startswith("release_pipeline_")
        }


@dataclass(frozen=True)
class _CatalogFlowProjectionIntegrity:
    canonical_source: str
    canonical_gap_count: int
    source_drift_count: int
    source_drift_fields: Tuple[str, ...]
    source_drift_sources: Tuple[str, ...]
    canonical_ready: bool
    source_consistent: bool

    def to_integrity_dict(self) -> Dict[str, Any]:
        return {
            "canonical_source": self.canonical_source,
            "canonical_gap_count": self.canonical_gap_count,
            "source_drift_count": self.source_drift_count,
            "source_drift_fields": list(self.source_drift_fields),
            "source_drift_sources": list(self.source_drift_sources),
            "canonical_ready": self.canonical_ready,
            "source_consistent": self.source_consistent,
        }


@dataclass(frozen=True)
class _RootCauseCatalogFlowRemovalProjection(
    _CatalogFlowProjectionIntegrity
):
    source: Any
    direct_artifact_present: bool
    root_cause_count: int
    present: bool
    source_location_gap_count: int
    first_surface: str
    blocks_removal: bool
    release_repair_path_count: int
    release_repair_path_source_location_gap_count: int
    claim_boundary: str


@dataclass(frozen=True)
class RootCauseCatalogFlowShortestPathProjection(
    _RootCauseCatalogFlowRemovalProjection
):
    state: str
    step_count: int
    current_repo_step_count: int
    target_machine_step_count: int
    primary_environment: str

    def to_closure_dict(self) -> Dict[str, Any]:
        return {
            "source": self.source,
            "direct_artifact_present": self.direct_artifact_present,
            "root_cause_count": self.root_cause_count,
            "present": self.present,
            "state": self.state,
            "step_count": self.step_count,
            "current_repo_step_count": self.current_repo_step_count,
            "target_machine_step_count": self.target_machine_step_count,
            "source_location_gap_count": self.source_location_gap_count,
            "primary_environment": self.primary_environment,
            "first_surface": self.first_surface,
            "blocks_removal": self.blocks_removal,
            "release_repair_path_count": self.release_repair_path_count,
            "release_repair_path_source_location_gap_count": (
                self.release_repair_path_source_location_gap_count
            ),
            "claim_boundary": self.claim_boundary,
            **self.to_integrity_dict(),
        }

    def to_release_pipeline_dict(self) -> Dict[str, Any]:
        return _catalog_flow_prefixed_fields(
            self.to_closure_dict(),
            "root_cause_catalog_flow_fallback_removal_shortest_path_",
            _CATALOG_FLOW_SHORTEST_PATH_COMPATIBILITY_FIELDS,
        )

    def to_root_cause_review_dict(self) -> Dict[str, Any]:
        return _catalog_flow_prefixed_fields(
            self.to_closure_dict(),
            "catalog_flow_fallback_removal_shortest_path_",
            _CATALOG_FLOW_SHORTEST_PATH_ROOT_CAUSE_REVIEW_FIELDS,
        )


@dataclass(frozen=True)
class RootCauseCatalogFlowParityGateProjection(
    _RootCauseCatalogFlowRemovalProjection
):
    decision: str
    removal_allowed: bool
    blocker_count: int
    blocking_surface_count: int
    fallback_surface_count: int
    parity_refresh_required_count: int
    current_repo_action_count: int
    target_machine_action_count: int

    def to_closure_dict(self) -> Dict[str, Any]:
        return {
            "source": self.source,
            "direct_artifact_present": self.direct_artifact_present,
            "root_cause_count": self.root_cause_count,
            "present": self.present,
            "decision": self.decision,
            "removal_allowed": self.removal_allowed,
            "blocker_count": self.blocker_count,
            "blocking_surface_count": self.blocking_surface_count,
            "fallback_surface_count": self.fallback_surface_count,
            "parity_refresh_required_count": (
                self.parity_refresh_required_count
            ),
            "current_repo_action_count": self.current_repo_action_count,
            "target_machine_action_count": self.target_machine_action_count,
            "source_location_gap_count": self.source_location_gap_count,
            "first_surface": self.first_surface,
            "blocks_removal": self.blocks_removal,
            "release_repair_path_count": self.release_repair_path_count,
            "release_repair_path_source_location_gap_count": (
                self.release_repair_path_source_location_gap_count
            ),
            "claim_boundary": self.claim_boundary,
            **self.to_integrity_dict(),
        }

    def to_release_pipeline_dict(self) -> Dict[str, Any]:
        return _catalog_flow_prefixed_fields(
            self.to_closure_dict(),
            "root_cause_catalog_flow_fallback_removal_parity_gate_",
            _CATALOG_FLOW_PARITY_GATE_COMPATIBILITY_FIELDS,
        )

    def to_root_cause_review_dict(self) -> Dict[str, Any]:
        projection = _catalog_flow_prefixed_fields(
            self.to_closure_dict(),
            "catalog_flow_fallback_removal_parity_gate_",
            _CATALOG_FLOW_PARITY_GATE_ROOT_CAUSE_REVIEW_FIELDS,
        )
        projection[
            "catalog_flow_fallback_removal_parity_gate_"
            "blocking_surface_source_location_gap_count"
        ] = self.source_location_gap_count
        return projection


@dataclass(frozen=True)
class RootCauseCatalogFlowGovernanceProjection(
    _CatalogFlowProjectionIntegrity
):
    values: Mapping[str, Any]
    claim_boundary: str

    def to_closure_dict(self) -> Dict[str, Any]:
        return {
            **self.values,
            **self.to_integrity_dict(),
            "claim_boundary": self.claim_boundary,
        }

    def to_root_cause_review_dict(self) -> Dict[str, Any]:
        return dict(self.values)


def _catalog_flow_prefixed_fields(
    values: Mapping[str, Any],
    prefix: str,
    field_names: Tuple[str, ...],
) -> Dict[str, Any]:
    return {
        (
            f"{prefix}"
            f"{_CATALOG_FLOW_FIELD_SUFFIX_OVERRIDES.get(field_name, field_name)}"
        ): values.get(field_name)
        for field_name in field_names
    }


def _normalize_catalog_flow_projection_value(
    value: Any, value_kind: str, default: Any
) -> Tuple[Any, bool]:
    if value_kind == "count":
        valid = (
            isinstance(value, int)
            and not isinstance(value, bool)
            and value >= 0
        )
    elif value_kind == "bool":
        valid = isinstance(value, bool)
    elif value_kind == "possibly_empty_text":
        valid = isinstance(value, str)
    else:
        valid = isinstance(value, str) and bool(value)
    return (value if valid else default), valid


def _project_catalog_flow_canonical_fields(
    canonical: Mapping[str, Any],
    *,
    canonical_source: str,
    field_specs: Tuple[Tuple[str, str, Any], ...],
    mirrors: Tuple[
        Tuple[str, Mapping[str, Any], Mapping[str, str]], ...
    ],
) -> Dict[str, Any]:
    values: Dict[str, Any] = {}
    canonical_validity: Dict[str, bool] = {}
    canonical_gap_count = 0
    for field_name, value_kind, default in field_specs:
        value, valid = _normalize_catalog_flow_projection_value(
            canonical.get(field_name), value_kind, default
        )
        values[field_name] = value
        canonical_validity[field_name] = valid
        if not valid:
            canonical_gap_count += 1

    source_drift_count = 0
    source_drift_fields = set()
    source_drift_sources = set()
    spec_by_field = {
        field_name: (value_kind, default)
        for field_name, value_kind, default in field_specs
    }
    for source_name, mirror, key_map in mirrors:
        for field_name, mirror_key in key_map.items():
            if mirror_key not in mirror:
                continue
            value_kind, default = spec_by_field[field_name]
            mirror_value, mirror_valid = (
                _normalize_catalog_flow_projection_value(
                    mirror.get(mirror_key), value_kind, default
                )
            )
            if (
                not mirror_valid
                or not canonical_validity[field_name]
                or mirror_value != values[field_name]
            ):
                source_drift_count += 1
                source_drift_fields.add(field_name)
                source_drift_sources.add(source_name)

    values.update(
        {
            "canonical_source": canonical_source,
            "canonical_gap_count": canonical_gap_count,
            "source_drift_count": source_drift_count,
            "source_drift_fields": tuple(sorted(source_drift_fields)),
            "source_drift_sources": tuple(sorted(source_drift_sources)),
            "canonical_ready": canonical_gap_count == 0,
            "source_consistent": source_drift_count == 0,
        }
    )
    return values


def _project_root_cause_catalog_flow_review(
    closure_review: Mapping[str, Any],
    release_summary: Mapping[str, Any],
    direct_root_cause_summary: Mapping[str, Any],
    *,
    closure_name: str,
    summary_prefix: str,
    mirror_prefix: str,
    field_specs: Tuple[Tuple[str, str, Any], ...],
) -> Dict[str, Any]:
    canonical = dict_value(closure_review)

    summary_keys = {
        field_name: (
            f"{summary_prefix}"
            f"{_CATALOG_FLOW_FIELD_SUFFIX_OVERRIDES.get(field_name, field_name)}"
        )
        for field_name, _, _ in field_specs
        if field_name not in _CATALOG_FLOW_REMOVAL_NON_COMPATIBILITY_FIELDS
    }
    mirror_keys = {
        field_name: f"{mirror_prefix}{field_name}"
        for field_name, _, _ in field_specs
        if field_name not in _CATALOG_FLOW_REMOVAL_NON_COMPATIBILITY_FIELDS
        and field_name not in _CATALOG_FLOW_FIELD_SUFFIX_OVERRIDES
    }
    mirrors = (
        (
            "native_release_evidence_pipeline.json.summary",
            dict_value(release_summary),
            summary_keys,
        ),
        (
            "native_evidence_query_result.json.summary",
            dict_value(direct_root_cause_summary),
            mirror_keys,
        ),
    )
    return _project_catalog_flow_canonical_fields(
        canonical,
        canonical_source=(
            "native_release_evidence_pipeline.json.non_gpu_review_closure."
            f"{closure_name}"
        ),
        field_specs=field_specs,
        mirrors=mirrors,
    )


def project_root_cause_catalog_flow_shortest_path(
    closure_review: Mapping[str, Any],
    release_summary: Mapping[str, Any],
    direct_root_cause_summary: Mapping[str, Any],
) -> RootCauseCatalogFlowShortestPathProjection:
    """Use the Pipeline nested closure and report mirror drift without voting."""
    return RootCauseCatalogFlowShortestPathProjection(
        **_project_root_cause_catalog_flow_review(
            closure_review,
            release_summary,
            direct_root_cause_summary,
            closure_name=(
                "root_cause_catalog_flow_fallback_removal_shortest_path"
            ),
            summary_prefix=(
                "root_cause_catalog_flow_fallback_removal_shortest_path_"
            ),
            mirror_prefix=(
                "catalog_flow_fallback_removal_shortest_path_"
            ),
            field_specs=_CATALOG_FLOW_SHORTEST_PATH_FIELD_SPECS,
        )
    )


def project_root_cause_catalog_flow_parity_gate(
    closure_review: Mapping[str, Any],
    release_summary: Mapping[str, Any],
    direct_root_cause_summary: Mapping[str, Any],
) -> RootCauseCatalogFlowParityGateProjection:
    """Use the Pipeline nested parity gate and report mirror drift."""
    return RootCauseCatalogFlowParityGateProjection(
        **_project_root_cause_catalog_flow_review(
            closure_review,
            release_summary,
            direct_root_cause_summary,
            closure_name=(
                "root_cause_catalog_flow_fallback_removal_parity_gate"
            ),
            summary_prefix=(
                "root_cause_catalog_flow_fallback_removal_parity_gate_"
            ),
            mirror_prefix=(
                "catalog_flow_fallback_removal_parity_gate_"
            ),
            field_specs=_CATALOG_FLOW_PARITY_GATE_FIELD_SPECS,
        )
    )


def project_root_cause_catalog_flow_governance(
    direct_root_cause_summary: Mapping[str, Any],
) -> RootCauseCatalogFlowGovernanceProjection:
    """Project canonical Evidence Query catalog-flow governance fields."""
    field_names = tuple(
        field_name
        for field_name, _, _ in _ROOT_CAUSE_CATALOG_FLOW_GOVERNANCE_FIELD_SPECS
    )
    projected = _project_catalog_flow_canonical_fields(
        dict_value(direct_root_cause_summary),
        canonical_source="native_evidence_query_result.json.summary",
        field_specs=_ROOT_CAUSE_CATALOG_FLOW_GOVERNANCE_FIELD_SPECS,
        mirrors=(),
    )
    return RootCauseCatalogFlowGovernanceProjection(
        values={field_name: projected[field_name] for field_name in field_names},
        canonical_source=projected["canonical_source"],
        canonical_gap_count=projected["canonical_gap_count"],
        source_drift_count=projected["source_drift_count"],
        source_drift_fields=projected["source_drift_fields"],
        source_drift_sources=projected["source_drift_sources"],
        canonical_ready=projected["canonical_ready"],
        source_consistent=projected["source_consistent"],
        claim_boundary=CATALOG_FLOW_GOVERNANCE_CLAIM_BOUNDARY,
    )


def project_root_cause_native_audit_coverage(
    release_summary: Mapping[str, Any],
    closure_review: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], Any],
    truthy_coercer: Callable[[Any], Any],
    preserve_missing: bool = False,
) -> RootCauseNativeAuditCoverageProjection:
    """Resolve Native Audit coverage from Pipeline summary and closure."""
    summary = dict_value(release_summary)
    review = dict_value(closure_review)

    default = partial(
        missing_preserving_default,
        preserve_missing=preserve_missing,
    )

    def value(
        summary_key: str,
        review_key: str,
        fallback: Any = None,
    ) -> Any:
        return first_present(
            summary.get(summary_key),
            review.get(review_key),
            fallback,
        )

    def count(summary_key: str, review_key: str) -> Any:
        return count_coercer(value(summary_key, review_key))

    def truthy(summary_key: str, review_key: str) -> Any:
        return truthy_coercer(value(summary_key, review_key))

    summary_present = any(
        summary.get(key) is not None
        for key in (
            "root_cause_native_core_audit_review_coverage_gate_root_cause_count",
            "root_cause_native_core_audit_review_coverage_gate_present",
            "root_cause_native_core_audit_review_coverage_state",
        )
    )
    return RootCauseNativeAuditCoverageProjection(
        source=first_present(
            review.get("source"),
            (
                "native_release_evidence_pipeline.json.summary"
                if summary_present
                else None
            ),
            default("not_reported"),
        ),
        direct_artifact_present=truthy_coercer(
            first_present(
                review.get("direct_artifact_present"), default(False)
            )
        ),
        root_cause_count=count(
            "root_cause_native_core_audit_review_coverage_gate_root_cause_count",
            "root_cause_count",
        ),
        present=truthy(
            "root_cause_native_core_audit_review_coverage_gate_present",
            "present",
        ),
        state=value(
            "root_cause_native_core_audit_review_coverage_state",
            "state",
            fallback=default("unknown"),
        ),
        open_file_count=count(
            "root_cause_native_core_audit_review_coverage_open_file_count",
            "open_file_count",
        ),
        high_priority_open_file_count=count(
            "root_cause_native_core_audit_review_coverage_high_priority_open_file_count",
            "high_priority_open_file_count",
        ),
        current_repo_action_count=count(
            "root_cause_native_core_audit_review_coverage_current_repo_action_count",
            "current_repo_action_count",
        ),
        target_machine_action_count=count(
            "root_cause_native_core_audit_review_coverage_target_machine_action_count",
            "target_machine_action_count",
        ),
        source_location_gap_count=count(
            "root_cause_native_core_audit_review_coverage_source_location_gap_count",
            "source_location_gap_count",
        ),
        primary_blocker=value(
            "root_cause_native_core_audit_review_coverage_primary_blocker",
            "primary_blocker",
            fallback=default("unknown"),
        ),
        primary_environment=value(
            "root_cause_native_core_audit_review_coverage_primary_environment",
            "primary_environment",
            fallback=default("unknown"),
        ),
        first_open_file=value(
            "root_cause_native_core_audit_review_coverage_first_open_file",
            "first_open_file",
            fallback=default("unknown"),
        ),
        next_action=value(
            "root_cause_native_core_audit_review_coverage_next_action",
            "next_action",
            fallback=default("unknown"),
        ),
        blocks_full_audit_closure=truthy(
            "root_cause_native_core_audit_review_coverage_blocks_full_audit_closure",
            "blocks_full_audit_closure",
        ),
        release_repair_path_count=count(
            "root_cause_native_core_audit_review_coverage_repair_path_count",
            "release_repair_path_count",
        ),
        release_repair_path_source_location_gap_count=count(
            "root_cause_native_core_audit_review_coverage_repair_path_source_location_gap_count",
            "release_repair_path_source_location_gap_count",
        ),
        claim_boundary=first_present(
            review.get("claim_boundary"), NATIVE_AUDIT_COVERAGE_CLAIM_BOUNDARY
        ),
    )


def project_root_cause_native_audit_manifest(
    release_summary: Mapping[str, Any],
    closure_review: Mapping[str, Any],
    *,
    count_coercer: Callable[[Any], Any],
    preserve_missing: bool = False,
) -> RootCauseNativeAuditManifestProjection:
    """Resolve Native Audit manifest fields from Pipeline summary and closure."""
    summary = dict_value(release_summary)
    review = dict_value(closure_review)

    default = partial(
        missing_preserving_default,
        preserve_missing=preserve_missing,
    )

    def count(summary_key: str, review_key: str) -> Any:
        return count_coercer(
            first_present(
                summary.get(summary_key),
                review.get(review_key),
            )
        )

    summary_present = any(
        summary.get(key) is not None
        for key in (
            "root_cause_native_core_audit_review_manifest_root_cause_count",
            "root_cause_native_core_audit_review_manifest_invalid_override_count",
            "root_cause_native_core_audit_review_manifest_unmatched_override_count",
        )
    )
    return RootCauseNativeAuditManifestProjection(
        source=first_present(
            review.get("source"),
            (
                "native_release_evidence_pipeline.json.summary"
                if summary_present
                else None
            ),
            default("not_reported"),
        ),
        root_cause_count=count(
            "root_cause_native_core_audit_review_manifest_root_cause_count",
            "root_cause_count",
        ),
        invalid_override_count=count(
            "root_cause_native_core_audit_review_manifest_invalid_override_count",
            "invalid_override_count",
        ),
        unmatched_override_count=count(
            "root_cause_native_core_audit_review_manifest_unmatched_override_count",
            "unmatched_override_count",
        ),
        release_repair_path_count=count(
            "root_cause_native_core_audit_review_manifest_repair_path_count",
            "release_repair_path_count",
        ),
        claim_boundary=first_present(
            review.get("claim_boundary"),
            NATIVE_AUDIT_MANIFEST_CLAIM_BOUNDARY,
        ),
    )
