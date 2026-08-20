# Edge Runtime Matrix

## Purpose

This report defines InferLab's canonical embedded edge-runtime evidence surface.
The implementation uses one C++17 lifecycle contract for llama.cpp and
ExecuTorch, one backend-aware scalar/ARM-NEON kernel dispatcher, and one
catalog-managed artifact. It does not replace the existing external llama.cpp
command runner, Android ADB runner, or edge compiler artifact matrix.

## Implemented Surface

- `inferlab::IEdgeRuntime` defines `load`, `generate`, streaming token callback,
  idempotent `cancel`, `reset`, and safe `unload` semantics.
- `rms_norm_with_backend`, `apply_rope_with_backend`, and
  `fused_int4_dequant_matvec_with_backend` reuse the existing scalar contracts
  and add an AArch64 NEON implementation behind one dispatcher.
- `auto` selection falls back deterministically to scalar. Explicit NEON
  selection fails when the implementation or runtime capability is absent.
  Unknown backend enum values fail closed. `sve_requested` records the reserved
  CMake request separately, while SVE remains uncompiled and unavailable
  because no SVE kernel exists.
- The canonical CPU LayerNorm/RMSNorm owner accumulates mean, variance and
  square reductions in FP64 before checked float projection. NEON RMSNorm uses
  the same reduction range in FP64 vector lanes, so backend choice does not
  change whether a finite input fails solely because of FP32 accumulator
  overflow.
- `inferlab_edge_runtime_matrix` reads one runtime matrix configuration and
  emits dependency, model, ISA, lifecycle, blocker, and metric-provenance rows.
- `target_platform` is an exact canonical identity, not a substring hint. The
  current bounded set covers Linux AArch64/x86_64. Android, macOS and iOS stay
  outside runtime-ready scope until their later milestones. Unknown ids fail
  config parsing, and Apple build detection already distinguishes macOS from
  iOS for future matching. A recognized
  but different build emits `mismatch_compiled_architecture`; it is not folded
  together with an unknown platform.
- Configure/build/run recommendations derive the build directory and NEON
  option from the canonical Linux target; x86_64 never receives the ARM build
  directory or `INFERLAB_ENABLE_ARM_NEON=ON`.
- When the compiled host and target do not match, configure recommendations
  include an explicit target-specific `CMAKE_TOOLCHAIN_FILE` placeholder.
  Enabling NEON alone is never represented as cross-build readiness, and the
  command does not prove that configuration, linkage, or execution succeeded.
- `edge_runtime_matrix_execution` is the single factory/capability/lifecycle
  orchestration owner; configuration, resource planning and summary remain in
  the model, bounded generated-text comparison derive/validation lives in
  `edge_runtime_quality_comparison`, and JSON projection remains in the view.
- The same execution owner now runs one isolated cold probe, configured serial
  warmup iterations, and a bounded measurement loop governed by minimum
  iterations, minimum sustained duration, and a maximum-iteration guard. Warm
  latency/throughput fields use per-field medians only when metric presence is
  consistent across all accepted samples; RSS/PSS use maxima and mapped-model
  bytes use the latest accepted-sample checkpoint, including null when that
  probe is unavailable. Warmup and cancel-probe timings
  never enter the median.
- Every repeated sample must reconcile callback sequence/count, prompt and
  generated token counts, finish reason, metric provenance, KV bytes and
  workspace bytes with the cold probe. Drift fails the protocol instead of
  being averaged away. Generated text is captured only for the bounded cold
  primary quality observation; warmup, measurement and concurrent performance
  probes do not copy it, avoiding quality bookkeeping in ranked TPOT/throughput
  samples. An oversized cold capture only makes text-quality comparison
  unavailable and does not turn a completed performance run into a failure.
  This is in-process stability evidence and does not claim power, energy,
  throttling or production behavior.
- Configuration processing is two-phase: every row must complete structured
  parsing and static model/tokenizer/resource preflight before any adapter is
  invoked. A malformed or statically invalid later row cannot leave an earlier
  runtime partially executed and then be relabeled as malformed input.
