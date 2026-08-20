#!/usr/bin/env python3
"""Bounded Android device evidence projections for the ADB edge runner."""

from __future__ import annotations

import re
import shlex
from dataclasses import dataclass
from typing import Any, Callable, Dict, List, Mapping, Sequence


MAX_ANDROID_TEXT_BYTES = 512
MAX_CPU_IDS = 4096
MAX_CPU_POLICIES = 64
MAX_APK_SPLITS = 64
MAX_COUNTER_VALUE = (1 << 63) - 1
ANDROID_PACKAGE_RE = re.compile(r"[A-Za-z][A-Za-z0-9_]*(?:\.[A-Za-z][A-Za-z0-9_]*)+")
ABI_RE = re.compile(r"[A-Za-z0-9_.-]{1,64}")


@dataclass(frozen=True)
class AndroidProbeSpec:
    probe_id: str
    category: str
    shell: str
    required: bool = False


def planned_probe_records(
    specs: Sequence[AndroidProbeSpec],
    stage: str,
    command_builder: Callable[[str], Sequence[str]],
    command_projector: Callable[[Sequence[str]], Mapping[str, Any]],
) -> List[Dict[str, Any]]:
    records: List[Dict[str, Any]] = []
    for spec in specs:
        command = command_builder(spec.shell)
        records.append({
            "probe_id": spec.probe_id,
            "category": spec.category,
            "required": spec.required,
            "stage": stage,
            **command_projector(command),
            "status": "planned",
        })
    return records


def execute_probe_records(
    specs: Sequence[AndroidProbeSpec],
    stage: str,
    command_builder: Callable[[str], Sequence[str]],
    command_projector: Callable[[Sequence[str]], Mapping[str, Any]],
    command_executor: Callable[[Sequence[str]], Mapping[str, Any]],
    result_ok: Callable[[Mapping[str, Any]], bool],
) -> List[Dict[str, Any]]:
    records = planned_probe_records(specs, stage, command_builder, command_projector)
    executed: List[Dict[str, Any]] = []
    for spec, planned in zip(specs, records):
        record = dict(planned)
        result = command_executor(command_builder(spec.shell))
        record["status"] = "ok" if result_ok(result) else "failed"
        record["result"] = dict(result)
        executed.append(record)
    return executed


def probe_command_entries(records: Sequence[Mapping[str, Any]]) -> List[Dict[str, Any]]:
    return [
        {
            "stage": record.get("stage"),
            "probe_id": record.get("probe_id"),
            "command": record.get("command", []),
            "shell": record.get("shell", ""),
        }
        for record in records
    ]


def validate_android_package_name(value: str) -> None:
    if value and (len(value) > 255 or ANDROID_PACKAGE_RE.fullmatch(value) is None):
        raise ValueError("Android package name must be a dotted Java identifier up to 255 bytes")


def validate_android_abi(value: str) -> None:
    if ABI_RE.fullmatch(value) is None:
        raise ValueError("Android ABI must contain 1..64 letters, digits, dots, underscores or hyphens")


def identity_probe_specs() -> List[AndroidProbeSpec]:
    return [
        AndroidProbeSpec("device_model", "identity", "getprop ro.product.model", True),
        AndroidProbeSpec("soc_model", "identity", "getprop ro.soc.model"),
        AndroidProbeSpec("board_platform", "identity", "getprop ro.board.platform"),
        AndroidProbeSpec("android_api_level", "identity", "getprop ro.build.version.sdk", True),
        AndroidProbeSpec("primary_abi", "identity", "getprop ro.product.cpu.abi", True),
        AndroidProbeSpec("abi_list", "identity", "getprop ro.product.cpu.abilist"),
        AndroidProbeSpec("kernel_architecture", "identity", "uname -m"),
    ]


