#!/usr/bin/env python3
"""Shared runner for lifecycle-managed native compatibility entrypoints."""

from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Any, Dict, Mapping

from advanced_serving.common import (
    artifact_command_parts,
    base_parser,
    command_succeeded,
    path_payload,
    repo_root,
    run,
    shell_join,
    write_json,
    write_text_file,
)
from scripts.implementation_lifecycle import (
    governed_lifecycle_for,
)
from scripts.compatibility_evidence_primitives import (
    CompatibilityRetirementEvidenceError,
)
from scripts.compatibility_governance_cli import (
    CompatibilityGovernanceCliError,
    add_compatibility_governance_arguments,
    compatibility_governance_location_values,
    load_compatibility_governance_projection,
    resolve_distinct_compatibility_locations,
    validate_compatibility_governance_arguments,
)
from scripts.compatibility_removal_authorization_auth import (
    compatibility_removal_authorization_child_environment,
)
from scripts.path_status import safe_is_file, safe_mtime_ns


@dataclass(frozen=True)
class NativeCompatibilitySpec:
    entry_id: str
    report_title: str
    output: str
    report: str
    core_source: str
    native_artifact: str
    main_report: str
    extra_payload: Mapping[str, Any] = field(default_factory=dict)


def _write_report(path: str, payload: Dict[str, Any], spec: NativeCompatibilitySpec) -> None:
    lifecycle = payload["compatibility_lifecycle"]
    caller_inventory = lifecycle["caller_inventory"]
    retirement_evidence = lifecycle["retirement_evidence"]
    parity_binding = lifecycle["parity_evidence_binding"]
    external_caller_binding = lifecycle["external_caller_evidence_binding"]
    authorization_binding = lifecycle["removal_authorization_binding"]
    write_text_file(
        path,
        "\n".join(
            [
                f"# {spec.report_title}",
                "",
                "This compatibility entry keeps the earlier development-document path alive.",
                f"The production implementation is `{lifecycle['canonical_owner']}`.",
                "",
                "## Lifecycle",
                "",
                f"- Status: `{lifecycle['status']}`",
                f"- Registry status: `{lifecycle['registry_status']}`",
                f"- Caller audit: `{lifecycle['caller_audit_status']}`",
                "- Repository runtime callers: "
                f"`{caller_inventory['repository_runtime_caller_count']}`",
                "- Documentation references: "
                f"`{caller_inventory['documentation_reference_count']}`",
                "- External caller audit: "
                f"`{caller_inventory['external_caller_audit_status']}`",
                f"- Behavior parity: `{lifecycle['behavior_parity_status']}`",
                f"- Retirement evidence: `{retirement_evidence['status']}`",
                "- External caller evidence binding: "
                f"`{external_caller_binding['status']}`",
                "- External caller binding key: "
                f"`{external_caller_binding['binding_key']}`",
                "- Linked external caller evidence: "
                f"`{external_caller_binding['evidence_count']}`",
                f"- Parity evidence binding: `{parity_binding['status']}`",
                f"- Parity binding key: `{parity_binding['binding_key']}`",
                f"- Linked parity evidence: `{parity_binding['evidence_count']}`",
                "- Target-release authorization: "
                f"`{authorization_binding['status']}`",
                "- Native artifact refresh: "
                f"`{payload['native_artifact_refresh']['status']}`",
                "- Removal candidate: "
                f"`{str(lifecycle['removal_candidate']).lower()}`",
                "- Removal allowed: "
                f"`{str(lifecycle['removal_allowed']).lower()}`",
                f"- Removal blocker: {lifecycle['removal_blocker']}",
                f"- Removal condition: {lifecycle['removal_condition']}",
                f"- Target release: `{lifecycle['target_release']}`",
                "",
                "## Command",
                "",
                f"- `{payload['native_command_shell']}`",
                "",
                "## Outputs",
                "",
                f"- Planned output: `{payload['output']}`",
                f"- Native artifact: `{spec.native_artifact}`",
                f"- Main report: `{spec.main_report}`",
                "",
            ]
        ),
    )