- Multi-session execute now creates one independent `IEdgeRuntime` instance per
  configured session, reuses the serial primary as session zero, normalizes all
  instances with one generation/reset, and releases one measured generation per
  session through a C++17 start gate. No adapter instance is called by two
  threads concurrently.
- The concurrent projection records loaded/warmed/started/completed counts,
  total tokens/callbacks, wall time, median per-session warm TTFT/TPOT,
  aggregate generated-token throughput and concurrency-window process metrics.
  Every session must match the serial primary's callback, token, finish,
  provenance, KV and workspace identity. This is independent-runtime
  concurrency, not continuous batching or shared-context scheduling.
- Concurrent process observations are merged by actual worker completion order,
  not session index. That preserves latest-checkpoint mapping semantics while
  RSS/PSS remain maxima. The later cancel probe may update the row-level latest
  mapping, so the concurrency projection retains its own window mapping value.
- The shared request carries one prompt, so `batch_size` is currently fixed to
  one by the canonical request validator. llama.cpp does not reuse that field
  as `n_batch`: its token submission capacities start from the pinned runtime
  defaults, are bounded by context capacity, and drive chunked prefill. This
  keeps request cardinality comparable with ExecuTorch while preserving normal
  multi-token prompt decode behavior.
- The fixed registration retains cross-target product backend/instruction-set
  declarations so an x86 host can produce a valid AArch64 migration plan. One
  central architecture projection validates the requested target, and the
  current-build wrapper reuses it for execute. Thus an x86 target plan and an
  x86 binary both reject `cpu-neon`. Plan preflight, both adapters and execute
  reconciliation borrow these projections instead of duplicating
  version/backend/ISA/lifecycle booleans; linked llama.cpp mmap support is
  supplied there by the official runtime probe.
- The optional llama.cpp adapter is pinned to b10173/e9fa078, requires a clean
  source worktree, resolves the pin and current source HEAD to complete commits
  and requires exact equality before linking. It rejects non-decoder-only GGUF
  at load and maps CPU execution, mmap policy, chunked prefill, greedy/top-p
  sampling, streaming callback, cancellation, reset, and unload into
  `IEdgeRuntime`. After context creation it reconciles actual total/per-sequence
  context and logical/physical batch capacities; prefill uses actual logical
  `n_batch`, not the requested parameter copy.
- The optional ExecuTorch adapter verifies the v1.2.0/0b0e2c5 source checkout
  and requires the separately installed SDK to carry matching build-revision
  metadata before it checks `extension_llm_runner`, XNNPACK, and
  static registration link support, then reuses the official `TextLLMRunner`,
  `GenerationConfig`, `stop/reset`, tokenizer loader, and `Stats` APIs. Its
  first scope is PTE + XNNPACK, batch size one, greedy generation; thread count
  is explicitly reported as runtime-managed rather than silently claimed.
  Build-revision metadata prevents accidental SDK/source mismatch; it is not a
  signature or cryptographic supply-chain attestation.
- The shared runtime contract validates load/reset status-error coherence and
  generation metric finiteness, non-negativity, provenance and finish semantics
  before the execution owner accepts any adapter observation.
- Resource rows reuse the canonical KV-cache estimator and combine mmap-shareable
  or session-scaled buffered model residency, session-scaled KV estimate,
  checked per-session runtime-workspace total, and system reserve without presenting
  the estimate as observed RSS/PSS. One resource-plan owner also projects
  estimated headroom/shortfall, per-session target-core-aware thread guidance, mmap
  strategy and execute-observed working-set headroom/shortfall. PSS is
  preferred for the observed budget; process-lifetime RSS is an explicit
  fallback rather than a per-request measurement. The artifact also declares the ARM Linux
  package directory and manifest contract without claiming a package was built.
- Matrix self-validation requires complete runtime/model identity and all five
  lifecycle statuses. The resource-plan owner independently reconciles
  analytical totals and observed working-set source/fit/headroom/shortfall, so
  a stale internal projection fails before JSON rendering.
