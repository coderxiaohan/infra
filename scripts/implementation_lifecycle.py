#!/usr/bin/env python3
"""Canonical lifecycle registry and retirement projection for Python glue code."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Optional, Tuple

if __package__:
    from .compatibility_repository_caller_audit import (
        CompatibilityRepositorySourceSnapshot,
        audit_python_compatibility_callers,
        capture_compatibility_repository_source_snapshot,
    )
    from .compatibility_evidence_primitives import (
        CompatibilityRetirementEvidenceError,
        require_compatibility_evidence_token,
    )
    from .compatibility_external_caller_evidence import (
        CompatibilityExternalCallerEvidence,
    )
    from .compatibility_parity_evidence import (
        CompatibilityParityEvidence,
        load_verified_compatibility_parity_evidence,
    )
    from .compatibility_removal_authorization import (
        CompatibilityRemovalAuthorization,
        load_compatibility_removal_authorization_document,
    )
    from .compatibility_removal_authorization_binding import (
        bind_compatibility_removal_authorization,
        compatibility_removal_authorization_binding_payload,
    )
    from .compatibility_retirement_candidate_evidence import (
        bind_compatibility_retirement_candidate_evidence,
        compatibility_retirement_candidate_evidence_payloads,
    )
    from .compatibility_retirement_evidence import (
        CompatibilityRetirementEvidence,
        compatibility_retirement_evidence_payload,
        load_compatibility_retirement_evidence_document,
        missing_compatibility_retirement_evidence_payload,
    )
else:
    from compatibility_repository_caller_audit import (  # type: ignore
        CompatibilityRepositorySourceSnapshot,
        audit_python_compatibility_callers,
        capture_compatibility_repository_source_snapshot,
    )
    from compatibility_evidence_primitives import (  # type: ignore
        CompatibilityRetirementEvidenceError,
        require_compatibility_evidence_token,
    )
    from compatibility_external_caller_evidence import (  # type: ignore
        CompatibilityExternalCallerEvidence,
    )
    from compatibility_parity_evidence import (  # type: ignore
        CompatibilityParityEvidence,
        load_verified_compatibility_parity_evidence,
    )
    from compatibility_removal_authorization import (  # type: ignore
        CompatibilityRemovalAuthorization,
        load_compatibility_removal_authorization_document,
    )
    from compatibility_removal_authorization_binding import (  # type: ignore
        bind_compatibility_removal_authorization,
        compatibility_removal_authorization_binding_payload,
    )
    from compatibility_retirement_candidate_evidence import (  # type: ignore
        bind_compatibility_retirement_candidate_evidence,
        compatibility_retirement_candidate_evidence_payloads,
    )
    from compatibility_retirement_evidence import (  # type: ignore
        CompatibilityRetirementEvidence,
        compatibility_retirement_evidence_payload,
        load_compatibility_retirement_evidence_document,
        missing_compatibility_retirement_evidence_payload,
    )

_LIFECYCLE_VALIDATION_TOKEN = object()


@dataclass(frozen=True)
class ImplementationLifecycle:
    entry_id: str
    path: str
    role: str
    status: str
    canonical_owner: str
    replacement: Optional[str]
    delegate_override_option: Optional[str]
    enterprise_counterpart: Optional[str]
    known_callers: Tuple[str, ...]
    repository_runtime_callers: Tuple[str, ...]
    documentation_references: Tuple[str, ...]
    caller_audit_status: str
    external_caller_audit_status: str
    behavior_parity_status: str
    removal_condition: str
    removal_blocker: str
    removal_allowed: bool
    target_release: str
    claim_boundary: str
    display_path: Optional[str] = None
    display_replacement: Optional[str] = None
    repository_caller_audit_method: str = "canonical_lifecycle_registry"
    repository_caller_audit_status: str = "registry_declared"
    repository_caller_audit_scope: str = "canonical_lifecycle_registry_entries"
    repository_caller_audit_file_count: int = 0
    repository_caller_audit_byte_count: int = 0
    repository_non_runtime_references: Tuple[str, ...] = ()
    _validation_token: object = field(
        default=None, repr=False, compare=False
    )


_COMPAT_REMOVAL = (
    "canonical behavior/artifact parity plus repository and external caller migration"
)
_COMPAT_REMOVAL_BLOCKER = (
    "canonical behavior/artifact parity is not validated and the external caller "
    "inventory is incomplete"
)
COMPATIBILITY_RETIREMENT_RELEASE_EPOCH = "inferlab_compatibility_cleanup_v1"
_REFERENCE_REMOVAL = (
    "retain until reference, teaching, fixture, and documented callers migrate; "
    "do not delete by filename similarity alone"
)
_REFERENCE_BOUNDARY = (
    "reference or teaching implementation only; not the enterprise native core and "
    "not production performance evidence"
)


def _compatibility_lifecycle_descriptor(
    entry_id: str,
    path: str,
    canonical_owner: str,
    replacement: str,
    documentation_references: Tuple[str, ...] = (),
    *,
    repository_runtime_callers: Tuple[str, ...] = (),
    delegate_override_option: Optional[str] = None,
    display_path: Optional[str] = None,
    display_replacement: Optional[str] = None,
    target_release: str = COMPATIBILITY_RETIREMENT_RELEASE_EPOCH,
    repository_caller_audit_method: str = "canonical_lifecycle_registry",
    repository_caller_audit_status: str = "registry_declared",
    repository_caller_audit_scope: str = "canonical_lifecycle_registry_entries",
    repository_caller_audit_file_count: int = 0,
    repository_caller_audit_byte_count: int = 0,
    repository_non_runtime_references: Tuple[str, ...] = (),
) -> ImplementationLifecycle:
    known_references = repository_runtime_callers + documentation_references
    return ImplementationLifecycle(
        entry_id=entry_id,
        path=path,
        role="compatibility_wrapper",
        status="active_compatibility",
        canonical_owner=canonical_owner,
        replacement=replacement,
        delegate_override_option=delegate_override_option,
        enterprise_counterpart=canonical_owner,
        known_callers=known_references,
        repository_runtime_callers=repository_runtime_callers,
        documentation_references=documentation_references,
        caller_audit_status=(
            "repository_runtime_callers_recorded_external_audit_pending"
            if repository_runtime_callers
            else "repository_runtime_callers_clear_external_audit_pending"
        ),
        external_caller_audit_status="pending_external_inventory",
        behavior_parity_status="not_run_per_user_request",
        removal_condition=_COMPAT_REMOVAL,
        removal_blocker=_COMPAT_REMOVAL_BLOCKER,
        removal_allowed=False,
        target_release=target_release,
        claim_boundary="delegation only; creates no independent benchmark or native-core claim",
        display_path=display_path,
        display_replacement=display_replacement,
        repository_caller_audit_method=repository_caller_audit_method,
        repository_caller_audit_status=repository_caller_audit_status,
        repository_caller_audit_scope=repository_caller_audit_scope,
        repository_caller_audit_file_count=repository_caller_audit_file_count,
        repository_caller_audit_byte_count=repository_caller_audit_byte_count,
        repository_non_runtime_references=repository_non_runtime_references,
        _validation_token=_LIFECYCLE_VALIDATION_TOKEN,
    )


def _reference(
    entry_id: str,
    path: str,
    enterprise_counterpart: str,
    repository_runtime_callers: Tuple[str, ...] = (),
    documentation_references: Tuple[str, ...] = (),
    role: str = "reference_simulator",
) -> ImplementationLifecycle:
    known_references = repository_runtime_callers + documentation_references
    return ImplementationLifecycle(
        entry_id=entry_id,
        path=path,
        role=role,
        status="reference",
        canonical_owner=path,
        replacement=None,
        delegate_override_option=None,
        enterprise_counterpart=enterprise_counterpart,
        known_callers=known_references,
        repository_runtime_callers=repository_runtime_callers,
        documentation_references=documentation_references,
        caller_audit_status=(
            "repository_runtime_callers_recorded"
            if repository_runtime_callers
            else "repository_runtime_callers_clear"
        ),
        external_caller_audit_status="not_applicable_reference",
        behavior_parity_status="not_applicable_reference",
        removal_condition=_REFERENCE_REMOVAL,
        removal_blocker="retained reference; no compatibility retirement is scheduled",
        removal_allowed=False,
        target_release="retained_reference",
        claim_boundary=_REFERENCE_BOUNDARY,
        _validation_token=_LIFECYCLE_VALIDATION_TOKEN,
    )


_ENTRIES = (
    _compatibility_lifecycle_descriptor(
        "benchmarks.vllm_online",
        "benchmarks/bench_vllm_online.py",
        "scripts/run_vllm_benchmark.py",
        "scripts/run_vllm_benchmark.py",
        (
            "profiling/vllm/README.md",
            "reports/04_vllm_serving_benchmark.md",
            "../大模型推理优化综合项目方案.md",
        ),
    ),
    _compatibility_lifecycle_descriptor(
        "benchmarks.vllm_offline",
        "benchmarks/bench_vllm_offline.py",
        "scripts/run_vllm_benchmark.py",
        "scripts/run_vllm_benchmark.py",
        (
            "profiling/vllm/README.md",
            "reports/04_vllm_serving_benchmark.md",
            "../大模型推理优化综合项目方案.md",
        ),
    ),
    _compatibility_lifecycle_descriptor(
        "benchmarks.llama_cpp",
        "benchmarks/bench_llama_cpp.py",
        "scripts/run_llama_cpp_benchmark.py",
        "scripts/run_llama_cpp_benchmark.py",
        (
            "reports/07_llama_cpp_edge_benchmark.md",
            "../大模型推理优化综合项目方案.md",
        ),
    ),
    _compatibility_lifecycle_descriptor(
        "benchmarks.trtllm",
        "benchmarks/bench_trtllm.py",
        "scripts/run_trtllm_benchmark.py",
        "scripts/run_trtllm_benchmark.py",
        ("../大模型推理优化综合项目方案.md",),
    ),
    _compatibility_lifecycle_descriptor(
        "advanced_serving.engine_memory_planner",
        "advanced_serving/engine_memory_planner.py",
        "inferlab_engine_memory_planner",
        "./build/inferlab_engine_memory_planner",
        ("../竞品功能优化建议.md",),
        delegate_override_option="--binary",
    ),
    _compatibility_lifecycle_descriptor(
        "advanced_serving.kv_offload_hierarchy",
        "advanced_serving/kv_offload_hierarchy.py",
        "inferlab_kv_offload_hierarchy_plan",
        "./build/inferlab_kv_offload_hierarchy_plan",
        ("../竞品功能优化建议.md",),
        delegate_override_option="--binary",
    ),
    _compatibility_lifecycle_descriptor(
        "advanced_serving.streaming_sse_benchmark",
        "advanced_serving/streaming_sse_benchmark.py",
        "inferlab_streaming_sse_plan",
        "./build/inferlab_streaming_sse_plan",
        ("../竞品功能优化建议.md",),
        delegate_override_option="--binary",
    ),
    _reference(
        "reference.mini_llm.kv_cache",
        "mini_llm/kv_cache.py",
        "native_core/include/inferlab/kv_cache.hpp",
        (
            "tests/test_kv_cache.py",
        ),
        documentation_references=("scripts/render_final_report.py",),
        role="reference_implementation",
    ),
    _reference(
        "reference.mini_llm.speculative_decode",
        "examples/reference/speculative_decode.py",
        "native_core/include/inferlab/advanced_serving_generation_strategy.hpp",
        documentation_references=(
            "README.md",
            "reports/10_speculative_decoding.md",
        ),
        role="reference_implementation",
    ),
)

_LIFECYCLE_BY_ID: Dict[str, ImplementationLifecycle] = {
    entry.entry_id: entry for entry in _ENTRIES
}

if len(_LIFECYCLE_BY_ID) != len(_ENTRIES):
    raise ValueError("duplicate implementation lifecycle entry id")
if len({entry.path for entry in _ENTRIES}) != len(_ENTRIES):
    raise ValueError("duplicate implementation lifecycle path")
for entry in _ENTRIES:
    require_compatibility_evidence_token(
        "implementation lifecycle target_release", entry.target_release
    )
    for label, values in (
        ("known caller", entry.known_callers),
        ("repository runtime caller", entry.repository_runtime_callers),
        ("documentation reference", entry.documentation_references),
    ):
        if any(not value.strip() for value in values):
            raise ValueError(f"blank {label} in lifecycle entry: {entry.entry_id}")
        if len(set(values)) != len(values):
            raise ValueError(f"duplicate {label} in lifecycle entry: {entry.entry_id}")
    if set(entry.repository_runtime_callers) & set(entry.documentation_references):
        raise ValueError(
            f"runtime caller also classified as documentation: {entry.entry_id}"
        )
    expected_references = (
        entry.repository_runtime_callers + entry.documentation_references
    )
    if entry.known_callers != expected_references:
        raise ValueError(f"caller inventory drift: {entry.entry_id}")
    if entry.role == "compatibility_wrapper":
        if entry.target_release != COMPATIBILITY_RETIREMENT_RELEASE_EPOCH:
            raise ValueError(
                f"compatibility release epoch drift: {entry.entry_id}"
            )
        if entry.removal_allowed:
            raise ValueError(
                f"compatibility removal cannot be allowed without imported parity "
                f"and external caller evidence: {entry.entry_id}"
            )
        if entry.delegate_override_option is not None and (
            not entry.delegate_override_option.startswith("--")
            or "=" in entry.delegate_override_option
            or len(entry.delegate_override_option) > 64
        ):
            raise ValueError(
                f"invalid compatibility delegate override option: {entry.entry_id}"
            )
    elif entry.delegate_override_option is not None:
        raise ValueError(
            f"non-compatibility entry has a delegate override: {entry.entry_id}"
        )


def implementation_lifecycles() -> Tuple[ImplementationLifecycle, ...]:
    return _ENTRIES


def lifecycle_for(entry_id: str) -> ImplementationLifecycle:
    try:
        return _LIFECYCLE_BY_ID[entry_id]
    except KeyError as exc:
        raise KeyError(f"unknown implementation lifecycle entry: {entry_id}") from exc


def governed_lifecycle_for(
    entry_id: str,
    repo_root: str | Path | None = None,
    *,
    source_snapshot: Optional[CompatibilityRepositorySourceSnapshot] = None,
) -> ImplementationLifecycle:
    entry = lifecycle_for(entry_id)
    if entry.role != "compatibility_wrapper":
        return entry
    root = (
        Path(repo_root)
        if repo_root is not None
        else Path(__file__).resolve().parents[1]
    )
    audit = audit_python_compatibility_callers(
        root,
        entry_id=entry.entry_id,
        entrypoint_path=entry.path,
        source_snapshot=source_snapshot,
    )
    scanned_callers = tuple(
        reference.location for reference in audit.runtime_callers
    )
    repository_runtime_callers = tuple(
        dict.fromkeys(entry.repository_runtime_callers + scanned_callers)
    )
    audit_status = (
        "runtime_callers_detected"
        if repository_runtime_callers
        else audit.status
    )
    return _compatibility_lifecycle_descriptor(
        entry_id=entry.entry_id,
        path=entry.path,
        canonical_owner=entry.canonical_owner,
        replacement=entry.replacement or "",
        documentation_references=entry.documentation_references,
        repository_runtime_callers=repository_runtime_callers,
        delegate_override_option=entry.delegate_override_option,
        display_path=entry.display_path,
        display_replacement=entry.display_replacement,
        target_release=entry.target_release,
        repository_caller_audit_method=audit.method,
        repository_caller_audit_status=audit_status,
        repository_caller_audit_scope=(
            "repository_source_files_excluding_generated_virtualenv_results_"
            "reports_and_markdown"
        ),
        repository_caller_audit_file_count=audit.scanned_file_count,
        repository_caller_audit_byte_count=audit.scanned_byte_count,
        repository_non_runtime_references=tuple(
            f"{reference.location} [{reference.classification}]"
            for reference in audit.non_runtime_references
        ),
    )


def governed_implementation_lifecycles(
    repo_root: str | Path | None = None,
) -> Tuple[ImplementationLifecycle, ...]:
    """Project every lifecycle while reading repository source only once."""

    root = (
        Path(repo_root)
        if repo_root is not None
        else Path(__file__).resolve().parents[1]
    )
    source_snapshot = capture_compatibility_repository_source_snapshot(root)
    return tuple(
        governed_lifecycle_for(
            entry.entry_id,
            root,
            source_snapshot=source_snapshot,
        )
        for entry in _ENTRIES
    )


def _require_compatibility_lifecycle(
    entry: ImplementationLifecycle,
) -> ImplementationLifecycle:
    if not isinstance(entry, ImplementationLifecycle):
        raise CompatibilityRetirementEvidenceError(
            "compatibility lifecycle descriptor is not canonical"
        )
    if entry._validation_token is not _LIFECYCLE_VALIDATION_TOKEN:
        raise CompatibilityRetirementEvidenceError(
            "compatibility lifecycle must originate from the canonical factory"
        )
    if entry.role != "compatibility_wrapper":
        raise CompatibilityRetirementEvidenceError(
            "governance evidence can only target a compatibility wrapper"
        )
    if not entry.replacement:
        raise CompatibilityRetirementEvidenceError(
            "compatibility lifecycle has no canonical replacement"
        )
    require_compatibility_evidence_token(
        "compatibility lifecycle entry_id", entry.entry_id
    )
    require_compatibility_evidence_token(
        "compatibility lifecycle target_release", entry.target_release
    )
    if not (
        entry.repository_caller_audit_method
        and entry.repository_caller_audit_status
        and entry.repository_caller_audit_scope
    ):
        raise CompatibilityRetirementEvidenceError(
            "compatibility lifecycle repository caller audit metadata is empty"
        )
    for label, value in (
        ("repository caller audit file count", entry.repository_caller_audit_file_count),
        ("repository caller audit byte count", entry.repository_caller_audit_byte_count),
    ):
        if type(value) is not int or value < 0:
            raise CompatibilityRetirementEvidenceError(
                f"compatibility lifecycle {label} is invalid"
            )
    return entry


def load_compatibility_retirement_evidence_for_lifecycle(
    entry: ImplementationLifecycle,
    evidence_path: str | Path,
) -> CompatibilityRetirementEvidence:
    """Load a bounded attestation that may promote one wrapper to review candidate."""

    entry = _require_compatibility_lifecycle(entry)
    if entry.repository_caller_audit_status not in {
        "scanned_clear",
        "runtime_callers_detected",
    }:
        raise CompatibilityRetirementEvidenceError(
            "repository caller scan is required before retirement evidence import"
        )
    return load_compatibility_retirement_evidence_document(
        evidence_path,
        expected_entry_id=entry.entry_id,
        expected_target_release=entry.target_release,
        repository_runtime_callers=entry.repository_runtime_callers,
    )


def load_compatibility_retirement_evidence(
    entry_id: str,
    evidence_path: str | Path,
) -> CompatibilityRetirementEvidence:
    return load_compatibility_retirement_evidence_for_lifecycle(
        governed_lifecycle_for(entry_id), evidence_path
    )


def load_compatibility_parity_evidence_for_lifecycle(
    entry: ImplementationLifecycle,
    evidence_path: str | Path,
) -> CompatibilityParityEvidence:
    """Load one verified execute artifact for an attested parity evidence id."""

    entry = _require_compatibility_lifecycle(entry)
    return load_verified_compatibility_parity_evidence(
        evidence_path,
        expected_entry_id=entry.entry_id,
        expected_target_release=entry.target_release,
        expected_canonical_entrypoint=entry.replacement,
        expected_compatibility_entrypoint=entry.path,
        expected_compatibility_delegate_override_option=(
            entry.delegate_override_option
        ),
    )


def load_compatibility_parity_evidence(
    entry_id: str,
    evidence_path: str | Path,
) -> CompatibilityParityEvidence:
    return load_compatibility_parity_evidence_for_lifecycle(
        lifecycle_for(entry_id), evidence_path
    )


def load_compatibility_removal_authorization_for_lifecycle(
    entry: ImplementationLifecycle,
    authorization_path: str | Path,
) -> CompatibilityRemovalAuthorization:
    """Load a strict target-release authorization for one compatibility entry."""

    entry = _require_compatibility_lifecycle(entry)
    return load_compatibility_removal_authorization_document(
        authorization_path,
        expected_entry_id=entry.entry_id,
        expected_target_release=entry.target_release,
    )


def load_compatibility_removal_authorization(
    entry_id: str,
    authorization_path: str | Path,
) -> CompatibilityRemovalAuthorization:
    return load_compatibility_removal_authorization_for_lifecycle(
        lifecycle_for(entry_id), authorization_path
    )


def lifecycle_payload_for_entry(
    entry: ImplementationLifecycle,
    retirement_evidence: Optional[CompatibilityRetirementEvidence] = None,
    parity_evidence: Tuple[CompatibilityParityEvidence, ...] = (),
    external_caller_evidence: Tuple[CompatibilityExternalCallerEvidence, ...] = (),
    removal_authorization: Optional[CompatibilityRemovalAuthorization] = None,
) -> Dict[str, object]:
    if entry.role == "compatibility_wrapper":
        entry = _require_compatibility_lifecycle(entry)
    if retirement_evidence is None and (
        parity_evidence or external_caller_evidence or removal_authorization
    ):
        raise CompatibilityRetirementEvidenceError(
            "linked evidence cannot be bound without a retirement attestation"
        )
    if retirement_evidence is not None and (
        entry.repository_caller_audit_status
        not in {"scanned_clear", "runtime_callers_detected"}
    ):
        raise CompatibilityRetirementEvidenceError(
            "repository caller scan is required before candidate promotion"
        )
    candidate_evidence = None
    removal_candidate = retirement_evidence is not None
    if retirement_evidence is not None:
        if entry.role != "compatibility_wrapper":
            raise CompatibilityRetirementEvidenceError(
                "retirement evidence can only target a compatibility wrapper"
            )
        if entry.replacement is None:
            raise CompatibilityRetirementEvidenceError(
                "compatibility lifecycle has no canonical replacement"
            )
        candidate_evidence = bind_compatibility_retirement_candidate_evidence(
            expected_entry_id=entry.entry_id,
            expected_target_release=entry.target_release,
            expected_canonical_entrypoint=entry.replacement,
            expected_compatibility_entrypoint=entry.path,
            expected_compatibility_delegate_override_option=(
                entry.delegate_override_option
            ),
            repository_runtime_callers=entry.repository_runtime_callers,
            retirement_evidence=retirement_evidence,
            external_caller_evidence=external_caller_evidence,
            parity_evidence=parity_evidence,
        )
    authorization_binding = None
    if removal_authorization is not None:
        if candidate_evidence is None or retirement_evidence is None:
            raise CompatibilityRetirementEvidenceError(
                "removal authorization requires validated candidate evidence"
            )
        authorization_binding = bind_compatibility_removal_authorization(
            expected_entry_id=entry.entry_id,
            expected_target_release=entry.target_release,
            repository_runtime_callers=entry.repository_runtime_callers,
            retirement_evidence=retirement_evidence,
            candidate_evidence=candidate_evidence,
            authorization=removal_authorization,
        )
    removal_allowed = authorization_binding is not None
    effective_status = (
        "removal_authorized"
        if removal_allowed
        else "removal_candidate"
        if removal_candidate
        else entry.status
    )
    effective_caller_audit_status = (
        "repository_and_external_callers_attested_clear"
        if removal_candidate
        else entry.caller_audit_status
    )
    effective_external_audit_status = (
        "attested_" + retirement_evidence.external_caller_audit_status
        if retirement_evidence is not None
        else entry.external_caller_audit_status
    )
    effective_parity_status = (
        "verified_from_linked_execute_parity_evidence"
        if removal_candidate
        else entry.behavior_parity_status
    )
    effective_removal_blocker = (
        "none; target-release removal is authorized but remains an explicit "
        "operator action"
        if removal_allowed
        else "candidate evidence is imported; explicit target-release source "
        "removal has not been authorized or executed"
        if removal_candidate
        else entry.removal_blocker
    )
    return {
        "entry_id": entry.entry_id,
        "path": entry.display_path or entry.path,
        "role": entry.role,
        "status": effective_status,
        "registry_status": entry.status,
        "canonical_owner": entry.canonical_owner,
        "replacement": entry.display_replacement or entry.replacement,
        "delegate_override_option": entry.delegate_override_option,
        "enterprise_counterpart": entry.enterprise_counterpart,
        "known_callers": list(entry.known_callers),
        "known_callers_semantics": (
            "legacy_union_of_runtime_callers_and_documentation_references"
            if entry.role == "compatibility_wrapper"
            else "repository_references"
        ),
        "caller_inventory": {
            "repository_runtime_callers": list(entry.repository_runtime_callers),
            "repository_runtime_caller_count": len(
                entry.repository_runtime_callers
            ),
            "documentation_references": list(entry.documentation_references),
            "documentation_reference_count": len(entry.documentation_references),
            "external_caller_audit_status": effective_external_audit_status,
            "repository_caller_audit_method": (
                entry.repository_caller_audit_method
            ),
            "repository_caller_audit_status": (
                entry.repository_caller_audit_status
            ),
            "repository_caller_audit_scope": (
                entry.repository_caller_audit_scope
            ),
            "repository_caller_audit_file_count": (
                entry.repository_caller_audit_file_count
            ),
            "repository_caller_audit_byte_count": (
                entry.repository_caller_audit_byte_count
            ),
            "repository_non_runtime_references": list(
                entry.repository_non_runtime_references
            ),
            "repository_non_runtime_reference_count": len(
                entry.repository_non_runtime_references
            ),
        },
        "caller_audit_status": effective_caller_audit_status,
        "behavior_parity_status": effective_parity_status,
        "removal_condition": entry.removal_condition,
        "removal_blocker": effective_removal_blocker,
        "removal_candidate": removal_candidate,
        "removal_allowed": removal_allowed,
        "automatic_removal": False,
        "target_release": entry.target_release,
        "claim_boundary": entry.claim_boundary,
        "retirement_evidence": (
            compatibility_retirement_evidence_payload(retirement_evidence)
            if retirement_evidence is not None
            else missing_compatibility_retirement_evidence_payload(
                required_for_candidate_promotion=(
                    entry.role == "compatibility_wrapper"
                )
            )
        ),
        **compatibility_retirement_candidate_evidence_payloads(
            candidate_evidence,
            required_for_candidate_promotion=(
                entry.role == "compatibility_wrapper"
            ),
        ),
        "removal_authorization_binding": (
            compatibility_removal_authorization_binding_payload(
                authorization_binding,
                required_for_removal_permission=(
                    entry.role == "compatibility_wrapper"
                ),
            )
        ),
    }


def lifecycle_payload(
    entry_id: str,
    retirement_evidence: Optional[CompatibilityRetirementEvidence] = None,
    parity_evidence: Tuple[CompatibilityParityEvidence, ...] = (),
    external_caller_evidence: Tuple[CompatibilityExternalCallerEvidence, ...] = (),
    removal_authorization: Optional[CompatibilityRemovalAuthorization] = None,
) -> Dict[str, object]:
    return lifecycle_payload_for_entry(
        governed_lifecycle_for(entry_id),
        retirement_evidence=retirement_evidence,
        parity_evidence=parity_evidence,
        external_caller_evidence=external_caller_evidence,
        removal_authorization=removal_authorization,
    )
