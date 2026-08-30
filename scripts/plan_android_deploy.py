#!/usr/bin/env python3
"""Generate an Android/ARM deployment plan for InferLab edge experiments.

The script does not invoke the Android NDK, adb, or llama.cpp. It records
preflight state and emits reproducible commands so the same repository can be
moved to a machine that has the required Android or ARM toolchain.
"""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
from time_utils import utc_now
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple

from command_redaction import REDACTED_VALUE, artifact_path_payload as path_payload, redacted_path_label as path_label
from path_status import safe_exists
from repo_paths import default_repo_root, resolve_repo_path, resolved_repo_root
from text_output import write_text_file


PATH_LABEL_RE = re.compile(r"path_[0-9a-f]{16}(?:\.[A-Za-z0-9_.-]+)?")


def artifact_path(value: str) -> str:
    return path_label(value) if value else ""


def command_templates(commands: List[str]) -> List[str]:
    templated: List[str] = []
    for command in commands:
        text = PATH_LABEL_RE.sub("<target-path>", command)
        text = text.replace(REDACTED_VALUE, "<prompt>")
        templated.append(text)
    return templated


def auto_host_tag() -> str:
    system = platform.system().lower()
    machine = platform.machine().lower()
    if system == "linux":
        return "linux-x86_64" if "64" in machine else "linux-x86"
    if system == "darwin":
        return "darwin-x86_64"
    if system == "windows":
        return "windows-x86_64"
    return "linux-x86_64"


def path_status(path: str) -> Tuple[bool, str]:
    return safe_exists(Path(path)) if path else (False, "")


def resolved_path_status(repo_root: Path, path: str) -> Tuple[bool, str]:
    return safe_exists(resolve_repo_path(repo_root, path)) if path else (False, "")


def tool_probe(name: str, override: str = "") -> Dict[str, Any]:
    requested = override or name
    path = None
    path_probe_error_value = ""
    if override:
        exists, path_probe_error_value = safe_exists(Path(override))
    else:
        exists = False
    if override and exists:
        path = str(Path(override))
    elif requested:
        path = shutil.which(requested)
    return {
        "name": name,
        "requested": artifact_path(override) if override else requested,
        **(path_payload(path) if path else {"path": None, "path_redacted": False}),
        "available": bool(path),
        "path_probe_error": path_probe_error_value,
    }


def ndk_paths(ndk_root: str, host_tag: str) -> Dict[str, str]:
    if not ndk_root:
        return {
            "toolchain_file": "",
            "clangxx": "",
            "sysroot": "",
        }
    root = Path(ndk_root)
    bin_dir = root / "toolchains" / "llvm" / "prebuilt" / host_tag / "bin"
    clang_name = "clang++.cmd" if host_tag.startswith("windows") else "clang++"
    return {
        "toolchain_file": str(root / "build" / "cmake" / "android.toolchain.cmake"),
        "clangxx": str(bin_dir / clang_name),
        "sysroot": str(root / "toolchains" / "llvm" / "prebuilt" / host_tag / "sysroot"),
    }


def shell_join(parts: List[str]) -> str:
    quoted: List[str] = []
    for part in parts:
        if not part:
            quoted.append('""')
            continue
        if all(ch.isalnum() or ch in "._+-=/:,@" for ch in part):
            quoted.append(part)
        else:
            quoted.append("'" + part.replace("'", "'\\''") + "'")
    return " ".join(quoted)


def native_hello_commands(args: argparse.Namespace, paths: Dict[str, str]) -> List[str]:
    build_dir = args.native_build_dir
    commands = [
        shell_join(
            [
                "cmake",
                "-S",
                "android/native_hello",
                "-B",
                artifact_path(build_dir),
                f"-DCMAKE_TOOLCHAIN_FILE={artifact_path(paths['toolchain_file'])}",
                f"-DANDROID_ABI={args.abi}",
                f"-DANDROID_PLATFORM={args.android_platform}",
                "-DCMAKE_BUILD_TYPE=Release",
            ]
        ),
        shell_join(["cmake", "--build", artifact_path(build_dir), "--config", "Release", "-j", str(args.jobs)]),
        shell_join(["adb", "push", artifact_path(f"{build_dir}/inferlab_android_hello"), args.device_dir + "/"]),
        shell_join(["adb", "shell", f"chmod +x {args.device_dir}/inferlab_android_hello"]),
        shell_join(["adb", "shell", f"{args.device_dir}/inferlab_android_hello"]),
    ]
    return commands