def run_native_compatibility(
    spec: NativeCompatibilitySpec,
    *,
    description: str,
) -> int:
    lifecycle = governed_lifecycle_for(spec.entry_id, repo_root())
    if lifecycle.role != "compatibility_wrapper":
        raise ValueError(
            f"lifecycle entry is not a compatibility wrapper: {spec.entry_id}"
        )
    if not lifecycle.replacement:
        raise ValueError(
            f"compatibility lifecycle has no replacement: {spec.entry_id}"
        )
    if lifecycle.delegate_override_option != "--binary":
        raise ValueError(
            f"native compatibility delegate option drift: {spec.entry_id}"
        )

    parser = base_parser(description=description)
    parser.allow_abbrev = False
    parser.add_argument(
        lifecycle.delegate_override_option,
        dest="binary",
        default=lifecycle.replacement,
    )
    parser.add_argument("--output", default=spec.output)
    parser.add_argument("--report", default=spec.report)
    add_compatibility_governance_arguments(parser)
    args, passthrough = parser.parse_known_args()

    try:
        validate_compatibility_governance_arguments(args)
    except CompatibilityGovernanceCliError as exc:
        parser.error(str(exc))
    location_values = {
        "native_binary": args.binary,
        "wrapper_output": args.output,
        "native_artifact": spec.native_artifact,
        "report": args.report,
    }
    location_values.update(compatibility_governance_location_values(args))
    try:
        resolved_locations = resolve_distinct_compatibility_locations(
            location_values,
            repo_root=repo_root(),
        )
    except CompatibilityGovernanceCliError as exc:
        parser.error(str(exc))
    native_artifact_path = resolved_locations["native_artifact"]

    try:
        lifecycle_projection = load_compatibility_governance_projection(
            lifecycle=lifecycle,
            args=args,
            resolved_locations=resolved_locations,
        )
    except (
        CompatibilityGovernanceCliError,
        CompatibilityRetirementEvidenceError,
    ) as exc:
        parser.error(str(exc))

    command = [args.binary]
    if "--repo-root" not in passthrough:
        command.extend(["--repo-root", "."])
    command.extend(["--output", spec.native_artifact, *passthrough])
    artifact_command = artifact_command_parts(command, ("--repo-root", "--output"))
    payload: Dict[str, Any] = {
        "schema_version": 1,
        "timestamp_utc": datetime.now(timezone.utc)
        .replace(microsecond=0)
        .isoformat()
        .replace("+00:00", "Z"),
        **path_payload(repo_root(), "repo_root", "repo_root_path_redacted"),
        "status": "delegated" if args.execute else "planned",
        "executed": bool(args.execute),
        "core_language": "C++17",
        "core_source": spec.core_source,
        **path_payload(args.binary, "native_binary", "native_binary_path_redacted"),
        "native_command": artifact_command,
        "native_command_shell": shell_join(artifact_command),
        **path_payload(
            spec.native_artifact,
            "native_artifact",
            "native_artifact_path_redacted",
        ),
        "native_artifact_refresh": {
            "status": "not_run",
            "executed": False,
            "preexisting": False,
            "regular_file_after": False,
            "mtime_change_observed": False,
            "claim_boundary": (
                "refresh observation only; does not prove semantic artifact parity, "
                "benchmark performance, or release readiness"
            ),
        },
        **path_payload(args.output, "output", "output_path_redacted"),
        **path_payload(args.report, "report", "report_path_redacted"),
        "compatibility_lifecycle": lifecycle_projection,
        **dict(spec.extra_payload),
    }

    exit_code = 0
    if args.execute:
        before_exists, before_mtime_ns, before_probe_error = safe_mtime_ns(
            native_artifact_path
        )
        before_is_file, before_file_error = safe_is_file(native_artifact_path)
        if before_probe_error or before_file_error:
            parser.error("native_artifact path could not be probed before execution")
        if before_exists and not before_is_file:
            parser.error("native_artifact must not be an existing non-file path")

        result = run(
            command,
            args.timeout_seconds,
            cwd=repo_root(),
            environment=(
                compatibility_removal_authorization_child_environment()
            ),
        )
        payload["execution_result"] = result
        command_ok = command_succeeded(result)
        refresh = payload["native_artifact_refresh"]
        refresh.update(
            {
                "executed": True,
                "preexisting": before_exists,
            }
        )
        if not command_ok:
            refresh["status"] = "command_failed"
            payload["status"] = "failed"
            exit_code = int(result.get("returncode") or 1)
        else:
            after_exists, after_mtime_ns, after_probe_error = safe_mtime_ns(
                native_artifact_path
            )
            after_is_file, after_file_error = safe_is_file(native_artifact_path)
            refresh["regular_file_after"] = bool(after_exists and after_is_file)
            refresh["mtime_change_observed"] = bool(
                after_exists
                and (not before_exists or before_mtime_ns != after_mtime_ns)
            )
            if after_probe_error or after_file_error:
                refresh["status"] = "post_run_probe_failed"
            elif not after_exists or not after_is_file:
                refresh["status"] = "missing_after_success"
            elif before_exists and before_mtime_ns == after_mtime_ns:
                refresh["status"] = "refresh_not_observed"
            else:
                refresh["status"] = "refreshed"

            payload["status"] = (
                "ok" if refresh["status"] == "refreshed" else "failed"
            )
            if payload["status"] == "failed":
                exit_code = 1
    write_json(args.output, payload)
    _write_report(args.report, payload, spec)
    return exit_code