- Empty or metadata-unreadable model files never receive a resource-fit
  estimate or enter the estimated candidate set. A resource-fitting row also
  stays outside that set when its pinned runtime version, model format, product
  backend or target-architecture backend contract is incompatible. Dependency
  linkage and model readiness remain visible blockers without erasing a valid
  target planning identity. ExecuTorch also blocks empty or unreadable
  tokenizer artifacts before execute.
- `deployment_recommendation` never selects a runtime in plan mode. It can name
  a runtime/backend/model-format performance candidate only when every
  identity-aligned row completes execute and supplies observed memory fit,
  configured-workload TPOT/throughput, matching observed token counts, and
  matching finish reason. Batch, concurrency, thread and memory-budget
  configuration are part of comparison identity. Multi-runtime ranking also
  fails closed when thread-control modes differ or any runtime-managed thread
  count remains unverified. That candidate does not prove quality, power,
  thermal behavior, production fitness, or release readiness.
- Adapter generation return paths use one shared collector for process-lifetime peak
  RSS, maximum PSS across explicit adapter checkpoints, and exact mappings of
  the configured main model/PTE path. The collector mirrors Linux maps'
  newline-only path rendering and requires the current file's device and inode;
  an old mapping at a reused pathname cannot become current evidence, and a
  live filename literally ending in ` (deleted)` is not rejected by suffix
  text alone. Both proc readers bound consumed line content and delimiters.
  These are not per-request RSS deltas,
  continuous PSS peaks, total runtime mappings, KV allocation, or workspace.
- The collector finalizes metrics into the returned value before move/copy and
  does not rely on optional NRVO. llama.cpp context and sampler remain alive
  until exception-path final sampling completes.
- Execute mode never degrades into a successful plan. Missing linked adapters
  produce `blocked_before_execute`, null metrics, `executed=false`, and a
  non-zero process result.
- Execute mode requires an explicit `--output` file because pinned third-party
  runtimes may write generated pieces or diagnostics to process stdout. Plan
  mode may still emit JSON to stdout because it does not invoke those runtimes.
- A nonempty output must not alias the configuration, model or configured
  tokenizer input. The shared path identity owner checks existing filesystem
  equivalence plus normalized weak-canonical/absolute fallback identity. All
  runtime input paths are preflighted before adapter execution, and a collision
  bypasses malformed-config fallback so no artifact write follows. Distinct
  external model and tokenizer paths remain supported. Without the structured
  parser, nonempty output fails closed because that separation cannot be
  established; plan-mode stdout remains available.
- Model and configured tokenizer inputs now receive one request-owned metadata
  snapshot during all-row preflight and are reconciled immediately before load
  and after the complete runtime attempt. Linux/Android use device, inode, size,
  mtime and ctime from `stat`; fallback platforms use canonical path, size and
  mtime. Drift or an unavailable execute-window snapshot blocks execute evidence,
  invalidates generated-text comparison and prevents runtime selection. Raw
  identity is never emitted, and stable metadata is not a content hash,
  signature or immutable-file proof. Resource planning consumes the same
  baseline size instead of probing a second file-size owner.
- `executed=true` records that every configured lifecycle ran; `execute_ready`
  additionally requires an empty blocker set. Configuration-identity drift,
  observed token/finish mismatch, missing ranking metrics, resource-budget
  failure, callback inconsistency, or lifecycle failure cannot be hidden by an
  all-row executed count.
- Execute mode also rejects target-platform/build-architecture mismatch and
  preflights the PTE tokenizer separately. Prompt text is never emitted;
  exact prompt equality is checked transiently while parsing, while the
  deterministic non-security fingerprint remains display metadata only.
  Sampling identity is compared separately.
- Schema-v2 request fields pass through the same validator used by both
  adapters, including the rule that maximum output cannot exceed context
  capacity. This is request-contract readiness, not tokenized prompt fit.
