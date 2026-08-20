# InferLab Android Edge Runtime JNI

This module is the Android application bridge for the canonical C++
`IEdgeRuntime` contract. It does not contain a second llama.cpp or ExecuTorch
adapter.

## Build Contract

The checked-in Android library module pins Android Gradle Plugin 8.7.3 and
expects Gradle 8.9 on JDK 17. It uses compile SDK 35, minimum API 24, Android
NDK `27.0.12077973`, CMake 3.22.1 and the `arm64-v8a` ABI. Gradle only builds
the existing root-CMake `inferlab_edge_runtime_jni` target.

No Gradle wrapper binary is committed. On a prepared Android build machine,
use an installed Gradle 8.9 distribution:

```bash
gradle --no-daemon assembleDebug
```

A dependency-free debug build is capability-only: the Java/native bridge can
report registrations, but `create` fails until at least one exact-pinned runtime
is linked. Gradle never downloads llama.cpp or ExecuTorch.

For a release llama.cpp AAR, use the canonical post-assembly verifier. It
depends on the release bundle task and rejects an AAR whose ABI, JNI classes,
consumer rules, generated metadata or license payload does not match the build
contract:

```bash
gradle --no-daemon verifyInferLabReleaseAar \
  -PinferlabLlamaCppSourceDir=/absolute/path/to/llama.cpp-e9fa078 \
  -PinferlabProjectLicenseFile=/absolute/path/to/INFERLAB-LICENSE \
  -PinferlabThirdPartyLicenseReviewFile=/absolute/path/to/license-review.txt
```

For ExecuTorch, provide all three SDK identity inputs together:

```bash
gradle --no-daemon verifyInferLabReleaseAar \
  -PinferlabExecutorchSourceDir=/absolute/path/to/executorch-0b0e2c5 \
  -PinferlabExecutorchInstallDir=/absolute/path/to/executorch-sdk \
  -PinferlabExecutorchBuildRevisionFile=/absolute/path/to/build-revision.txt \
  -PinferlabProjectLicenseFile=/absolute/path/to/INFERLAB-LICENSE \
  -PinferlabThirdPartyLicenseReviewFile=/absolute/path/to/license-review.txt
```

Both runtime property groups may be supplied to one build. Release packaging
fails closed when neither runtime is requested, the InferLab project license is
missing, or the external third-party license review input is missing. Release
also sets `INFERLAB_REQUIRE_EDGE_RUNTIME_LINKED=ON`, so a requested runtime that
fails its existing CMake source revision, SDK or target-link gate cannot produce
a capability-only release AAR. The resulting AAR path is
`build/outputs/aar/inferlab-edge-runtime-jni-release.aar`.

Direct NDK/CMake configuration remains available for native integration without
an AAR:

```bash
cmake -B build-android-edge \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/android-ndk/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-24 \
  -DINFERLAB_ENABLE_ANDROID_JNI=ON
```

Enable exactly pinned llama.cpp or ExecuTorch options in the same configure
request. The JNI target does not convert a dependency-unavailable runtime into
an executable runtime.

The resulting library is `libinferlab_edge_runtime_jni.so`.

## Packaging Contract

- `src/main/AndroidManifest.xml` declares the current API 24 minimum.
- `consumer-rules.pro` preserves the exact wrapper, nested callback contract,
  callback implementations and native method names required by short JNI
  symbol binding. The Android library publishes it through
  `consumerProguardFiles`.
- Java and native both declare bridge API version 1. Class initialization
  compares them immediately after loading the `.so` and fails before session
  creation when packages are mixed.
- `prepareInferLabEdgePackagingAssets` generates build-specific ABI and license
  manifests under `assets/inferlab/`. It copies the configured InferLab license,
  exact runtime checkout top-level license text and external license-review
  record without embedding host paths. The generated `package_id`, Android
  namespace and required JNI class path share the canonical
  `com.inferlab.edge` identity.
- `validateInferLabReleasePackaging` requires a functional runtime request and
  complete release license inputs. These packaging checks do not replace the
  CMake exact-revision/link gates or legal review.