def llama_cpp_commands(args: argparse.Namespace, paths: Dict[str, str]) -> List[str]:
    build_dir = args.llama_build_dir
    model_name = Path(args.model).name if args.model else "model.gguf"
    device_model = f"{args.device_dir}/{model_name}"
    commands = [
        shell_join(
            [
                "cmake",
                "-S",
                artifact_path(args.llama_cpp_dir),
                "-B",
                artifact_path(build_dir),
                f"-DCMAKE_TOOLCHAIN_FILE={artifact_path(paths['toolchain_file'])}",
                f"-DANDROID_ABI={args.abi}",
                f"-DANDROID_PLATFORM={args.android_platform}",
                "-DCMAKE_BUILD_TYPE=Release",
                "-DGGML_OPENMP=OFF",
                "-DGGML_NATIVE=OFF",
                "-DGGML_LLAMAFILE=OFF",
            ]
        ),
        shell_join(["cmake", "--build", artifact_path(build_dir), "--config", "Release", "-j", str(args.jobs)]),
        shell_join(["adb", "shell", f"mkdir -p {args.device_dir}"]),
        shell_join(["adb", "push", artifact_path(args.model or "/path/to/model.gguf"), device_model]),
        shell_join(["adb", "push", artifact_path(f"{build_dir}/bin/llama-cli"), args.device_dir + "/"]),
        shell_join(["adb", "shell", f"chmod +x {args.device_dir}/llama-cli"]),
        shell_join(
            [
                "adb",
                "shell",
                f"{args.device_dir}/llama-cli -m {device_model} -p {REDACTED_VALUE!r} -n {args.n_predict} -t {args.threads}",
            ]
        ),
    ]
    return commands


def perf_commands(args: argparse.Namespace) -> List[str]:
    model_name = Path(args.model).name if args.model else "model.gguf"
    device_model = f"{args.device_dir}/{model_name}"
    return [
        shell_join(["adb", "shell", f"simpleperf stat --app '' {args.device_dir}/inferlab_android_hello"]),
        shell_join(
            [
                "adb",
                "shell",
                f"simpleperf stat {args.device_dir}/llama-cli -m {device_model} -p {REDACTED_VALUE!r} -n {args.n_predict} -t {args.threads}",
            ]
        ),
        shell_join(["adb", "shell", "cat /proc/cpuinfo | head -80"]),
        shell_join(["adb", "shell", "cat /sys/class/thermal/thermal_zone*/temp 2>/dev/null | head"]),
    ]


