#!/usr/bin/env bash
set -euo pipefail

LLAMA_CLI=${LLAMA_CLI:-llama-cli}
MODEL=${1:?usage: edge/run_gguf.sh /path/to/model.gguf [prompt]}
PROMPT=${2:-Explain KV cache in one sentence.}

exec "$LLAMA_CLI" -m "$MODEL" -p "$PROMPT" -n "${N_PREDICT:-64}" -c "${CTX_SIZE:-2048}" -t "${THREADS:-4}" ${LLAMA_EXTRA_ARGS:-}