- `verifyInferLabReleaseAar` is the single post-assembly verifier. It rejects
  unsafe or duplicate archive paths, unexpected ABIs, missing JNI classes or
  consumer rules, model payloads, inconsistent ABI/runtime metadata, incomplete
  runtime license payloads and configured host paths in generated metadata.
  ABI and license manifests must contain the exact canonical field sets and
  shared identity/policy/claim declarations; extra claim-like metadata is not
  accepted. Object-scoped duplicate JSON keys are rejected after escape
  decoding, so an alternate spelling such as a Unicode-escaped key cannot hide
  a second claim. Packaged license and review files must be nonblank UTF-8 text
  with no disallowed control characters.
  Native entries must also be little-endian ELF64 AArch64 shared objects; an
  `arm64-v8a` directory name cannot substitute for the binary header.
  Libraries must be direct `jni/arm64-v8a/lib*.so` entries, and verification
  streams every non-directory AAR payload to reconcile its declared length and
  CRC32 before any specialized check. Nested nonempty `classes.jar` entries
  separately reconcile expanded length and CRC32; ELF inspection retains only
  the first bounded header after the complete native payload passes integrity.
  The verifier proves only the inspected package contract; it does not prove
  R8 application behavior, native loading, model generation or device support.
- `buildSrc/.../AarContractVerifier.kt` owns all archive, ELF, metadata and
  license decisions. `gradle/aar-verification.gradle.kts` is a thin task
  adapter only. The build logic uses the Kotlin DSL and Groovy JSON support
  distributed with Gradle; it is not part of the AAR runtime classpath.
- `buildSrc/.../AarPackagingAssets.kt` separately owns generated ABI/license
  assets and release-input checks. Producer and verifier consume the same typed
  `AarContractSettings`; the Android build script only collects properties and
  wires AGP, CMake and task dependencies.
  License files are copied through a 4 MiB bounded stream with source-length
  reconciliation and same-directory atomic publication; failed copies remove
  temporary or published partial output. Generated-root cleanup does not follow
  symlinks and rejects canonical overlap with runtime source, project-license or
  third-party-review inputs before deleting anything.

The typed build-logic values are serializable and task inputs include package,
ABI, SDK and native-library identity. This is source preparation for Gradle
configuration-cache validation, not evidence that configuration cache works;
that claim requires a target-machine Gradle run.

Increment the bridge API version whenever a native method descriptor, callback
descriptor, byte/result wire contract or lifecycle meaning changes
incompatibly. The Gradle module is implemented in source, but no AAR has been
assembled, inspected, loaded or executed in the current environment.

## Lifecycle

- `create`: binds one linked canonical runtime factory.
- `load`, `generate`, `generateAsync`, `reset`, `unload`: admitted one at a
  time by the Java facade and serialized per native session.
- `cancel`: may run from another Java thread or from the token callback.
- `close`: removes the public handle, publishes cancellation and defers native
  destruction until active calls return.

A token callback may call `cancel` or `close` on its active session. Re-entering
`load`, `generate`, `generateAsync`, `reset` or `unload` for that same session
fails closed instead of waiting on the generation lifecycle lock. The same
admission rule applies across application threads: an async task holds the
lifecycle claim while queued and running, so model replacement, reset or
unload cannot slip between request snapshotting and native execution.

`generate` is synchronous. The Android application owns its executor and must
call it off the UI thread; the JNI bridge does not introduce a second thread
pool or scheduling policy.

`generateAsync` snapshots the mutable request before submitting it to a
caller-owned, non-inline executor. Its `GenerationTask` exposes a read-only
completion stage and `requestCancel()`. Queued cancellation skips JNI;
in-flight cancellation delegates to the canonical native cancel path and is
also observed before the next token is forwarded. Applications still own
executor shutdown and UI-thread callback marshalling.

One runtime admits exactly one non-cancel lifecycle operation at a time. This
prevents a queued task's session-level cancellation from stopping a different
active request and prevents state-changing calls from reordering around queued
generation. `close()` first invalidates the Java handle, then marks the
registered task cancelled and destroys the saved native handle; a completion
callback therefore cannot enqueue work during close. Task ownership is released
before completion callbacks run so a callback may schedule the next lifecycle
operation during normal completion.
Task phase is explicitly `queued`, `running`, `native_returned` or `finished`;
only the exact `queued -> running` transition may enter JNI and only `running`
can call session-level native cancel. A rejecting or non-conforming executor
cannot run a task after its claim has been released, and a late cancel on an
old task cannot reach the next admitted generation.

Prompt and token text use UTF-8 byte arrays across JNI. Operation and generation
results are returned as UTF-8 JSON strings, but generated token bytes are only
delivered to the callback and are not stored in those results.

The native bridge derives the model path label through InferLab's shared path
redaction helper. A Java caller cannot provide an absolute display path.
Callback attachment, allocation and byte-copy failures are reported as JNI
transport failures; returning `false` from `onToken` remains an intentional
cooperative stop.

This source contract has not been built or executed with an Android NDK or
device in the current environment.