def _cpu_snapshot_shell() -> str:
    return " ".join(
        [
            "printf 'online|'; cat /sys/devices/system/cpu/online 2>/dev/null || true;",
            "printf 'present|'; cat /sys/devices/system/cpu/present 2>/dev/null || true;",
            "count=0; truncated=0;",
            "for policy in /sys/devices/system/cpu/cpufreq/policy*; do",
            "[ -d \"$policy\" ] || continue;",
            f"if [ \"$count\" -ge {MAX_CPU_POLICIES} ]; then truncated=1; break; fi;",
            "name=${policy##*/};",
            "related=$(cat \"$policy/related_cpus\" 2>/dev/null || true);",
            "minimum=$(cat \"$policy/cpuinfo_min_freq\" 2>/dev/null || true);",
            "maximum=$(cat \"$policy/cpuinfo_max_freq\" 2>/dev/null || true);",
            "current=$(cat \"$policy/scaling_cur_freq\" 2>/dev/null || true);",
            "printf 'policy|%s|%s|%s|%s|%s\n' \"$name\" \"$related\" \"$minimum\" \"$maximum\" \"$current\";",
            "count=$((count + 1));",
            "done;",
            "printf 'inventory|%s|%s\n' \"$count\" \"$truncated\"",
        ]
    )


def _thermal_snapshot_shell() -> str:
    return " ".join(
        [
            "seen=0; readable=0; truncated=0; maximum=''; hottest='';",
            "for zone in /sys/class/thermal/thermal_zone*; do",
            "[ -d \"$zone\" ] || continue;",
            "if [ \"$seen\" -ge 256 ]; then truncated=1; break; fi;",
            "seen=$((seen + 1));",
            "value=$(cat \"$zone/temp\" 2>/dev/null || true);",
            "case \"$value\" in -*) digits=${value#-} ;; *) digits=$value ;; esac;",
            "case \"$digits\" in ''|*[!0-9]*) continue ;; esac;",
            "kind=$(cat \"$zone/type\" 2>/dev/null || true);",
            "readable=$((readable + 1));",
            "if [ -z \"$maximum\" ] || [ \"$value\" -gt \"$maximum\" ]; then maximum=$value; hottest=$kind; fi;",
            "done;",
            "printf 'inventory|%s|%s|%s\n' \"$seen\" \"$readable\" \"$truncated\";",
            "printf 'maximum|%s|%s\n' \"$maximum\" \"$hottest\";",
            "dumpsys thermalservice 2>/dev/null |",
            r"sed -n 's/^[[:space:]]*Thermal Status:[[:space:]]*\([0-9][0-9]*\).*$/service_status|\1/p' | head -n 1",
        ]
    )


def _battery_snapshot_shell() -> str:
    fields = (
        "energy_now",
        "charge_counter",
        "voltage_now",
        "current_now",
        "capacity",
        "temp",
        "status",
        "health",
    )
    statements = ["root=/sys/class/power_supply/battery"]
    for field in fields:
        statements.append(
            f"if [ -r \"$root/{field}\" ]; then value=$(cat \"$root/{field}\" 2>/dev/null || true); "
            f"printf 'value|{field}|%s\\n' \"$value\"; fi"
        )
    return "; ".join(statements)


def resource_probe_specs() -> List[AndroidProbeSpec]:
    return [
        AndroidProbeSpec("cpu_snapshot", "resource", _cpu_snapshot_shell()),
        AndroidProbeSpec("thermal_snapshot", "resource", _thermal_snapshot_shell()),
        AndroidProbeSpec("battery_snapshot", "resource", _battery_snapshot_shell()),
    ]