- Schema v3 adds the serial measurement protocol. Schema v4 additionally
  requires `maximum_generation_duration_ms`; older v1-v3 configurations use
  the canonical 300000 ms cooperative-deadline default. The deadline starts
  inside each adapter after request validation and cancellation-window setup.
  Expiry publishes the same atomic cancel state and returns
  `generation_deadline_exceeded` when the runtime cooperates. It is not a hard
  thread kill, process timeout, or proof that an unresponsive third-party call
  can be terminated.
- Runtime version, accepted model format, backend and cross-runtime logical
  model/checkpoint/tokenizer/generation identity are checked before a row can
  be treated as comparable. Identity alignment permits quality comparison, not
  token/logit equality or performance equivalence.
- `comparison_identity.quality_observation` now retains bounded primary callback text only in memory,
  compares exact bytes after every identity-aligned row completes execute, then
  clears the retained text before rendering. The artifact emits row byte counts,
  capture status and a non-cryptographic FNV-1a diagnostic fingerprint, never
  generated text. Capture is bounded to 1 MiB per row, 8 MiB per matrix and
  64 runtime rows; exceeding either text budget only makes comparison unavailable.
  If any row lacks a usable capture, every row is classified unavailable for
  that comparison instead of leaving partially classified observations. Empty
  generated callback text is not accepted as exact-match evidence.
  `observed_exact_text_match` means one deterministic request
  produced the same bounded callback bytes; it is not a model-quality, token-id,
  logit or quantization-equivalence result. `observed_text_mismatch` is an
  observation and does not block an otherwise valid performance candidate. Its
  first differing byte offset is emitted for diagnosis without exposing either
  runtime's text.
- Cross-runtime identity also requires equal configured layer count, KV head
  count, head dimension and KV dtype. A shared model label cannot hide different
  KV geometry or resource-estimation inputs.
- Plan mode estimates aggregate KV/resource capacity for multiple concurrent
  sessions. Execute supports 1 to 64 independent in-process runtime instances;
  larger plans remain capacity-only and are blocked before execute. Multi-session
  deployment ranking uses concurrent median TPOT and aggregate generated-token
  throughput, while single-session rows retain serial measurement metrics.
  Explicit thread-control runtimes divide the target physical-core budget across
  sessions. Buffered or unsupported-mmap model residency scales with the session
  count; a supported requested mmap uses one analytical shareable model-residency
  amount. Neither estimate proves allocator/page sharing or observed memory use.
- ExecuTorch execute evidence distinguishes external cancellation from callback
  cancellation, requires bounded Stats counts to fit request context/output,
  stops and fails before callbacks can exceed the requested output limit,
  and normalizes pinned v1.2.0's callback-visible prefill token against the
  post-first-token `Stats::num_generated_tokens` count. The normalized total
  must equal callback count. Timeline validation follows the pinned write order
  of prompt-start/first-token/prompt-end/inference-end. TPOT starts at first
  token, while decode throughput uses only the post-first-token decode count.
- Both adapters establish a new generation's cooperative cancellation state
  before validation/setup. ExecuTorch checks that state again before forwarding
  each token, so a cross-thread cancel after the generation linearization point
  is not erased by later validation/setup. Target-runtime stop latency remains
  unverified until dependency-backed execution.
- ExecuTorch public `cancel()` only publishes the InferLab atomic flag. The
  generation callback checks it again after user token delivery and invokes the
  pinned runner's non-atomic stop state from the generation thread, not from an
  arbitrary caller thread.
- ExecuTorch success below the requested output limit is reported as
  `runtime_stop_unclassified`; the pinned API does not expose whether EOS or an
  effective PTE context cap stopped generation. Only reaching the request uses
  `length`, and cross-runtime finish alignment retains this uncertainty.
- The Matrix records an unclassified-stop run as lifecycle execution but adds
  `runtime_stop_reason_unclassified`; it cannot produce `execute_ready` or a
  deployment performance candidate until the cause is observable.