def build_plan(args: argparse.Namespace, repo_root: Path) -> Dict[str, Any]:
    host_tag = args.host_tag if args.host_tag != "auto" else auto_host_tag()
    paths = ndk_paths(args.ndk, host_tag)
    android_ndk_exists, android_ndk_probe_error = path_status(args.ndk)
    toolchain_file_exists, toolchain_file_probe_error = path_status(paths["toolchain_file"])
    clangxx_exists, clangxx_probe_error = path_status(paths["clangxx"])
    sysroot_exists, sysroot_probe_error = path_status(paths["sysroot"])
    llama_cpp_dir_exists, llama_cpp_dir_probe_error = resolved_path_status(repo_root, args.llama_cpp_dir)
    model_exists, model_probe_error = resolved_path_status(repo_root, args.model)
    preflight = {
        "android_ndk_root": artifact_path(args.ndk),
        "android_ndk_root_redacted": bool(args.ndk),
        "android_ndk_exists": android_ndk_exists,
        "android_ndk_probe_error": android_ndk_probe_error,
        "host_tag": host_tag,
        "toolchain_file": artifact_path(paths["toolchain_file"]),
        "toolchain_file_redacted": bool(paths["toolchain_file"]),
        "toolchain_file_exists": toolchain_file_exists,
        "toolchain_file_probe_error": toolchain_file_probe_error,
        "clangxx": artifact_path(paths["clangxx"]),
        "clangxx_redacted": bool(paths["clangxx"]),
        "clangxx_exists": clangxx_exists,
        "clangxx_probe_error": clangxx_probe_error,
        **path_payload(paths["sysroot"], "sysroot", "sysroot_redacted"),
        "sysroot_exists": sysroot_exists,
        "sysroot_probe_error": sysroot_probe_error,
        "adb": tool_probe("adb", args.adb),
        "cmake": tool_probe("cmake", args.cmake),
        **path_payload(args.llama_cpp_dir, "llama_cpp_dir", "llama_cpp_dir_redacted"),
        **path_payload(resolve_repo_path(repo_root, args.llama_cpp_dir), "resolved_llama_cpp_dir"),
        "llama_cpp_dir_exists": llama_cpp_dir_exists,
        "llama_cpp_dir_probe_error": llama_cpp_dir_probe_error,
        **path_payload(args.model, "model", "model_path_redacted"),
        **(
            path_payload(resolve_repo_path(repo_root, args.model), "resolved_model", "resolved_model_redacted")
            if args.model
            else {"resolved_model": "", "resolved_model_redacted": False}
        ),
        "model_exists": model_exists,
        "model_probe_error": model_probe_error,
    }
    commands = {
        "native_hello": native_hello_commands(args, paths),
        "llama_cpp": llama_cpp_commands(args, paths),
        "perf": perf_commands(args),
    }
    return {
        "schema_version": 1,
        "timestamp_utc": utc_now(),
        **path_payload(repo_root, "repo_root"),
        "target": {
            "abi": args.abi,
            "android_platform": args.android_platform,
            "device_dir": args.device_dir,
            "threads": args.threads,
            "n_predict": args.n_predict,
        },
        "preflight": preflight,
        "commands": commands,
        "command_templates": {name: command_templates(items) for name, items in commands.items()},
        "command_template_policy": "templates replace host paths and prompt content with target-machine placeholders",
        "notes": [
            "Commands are not executed by this planner.",
            "Run native_hello first to verify ABI, adb push, and executable permissions.",
            "Run llama.cpp only with a legal GGUF model path and a compatible device.",
            "Record thermal and CPU-frequency state before comparing x86, ARM Linux, and Android throughput.",
        ],
    }




def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo-root", default=str(default_repo_root()))
    parser.add_argument("--ndk", default=os.environ.get("ANDROID_NDK_HOME", ""))
    parser.add_argument("--adb", default="")
    parser.add_argument("--cmake", default="")
    parser.add_argument("--host-tag", default="auto")
    parser.add_argument("--abi", default="arm64-v8a")
    parser.add_argument("--android-platform", default="android-24")
    parser.add_argument("--device-dir", default="/data/local/tmp/inferlab")
    parser.add_argument("--native-build-dir", default="build/android-native-hello")
    parser.add_argument("--llama-cpp-dir", default="third_party/llama.cpp")
    parser.add_argument("--llama-build-dir", default="build/llama.cpp-android")
    parser.add_argument("--model", default="")
    parser.add_argument("--prompt", default="InferLab edge inference")
    parser.add_argument("--n-predict", default="32")
    parser.add_argument("--threads", default="4")
    parser.add_argument("--jobs", default="4")
    parser.add_argument("--output", default="results/raw/android_deploy_plan.json")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    repo_root = resolved_repo_root(args.repo_root)
    plan = build_plan(args, repo_root)
    output = resolve_repo_path(repo_root, args.output)
    write_text_file(output, json.dumps(plan, indent=2, sort_keys=True) + "\n")
    print(f"wrote {path_label(output)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