def footprint_probe_spec(device_llama_cli: str, device_model: str, package_name: str) -> AndroidProbeSpec:
    binary = shlex.quote(device_llama_cli)
    model = shlex.quote(device_model)
    statements = [
        f"value=$(stat -c %s {binary} 2>/dev/null || true); printf 'runtime_binary_bytes|%s\\n' \"$value\"",
        f"value=$(stat -c %s {model} 2>/dev/null || true); printf 'model_file_bytes|%s\\n' \"$value\"",
    ]
    if package_name:
        package = shlex.quote(package_name)
        statements.append(
            " ".join(
                [
                    f"pm path {package} 2>/dev/null | {{ count=0; truncated=0;",
                    "while IFS= read -r entry; do",
                    f"if [ \"$count\" -ge {MAX_APK_SPLITS} ]; then truncated=1; break; fi;",
                    "case \"$entry\" in package:*) apk=${entry#package:} ;; *) continue ;; esac;",
                    "value=$(stat -c %s \"$apk\" 2>/dev/null || true);",
                    "printf 'package_apk_bytes|%s\n' \"$value\";",
                    "count=$((count + 1));",
                    "done;",
                    "printf 'package_inventory|%s|%s\n' \"$count\" \"$truncated\";",
                    "}",
                ]
            )
        )
    return AndroidProbeSpec("deployment_footprint", "footprint", "; ".join(statements))


def _record_map(records: Sequence[Mapping[str, Any]]) -> Dict[str, Mapping[str, Any]]:
    mapped: Dict[str, Mapping[str, Any]] = {}
    for record in records:
        probe_id = str(record.get("probe_id", ""))
        if not probe_id or probe_id in mapped:
            continue
        mapped[probe_id] = record
    return mapped


def _probe_text(record: Mapping[str, Any] | None) -> str:
    if not record or record.get("status") != "ok":
        return ""
    result = record.get("result")
    if not isinstance(result, Mapping):
        return ""
    observation = result.get("output_observation")
    if isinstance(observation, Mapping):
        text = str(observation.get("probe_text", "")) if observation.get("status") == "observed" else ""
    else:
        text = str(result.get("output_tail", ""))
    return text if len(text.encode("utf-8", errors="replace")) <= 64 * 1024 else ""


def _bounded_text(value: str) -> str | None:
    value = value.strip()
    if not value or len(value.encode("utf-8", errors="replace")) > MAX_ANDROID_TEXT_BYTES:
        return None
    if any(ord(character) < 32 for character in value):
        return None
    return value


def _last_bounded_line(text: str) -> str | None:
    for line in reversed(text.splitlines()):
        value = _bounded_text(line)
        if value is not None:
            return value
    return None


def _bounded_int(value: str, minimum: int = 0, maximum: int = MAX_COUNTER_VALUE) -> int | None:
    value = value.strip()
    if len(value) > 20 or re.fullmatch(r"-?[0-9]+", value) is None:
        return None
    number = int(value)
    return number if minimum <= number <= maximum else None


def parse_device_identity(
    records: Sequence[Mapping[str, Any]],
    required_abi: str = "arm64-v8a",
    minimum_api_level: int = 24,
) -> Dict[str, Any]:
    mapped = _record_map(records)
    values = {probe_id: _last_bounded_line(_probe_text(mapped.get(probe_id))) for probe_id in (
        "device_model", "soc_model", "board_platform", "android_api_level", "primary_abi", "abi_list", "kernel_architecture"
    )}
    api_level = _bounded_int(values["android_api_level"] or "", 1, 1000)
    primary_abi = values["primary_abi"] if values["primary_abi"] and ABI_RE.fullmatch(values["primary_abi"]) else None
    abi_values: List[str] = []
    if values["abi_list"]:
        candidates = [item.strip() for item in values["abi_list"].split(",")]
        if len(candidates) <= 32 and all(ABI_RE.fullmatch(item) for item in candidates):
            abi_values = candidates
    if primary_abi and primary_abi not in abi_values:
        abi_values.insert(0, primary_abi)
    required_gaps = [
        field
        for field, present in (
            ("device_model", values["device_model"] is not None),
            ("android_api_level", api_level is not None),
            ("primary_abi", primary_abi is not None),
        )
        if not present
    ]
    if api_level is not None and api_level < minimum_api_level:
        required_gaps.append("android_api_level_below_minimum")
    if primary_abi is not None and required_abi not in abi_values:
        required_gaps.append("required_abi_unavailable")
    return {
        "status": "observed" if not required_gaps else "missing_required_identity",
        "device_model": values["device_model"],
        "soc_model": values["soc_model"],
        "board_platform": values["board_platform"],
        "soc_identity": values["soc_model"] or values["board_platform"],
        "android_api_level": api_level,
        "primary_abi": primary_abi,
        "abi_list": abi_values,
        "kernel_architecture": values["kernel_architecture"],
        "required_abi": required_abi,
        "minimum_api_level": minimum_api_level,
        "required_gaps": required_gaps,
        "claim_boundary": "ADB properties identify the connected target for this run; they do not prove backend delegation or runtime correctness",
    }