- Execute readiness now includes a separate post-reset public `cancel()` probe:
  the probe invokes cancel during an active token callback, requires a cancelled
  result with coherent callback counts, then requires another successful reset.
  Probe timings do not replace the primary generation timings; only process
  peak/mapping observations are reconciled. A nonempty lifecycle label alone
  cannot satisfy `executed=true`.
- Deployment candidates and the lowest-TPOT/highest-throughput tie-break winner
  are derived and validated through the same resource-plan helpers, so a stale
  but otherwise eligible selected row fails projection validation.

## Responsibility Boundary

| Surface | Canonical responsibility |
|---|---|
| `native_edge_runtime_matrix.json` | Embedded C++ runtime capability, lifecycle, resources, and execute provenance |
| `native_edge_compiler_artifact_matrix.json` | Toolchain, ABI, library/model layout, and packaging readiness |
| `llama_cpp_matrix.json` | External llama.cpp binary command matrix and product evidence |
| `android_edge_benchmark.json` | ADB/device execution and Android resource collection |

The center catalog owns artifact identity, refresh command, required fields,
claim boundary, report grouping, freshness, Summary, Prometheus, Final Report,
Evidence Bundle, and Trace Correlation discovery.

The Android ADB producer now structures device model, SoC/board, API, ABI,
CPU/cpufreq, thermal, battery counters and deployment footprint in the existing
artifact. Required API-24+/arm64-v8a identity is collected before model push or
timing, and execute intent is
separate from actual scenario attempts. Its before/after snapshots bracket the
external llama.cpp thread matrix, and the published duration encloses that full
observation window. Timing/backend evidence comes only from stderr diagnostics;
stdout generation text is neither parsed nor retained. Output/report aliases
with each other or the host model are rejected before ADB work. Signed
battery-counter change is not workload-exclusive energy, runtime log backend
names are not delegation proof, and this standalone CLI route does not exercise
the JNI/AAR lifecycle.

Requested simpleperf output is projected from the same stderr snapshot into a
bounded canonical event map, not-counted list, issue count and total test time.
Duplicates fail closed and incomplete requested evidence makes the scenario
partial. Counter values cover the external process wrapper and do not establish
per-operator attribution, backend delegation or frequency stability.

The external Android runner and compatibility parity now share one bounded
binary-pipe/process-group execution helper. Android probe streams are limited to
64 KiB and push/scenario streams to 16 MiB; timeout or overflow terminates the
process group and blocks scenario success. Compatibility keeps its existing
authorization environment and capture artifact contract. These paths remain
unverified until their respective execute workflows run.

The INT4 NEON fast path preserves the scalar owner's multiplication range by
promoting dequantized float lanes and activation lanes before FP64 multiply and
reduction. Matrix projection validation also requires the exact unique
INT4/RMSNorm/RoPE capability inventory and exact agreement with the shared ISA
probe. These are source-contract properties until AArch64 differential evidence
is collected.

The shared runtime-result contract rejects present non-positive TPOT/throughput
and zero RSS/PSS before those values can enter memory-fit or performance
ranking. Zero mapped-model bytes remains valid as an explicit no-current-mapping
observation. Ready finish reasons are restricted to `length`,
`end_of_generation`, and the explicitly blocked
`runtime_stop_unclassified` state.

If a validated primary generation is followed by a reset or cancellation-probe
exception, its partial observations retain their actual provenance and specific
error category. The row remains `executed=false` and blocked. Any present metric
paired with an `unavailable_*` provenance fails matrix validation, preventing a
contradictory artifact from being emitted.

The llama.cpp adapter bounds prompt bytes before its signed 32-bit API
conversion and validates both inline and dynamically retried token-piece return
lengths against the actual supplied buffer before constructing token text. This
is source-level memory-safety hardening, not linked-runtime execution evidence.

