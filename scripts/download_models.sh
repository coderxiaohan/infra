#!/usr/bin/env bash
set -euo pipefail

readonly HF_REPO="distilbert/distilgpt2"
readonly HF_REVISION="ca98be8f8f0994e707b944a9ef55e66fbcf9e586"
readonly LLAMA_CPP_REVISION="11cd98842874cc1b87ac274bd2d5cceb38102bb2"
readonly GGUF_SHA256="c2714a823e581c419bb4a7d18ebfb5162c87e9e5bfb1bcf91600a9c66c6d9981"

readonly MODEL_FILES=(
  "config.json"
  "generation_config.json"
  "merges.txt"
  "model.safetensors"
  "tokenizer.json"
  "tokenizer_config.json"
  "vocab.json"
)

readonly MODEL_SHA256=(
  "4ec5947c1d59fee6212cdf3b0ec1a53eac02092554c5ff0a733488cbd2c64f3a"
  "b90eadacf585a743a30ea51e8b5c88b8d282a2a34dc0c7e556d0987cdbd68805"
  "1ce1664773c50f3e0cc8842619a93edc4624525b728b188a9e0be33b7726adc5"
  "9e5f2c3056ef1aba650847f27aa947c73e8bd15839f12311b62d501c602c53a6"
  "8414cab924d8b9b33013f0d221c5862f365ee9be39c5c2bfae8a5a9e970478a6"
  "5e04eb606e3a1583530a42e36c2a6b6615c86f34fe77e44d9ddeb43ff940931f"
  "196139668be63f3b5d6574427317ae82f612a97c5d1cdaf36ed2256dbf636783"
)

usage() {
  cat <<'EOF'
Usage: scripts/download_models.sh [--download-only] [--force]

Downloads the pinned DistilGPT2 Hugging Face files, verifies every file, and
converts the model to models/distilgpt2-f16.gguf with the pinned llama.cpp
submodule.

Environment overrides:
  INFERLAB_MODEL_DIR           Destination directory (default: <repo>/models)
  INFERLAB_LLAMA_CPP_DIR       llama.cpp checkout (default: <repo>/third_party/llama.cpp)
  INFERLAB_CONVERT_PYTHON      Python with llama.cpp conversion dependencies
EOF
}

fail() {
  printf 'error: %s\n' "$*" >&2
  exit 1
}

sha256_of() {
  sha256sum "$1" | awk '{print $1}'
}

verify_hf_model() {
  local directory="$1"
  local index file actual

  for index in "${!MODEL_FILES[@]}"; do
    file="${MODEL_FILES[$index]}"
    [[ -f "${directory}/${file}" ]] || return 1
    actual="$(sha256_of "${directory}/${file}")"
    [[ "$actual" == "${MODEL_SHA256[$index]}" ]] || return 1
  done
}

download_only=0
force=0
while (($#)); do
  case "$1" in
    --download-only)
      download_only=1
      ;;
    --force)
      force=1
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      fail "unknown argument: $1"
      ;;
  esac
  shift
done

command -v curl >/dev/null 2>&1 || fail "curl is required"
command -v sha256sum >/dev/null 2>&1 || fail "sha256sum is required"

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "${script_dir}/.." && pwd)"
model_dir="${INFERLAB_MODEL_DIR:-${repo_root}/models}"
hf_model_dir="${model_dir}/distilgpt2_clean_hf"
gguf_path="${model_dir}/distilgpt2-f16.gguf"
llama_cpp_dir="${INFERLAB_LLAMA_CPP_DIR:-${repo_root}/third_party/llama.cpp}"
python_bin="${INFERLAB_CONVERT_PYTHON:-python3}"

mkdir -p "$model_dir"

if verify_hf_model "$hf_model_dir"; then
  printf 'Hugging Face model already verified: %s\n' "$hf_model_dir"
else
  if [[ -e "$hf_model_dir" && "$force" -ne 1 ]]; then
    fail "${hf_model_dir} exists but failed verification; inspect it or rerun with --force"
  fi

  temporary_dir="$(mktemp -d "${model_dir}/.distilgpt2-download.XXXXXX")"
  trap 'rm -rf -- "${temporary_dir:-}"' EXIT

  for index in "${!MODEL_FILES[@]}"; do
    file="${MODEL_FILES[$index]}"
    url="https://huggingface.co/${HF_REPO}/resolve/${HF_REVISION}/${file}?download=true"
    printf 'Downloading %s\n' "$file"
    curl --fail --location --retry 4 --retry-delay 2 \
      --output "${temporary_dir}/${file}" "$url"
    actual="$(sha256_of "${temporary_dir}/${file}")"
    [[ "$actual" == "${MODEL_SHA256[$index]}" ]] || \
      fail "SHA256 mismatch for ${file}: ${actual}"
  done

  verify_hf_model "$temporary_dir" || fail "downloaded model verification failed"
  if [[ -e "$hf_model_dir" ]]; then
    rm -rf -- "$hf_model_dir"
  fi
  mv -- "$temporary_dir" "$hf_model_dir"
  trap - EXIT
  printf 'Hugging Face model verified: %s\n' "$hf_model_dir"
fi

if [[ "$download_only" -eq 1 ]]; then
  exit 0
fi

converter="${llama_cpp_dir}/convert_hf_to_gguf.py"
[[ -f "$converter" ]] || fail "llama.cpp converter not found: ${converter}; initialize the submodule first"

actual_llama_revision="$(git -C "$llama_cpp_dir" rev-parse HEAD 2>/dev/null || true)"
[[ "$actual_llama_revision" == "$LLAMA_CPP_REVISION" ]] || \
  fail "llama.cpp must be checked out at ${LLAMA_CPP_REVISION}; found ${actual_llama_revision:-unknown}"

if [[ -f "$gguf_path" && "$(sha256_of "$gguf_path")" == "$GGUF_SHA256" ]]; then
  printf 'GGUF model already verified: %s\n' "$gguf_path"
  exit 0
fi

if [[ -e "$gguf_path" && "$force" -ne 1 ]]; then
  fail "${gguf_path} exists but failed verification; inspect it or rerun with --force"
fi

if ! "$python_bin" -c 'import numpy, sentencepiece, torch, transformers' >/dev/null 2>&1; then
  fail "conversion dependencies are missing; create a dedicated venv and install ${llama_cpp_dir}/requirements/requirements-convert_hf_to_gguf.txt, then set INFERLAB_CONVERT_PYTHON"
fi

temporary_gguf="${gguf_path}.tmp.$$"
trap 'rm -f -- "${temporary_gguf:-}"' EXIT
"$python_bin" "$converter" "$hf_model_dir" \
  --outfile "$temporary_gguf" \
  --outtype f16

actual_gguf_sha256="$(sha256_of "$temporary_gguf")"
[[ "$actual_gguf_sha256" == "$GGUF_SHA256" ]] || \
  fail "GGUF SHA256 mismatch: expected ${GGUF_SHA256}, found ${actual_gguf_sha256}"

mv -f -- "$temporary_gguf" "$gguf_path"
trap - EXIT
printf 'GGUF model verified: %s\n' "$gguf_path"