def _parse_cpu_set(value: str) -> List[int] | None:
    value = value.strip()
    if not value:
        return None
    cpus: set[int] = set()
    for token in re.split(r"[\s,]+", value):
        bounds = token.strip().split("-", 1)
        first = _bounded_int(bounds[0], 0, MAX_CPU_IDS - 1)
        last = _bounded_int(bounds[-1], 0, MAX_CPU_IDS - 1)
        if first is None or last is None or first > last or last - first + len(cpus) >= MAX_CPU_IDS:
            return None
        cpus.update(range(first, last + 1))
    return sorted(cpus)


def parse_cpu_snapshot(record: Mapping[str, Any] | None) -> Dict[str, Any]:
    text = _probe_text(record)
    if not text:
        return {"status": "unavailable", "online_cpu_count": None, "present_cpu_count": None, "policies": [], "issue_count": 1}
    online: List[int] | None = None
    present: List[int] | None = None
    policies: List[Dict[str, Any]] = []
    seen_policies: set[str] = set()
    declared_count: int | None = None
    truncated = False
    issues = 0
    for line in text.splitlines():
        parts = line.strip().split("|")
        if len(parts) == 2 and parts[0] == "online":
            online = _parse_cpu_set(parts[1])
            issues += int(online is None)
        elif len(parts) == 2 and parts[0] == "present":
            present = _parse_cpu_set(parts[1])
            issues += int(present is None)
        elif len(parts) == 6 and parts[0] == "policy":
            name = _bounded_text(parts[1])
            related = _parse_cpu_set(parts[2])
            minimum = _bounded_int(parts[3], 1, 100_000_000)
            maximum = _bounded_int(parts[4], 1, 100_000_000)
            current = _bounded_int(parts[5], 1, 100_000_000)
            if not name or name in seen_policies or related is None or minimum is None or maximum is None or minimum > maximum:
                issues += 1
                continue
            seen_policies.add(name)
            issues += int(current is None)
            policies.append({
                "policy": name,
                "related_cpu_count": len(related),
                "minimum_khz": minimum,
                "maximum_khz": maximum,
                "current_khz": current,
                "current_status": "observed" if current is not None else "unavailable",
            })
        elif len(parts) == 3 and parts[0] == "inventory":
            declared_count = _bounded_int(parts[1], 0, MAX_CPU_POLICIES)
            truncated = parts[2] == "1"
            if declared_count is None or parts[2] not in {"0", "1"}:
                issues += 1
        elif line.strip():
            issues += 1
    if declared_count is None or declared_count != len(policies):
        issues += 1
    status = "observed" if online is not None and present is not None and policies and not truncated and not issues else "partial"
    if online is None and present is None and not policies:
        status = "unavailable"
    return {
        "status": status,
        "online_cpu_count": len(online) if online is not None else None,
        "present_cpu_count": len(present) if present is not None else None,
        "policy_count": len(policies),
        "inventory_truncated": truncated,
        "policies": policies,
        "issue_count": issues,
        "provenance": "android_sysfs_cpufreq_point_snapshot",
    }


THERMAL_STATUS_NAMES = {
    0: "none",
    1: "light",
    2: "moderate",
    3: "severe",
    4: "critical",
    5: "emergency",
    6: "shutdown",
}