Deployment recommendation validation reuses the complete canonical derivation
and reconciles status, candidates, observation gaps, winner identity, thread
projection, mmap strategy, and criterion. A stale projection cannot pass merely
because its selected row still matches the TPOT tie-break. The recommendation
remains a configured-artifact performance candidate, not a quality or release
decision.

Sampling configuration is range-checked at JSON double precision before float
narrowing. The parser rejects `temperature < 0`, `top_p` outside `(0, 1]`, and
nonzero values that underflow to float zero, preventing rounding from silently
changing sampling semantics before the shared request contract runs.

Process RSS and sampled PSS remain maxima over explicit checkpoints. Exact
mapped-model bytes instead use the latest checkpoint: null means the latest
`/proc` read was unavailable, zero means no exact current-model mapping, and a
positive value is the latest mapping total. The later cancel probe replaces this
point-in-time value; a legitimate mapping change is not treated as drift.

Linux PSS checkpoints prefer an exact-one `Pss:` value from
`/proc/self/smaps_rollup`. If that source is unavailable, the same bounded
collector checked-sums every mapping `Pss:` line in `/proc/self/smaps`.
Duplicate or malformed rollup values, malformed mapping values, overflow,
empty input and read-budget exhaustion discard the entire PSS sample rather
than retaining a partial working-set value.

The same process-observation owner now takes bounded Linux thermal-zone
snapshots after successful load and before unload. The row exposes the maximum
readable temperature, readable sensor count, status and provenance for each
checkpoint. Missing, malformed, inaccessible or limit-truncated sysfs input
stays null/unavailable; these two snapshots neither affect runtime selection nor
prove throttling, sustained performance, power, energy or device fitness.

The thermal directory bound distinguishes truncation from exact completion: a
snapshot is marked limit-reached only when 256 thermal-zone entries have been
inspected and the directory iterator still has unvisited entries. Exactly 256
entries followed by end-of-directory retain their complete observation.

The same owner also implements a bounded Linux powercap lifecycle-energy
observation using the kernel `energy_uj`, `max_energy_range_uj` and
`max_power_range_uw` interface.
It selects root power zones so parent and child counters are never added
together, samples immediately before runtime load and after unload, and emits a
delta only when the two snapshots have the same nonempty zone identities and
ranges. Missing or malformed sysfs input, inventory truncation, zone/range
drift, a missing power bound, counter decrease/reset, a time-and-power bound
that cannot rule out hidden wrap, an impossible counter rate and checked-sum
overflow remain explicit unavailable states. Raw sysfs identities and capture
timestamps are request-local and are not rendered.

This optional observation does not affect lifecycle success, deployment
ranking or release status. It covers the whole system represented by the root
power zones during the complete InferLab runtime attempt; it is not
process/model/inference-exclusive energy, instantaneous power, battery energy,
or a production-efficiency result.

The three backend-aware kernel facades now treat execution metadata as a
success commit. A caller-provided `EdgeKernelExecution` record is cleared at
entry, scalar/NEON computation runs, and requested/selected/fallback/reason are
published only after a successful output. Invalid inputs, unavailable explicit
NEON requests and numerical failures cannot retain a prior record or expose a
precommitted backend selection as completed execution.

Both adapters now project first-token latency as soon as that timestamp is
available and at least one token has been delivered to the callback. A
successful public-cancel probe may therefore contribute the
second generation's `warm_ttft_ms` even though it intentionally stops after the
first callback. The probe still cannot overwrite the primary request's cold
TTFT, TPOT, throughput, token counts or finish reason; missing cancellation
Stats remain null.

The shared runtime contract now also validates ready and cancelled results
against the originating request before the matrix consumes them. Prompt and
generated token counts must fit the context and output limits using checked
subtraction; `length` must exactly reach the requested output count, while an
immediate end-of-generation may legitimately emit no output token. Failed
results retain their diagnostic counts and error category.