def parse_thermal_snapshot(record: Mapping[str, Any] | None) -> Dict[str, Any]:
    text = _probe_text(record)
    seen = readable = maximum = service_status = None
    truncated = False
    hottest = None
    issues = 0
    for line in text.splitlines():
        parts = line.strip().split("|")
        if len(parts) == 4 and parts[0] == "inventory":
            seen = _bounded_int(parts[1], 0, 256)
            readable = _bounded_int(parts[2], 0, 256)
            truncated = parts[3] == "1"
            if seen is None or readable is None or readable > seen or parts[3] not in {"0", "1"}:
                issues += 1
        elif len(parts) == 3 and parts[0] == "maximum":
            maximum = _bounded_int(parts[1], -100_000, 300_000)
            hottest = _bounded_text(parts[2])
            if parts[1] and maximum is None:
                issues += 1
        elif len(parts) == 2 and parts[0] == "service_status":
            service_status = _bounded_int(parts[1], 0, 6)
            if service_status is None:
                issues += 1
        elif line.strip():
            issues += 1
    if seen is None or readable is None or truncated:
        issues += 1
    status = "observed" if maximum is not None and not issues else "partial"
    if maximum is None and service_status is None:
        status = "unavailable"
    return {
        "status": status,
        "maximum_millicelsius": maximum,
        "hottest_zone_type": hottest,
        "inspected_sensor_count": seen,
        "readable_sensor_count": readable,
        "inventory_truncated": truncated,
        "thermal_service_status": service_status,
        "thermal_service_status_name": THERMAL_STATUS_NAMES.get(service_status),
        "issue_count": issues,
        "provenance": "android_thermal_sysfs_and_thermalservice_point_snapshot",
        "claim_boundary": "two point snapshots do not prove throttling, sustained thermal stability or workload-exclusive heating",
    }


BATTERY_NUMERIC_FIELDS = {
    "energy_now": (0, MAX_COUNTER_VALUE, "microwatt_hours"),
    "charge_counter": (-MAX_COUNTER_VALUE, MAX_COUNTER_VALUE, "microamp_hours"),
    "voltage_now": (0, 1_000_000_000, "microvolts"),
    "current_now": (-1_000_000_000, 1_000_000_000, "microamps"),
    "capacity": (0, 100, "percent"),
    "temp": (-1_000, 3_000, "tenths_celsius"),
}


def parse_battery_snapshot(record: Mapping[str, Any] | None) -> Dict[str, Any]:
    values: Dict[str, Any] = {field: None for field in BATTERY_NUMERIC_FIELDS}
    values.update({"battery_status": None, "health": None})
    units = {field: spec[2] for field, spec in BATTERY_NUMERIC_FIELDS.items()}
    issues = 0
    seen: set[str] = set()
    for line in _probe_text(record).splitlines():
        parts = line.strip().split("|", 2)
        if len(parts) != 3 or parts[0] != "value" or parts[1] in seen:
            issues += int(bool(line.strip()))
            continue
        field, raw = parts[1], parts[2]
        seen.add(field)
        if field in BATTERY_NUMERIC_FIELDS:
            minimum, maximum, _ = BATTERY_NUMERIC_FIELDS[field]
            values[field] = _bounded_int(raw, minimum, maximum)
            issues += int(values[field] is None)
        elif field == "status":
            values["battery_status"] = _bounded_text(raw)
            issues += int(values["battery_status"] is None)
        elif field == "health":
            values["health"] = _bounded_text(raw)
            issues += int(values["health"] is None)
        else:
            issues += 1
    numeric_count = sum(values[field] is not None for field in BATTERY_NUMERIC_FIELDS)
    status = "observed" if numeric_count and not issues else "partial" if numeric_count else "unavailable"
    return {
        "status": status,
        **values,
        "units": units,
        "issue_count": issues,
        "measurement_capabilities": {
            "stored_energy_counter": values["energy_now"] is not None,
            "charge_counter": values["charge_counter"] is not None,
            "instantaneous_voltage": values["voltage_now"] is not None,
            "instantaneous_current": values["current_now"] is not None,
        },
        "provenance": "android_power_supply_battery_sysfs_point_snapshot",
    }