The llama.cpp adapter also bounds third-party sizing returns before allocation.
Prompt tokenization cannot allocate beyond the validated
`context_tokens - maximum_output_tokens` budget, and a dynamically retried
token-piece buffer is capped at 1 MiB. Existing signed-return and actual-buffer
capacity checks remain in force. ExecuTorch uses the same runtime-contract
limit before forwarding a callback token string and reports a distinct failure
if the third-party runner exceeds it.

The public generation request now also owns a separate 1 MiB prompt-byte
boundary. It is enforced by the shared request validator before either adapter
tokenizes or forwards input, so direct runtime callers cannot bypass the matrix
configuration file-size bound. This is a host-memory and third-party-input
safety limit; it is not a token-context allowance, model capability, quality or
performance claim.

Cancellation-probe resource observations are now merged only after the public
cancel status, callback sequence/count and generated-token count all reconcile,
and only when the probe metric provenance exactly matches the primary
generation. A failed or provenance-drifted probe cannot inflate primary
RSS/PSS, replace the latest mapped-model checkpoint or supply warm TTFT.
Runtime-owned KV/workspace fields remain null until a pinned runtime exposes a
truthful allocation API; serialized state size and process-memory deltas are not
reported as allocated KV or workspace bytes.

Generation-result validation now applies TTFT/token consistency to every
status, including failures: cold or warm TTFT requires at least one token that
the adapter actually delivered to its callback. ExecuTorch records that
callback count immediately after the runner returns and projects Stats timing
only when it is nonzero. Failed results may keep partial diagnostic counts and
metrics, but non-ready/non-cancelled results cannot carry a completion finish
reason.

## Current State

The unified contract, kernel dispatcher, configuration, artifact model/view,
CLI, CMake switches, Catalog entry, benchmark matrix entry, schema, and report
and both optional C++ adapters are implemented at source level. The llama.cpp
and ExecuTorch dependency switches still default to off. An adapter is reported
as linked only after its pinned source/SDK checks and CMake target checks pass;
merely enabling an option is not sufficient.

The lifecycle is fail-closed for replacement loads, model identity includes
checkpoint and conversion provenance, and cancel/unload preserve their
documented no-throw boundary. These are source-level contract properties, not
evidence that either external runtime has compiled or executed here.

The source owner also implements bounded independent-runtime concurrency,
session-aware resource/thread planning and configured-workload ranking. The
Android JNI/AAR producer and verifier source exist separately. None of those
source states proves linked runtime, AAR, device, performance or energy
evidence.

No build, test, benchmark, native binary, product runtime, Summary, Prometheus,
or Final Report command was run for this implementation: `not run per user
request`.

## Remaining Architecture Work

1. Add runtime-owned KV/workspace observations only when pinned APIs expose
   truthful allocation values; do not infer them from serialized size or
   process-memory deltas.
2. A hard wall-clock bound would require subprocess isolation or a pinned
   runtime API that guarantees interruptible calls. The in-process cooperative
   deadline scheduler intentionally does not detach or force-kill a worker
   using live runtime state.

## Remaining Target Work

1. Build the pinned llama.cpp adapter against b10173/e9fa078 and run its GGUF
   serial and multi-session lifecycle on AArch64.
2. Build/install ExecuTorch v1.2.0 with LLM Runner/XNNPACK and run its PTE plus
   tokenizer serial and multi-session lifecycle on AArch64.
3. Run scalar/NEON differential correctness on AArch64.
4. Validate start-gate concurrency counts, configured-workload ranking, Linux
   PSS/mapped-file, bounded before/after thermal-zone collectors and the
   implemented powercap lifecycle-energy collector; retain target-specific
   runtime-owned KV/workspace observations separately.
5. Run the implemented bounded GGUF/PTE generated-text comparison for the same
   logical model and deterministic request, then review mismatch semantics
   without promoting a match to a model-quality claim.
6. Resolve the existing Android module's AGP/SDK/NDK dependencies, assemble and
   inspect its AAR, then exercise JNI lifecycle/cancel/release on a device.

Until those steps produce current artifacts, no product-execute, speedup,
quality-equivalence, thermal, device-calibrated energy/power, or release claim
is valid.