def battery_interval(
    before: Mapping[str, Any],
    after: Mapping[str, Any],
    observation_window_duration_ms: float | None,
) -> Dict[str, Any]:
    energy_change = charge_change = None
    method = "unavailable"
    if isinstance(before.get("energy_now"), int) and isinstance(after.get("energy_now"), int):
        energy_change = int(after["energy_now"]) - int(before["energy_now"])
        if abs(energy_change) > MAX_COUNTER_VALUE:
            energy_change = None
        elif energy_change is not None:
            method = "power_supply_energy_now_signed_change"
    if isinstance(before.get("charge_counter"), int) and isinstance(after.get("charge_counter"), int):
        charge_change = int(after["charge_counter"]) - int(before["charge_counter"])
        if abs(charge_change) > MAX_COUNTER_VALUE:
            charge_change = None
        if method == "unavailable" and charge_change is not None:
            method = "power_supply_charge_counter_signed_change"
    available = observation_window_duration_ms is not None and (energy_change is not None or charge_change is not None)
    return {
        "status": "observed_signed_counter_change" if available else "unavailable",
        "duration_ms": observation_window_duration_ms if available else None,
        "measurement_method": method if available else "unavailable",
        "stored_energy_counter_change_uwh": energy_change if available else None,
        "charge_counter_change_uah": charge_change if available else None,
        "before_battery_status": before.get("battery_status"),
        "after_battery_status": after.get("battery_status"),
        "claim_boundary": "signed whole-device battery counter change is not workload-exclusive energy, power, efficiency or attribution evidence",
    }


def parse_deployment_footprint(record: Mapping[str, Any] | None, package_requested: bool) -> Dict[str, Any]:
    runtime_bytes = model_bytes = None
    package_sizes: List[int] = []
    package_count = None
    package_truncated = False
    issues = 0
    for line in _probe_text(record).splitlines():
        parts = line.strip().split("|")
        if len(parts) == 2 and parts[0] in {"runtime_binary_bytes", "model_file_bytes"}:
            value = _bounded_int(parts[1], 1, MAX_COUNTER_VALUE)
            if parts[0] == "runtime_binary_bytes":
                runtime_bytes = value
            else:
                model_bytes = value
            issues += int(value is None)
        elif len(parts) == 2 and parts[0] == "package_apk_bytes":
            value = _bounded_int(parts[1], 1, MAX_COUNTER_VALUE)
            if value is None or len(package_sizes) >= MAX_APK_SPLITS:
                issues += 1
            else:
                package_sizes.append(value)
        elif len(parts) == 3 and parts[0] == "package_inventory":
            package_count = _bounded_int(parts[1], 0, MAX_APK_SPLITS)
            package_truncated = parts[2] == "1"
            if package_count is None or parts[2] not in {"0", "1"}:
                issues += 1
        elif line.strip():
            issues += 1
    package_total = sum(package_sizes)
    if package_total > MAX_COUNTER_VALUE:
        package_total = None
        issues += 1
    if package_requested and (package_count is None or package_count != len(package_sizes) or package_truncated or not package_sizes):
        package_total = None
        issues += 1
    package_status = "not_requested"
    if package_requested:
        package_status = "observed" if package_total is not None and not package_truncated else "unavailable"
    status = "observed" if runtime_bytes is not None and not issues else "partial" if runtime_bytes is not None or model_bytes is not None else "unavailable"
    return {
        "status": status,
        "runtime_binary_bytes": runtime_bytes,
        "model_file_bytes": model_bytes,
        "package": {
            "requested": package_requested,
            "status": package_status,
            "split_apk_count": package_count if package_requested else None,
            "total_archive_bytes": package_total if package_requested else None,
            "inventory_truncated": package_truncated if package_requested else False,
        },
        "issue_count": issues,
        "provenance": "android_device_stat_and_package_manager",
        "claim_boundary": "archive and file sizes exclude extracted data, runtime allocations, file-cache residency and external model transfer cost",
    }


def build_device_evidence(
    identity_records: Sequence[Mapping[str, Any]],
    before_records: Sequence[Mapping[str, Any]],
    after_records: Sequence[Mapping[str, Any]],
    footprint_record: Mapping[str, Any] | None,
    observation_window_duration_ms: float | None,
    package_requested: bool,
    execution_requested: bool,
    required_abi: str = "arm64-v8a",
    minimum_api_level: int = 24,
) -> Dict[str, Any]:
    if not execution_requested:
        return {
            "status": "planned",
            "device_identity": {
                "status": "planned",
                "required_abi": required_abi,
                "minimum_api_level": minimum_api_level,
                "required_gaps": [],
            },
            "resource_window": {"status": "planned", "duration_ms": None},
            "deployment_footprint": {"status": "planned"},
            "jni_lifecycle_observation": {"status": "not_exercised_by_standalone_cli"},
            "claim_scope": "plan records commands only and contains no Android device observation",
        }
    identity = parse_device_identity(identity_records, required_abi, minimum_api_level)
    before_map = _record_map(before_records)
    after_map = _record_map(after_records)
    before = {
        "cpu": parse_cpu_snapshot(before_map.get("cpu_snapshot")),
        "thermal": parse_thermal_snapshot(before_map.get("thermal_snapshot")),
        "battery": parse_battery_snapshot(before_map.get("battery_snapshot")),
    }
    after = {
        "cpu": parse_cpu_snapshot(after_map.get("cpu_snapshot")),
        "thermal": parse_thermal_snapshot(after_map.get("thermal_snapshot")),
        "battery": parse_battery_snapshot(after_map.get("battery_snapshot")),
    }
    battery = battery_interval(before["battery"], after["battery"], observation_window_duration_ms)
    footprint = parse_deployment_footprint(footprint_record, package_requested)
    resource_statuses = [before["cpu"]["status"], before["thermal"]["status"], before["battery"]["status"], after["cpu"]["status"], after["thermal"]["status"], after["battery"]["status"]]
    resource_gap_count = sum(status != "observed" for status in resource_statuses)
    resource_gap_count += int(battery["status"] != "observed_signed_counter_change")
    optional_gap_count = resource_gap_count + int(footprint["status"] != "observed")
    status = "observed"
    if identity["required_gaps"]:
        status = "blocked_required_identity"
    elif optional_gap_count:
        status = "partial"
    return {
        "status": status,
        "device_identity": identity,
        "resource_window": {
            "status": "observed" if observation_window_duration_ms is not None and not resource_gap_count else "partial" if observation_window_duration_ms is not None else "unavailable",
            "duration_ms": observation_window_duration_ms,
            "before": before,
            "after": after,
            "battery_counter_interval": battery,
            "claim_boundary": "host-bounded ADB snapshots bracket the scenario matrix but are not synchronized per-token telemetry",
        },
        "deployment_footprint": footprint,
        "jni_lifecycle_observation": {
            "status": "not_exercised_by_standalone_cli",
            "reason": "this runner invokes the external llama.cpp CLI, not the InferLab Android JNI/AAR bridge",
        },
        "summary": {
            "required_identity_gap_count": len(identity["required_gaps"]),
            "optional_evidence_gap_count": optional_gap_count,
            "resource_evidence_gap_count": resource_gap_count,
            "battery_counter_interval_observed": battery["status"] == "observed_signed_counter_change",
            "package_size_observed": footprint["package"]["status"] == "observed",
        },
        "claim_scope": "ADB device identity, point resource snapshots, signed battery-counter change and deployment footprint for an external llama.cpp CLI run; no JNI lifecycle, backend delegation, workload-exclusive energy or release claim",
    }
