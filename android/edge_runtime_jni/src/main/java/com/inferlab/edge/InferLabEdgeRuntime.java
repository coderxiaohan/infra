package com.inferlab.edge;

import java.nio.charset.StandardCharsets;
import java.util.Objects;
import java.util.concurrent.CancellationException;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.CompletionStage;
import java.util.concurrent.Executor;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicReference;

/** Android wrapper for InferLab's canonical C++ edge runtime lifecycle. */
public final class InferLabEdgeRuntime implements AutoCloseable {
    private static final int BRIDGE_API_VERSION = 1;

    static {
        System.loadLibrary("inferlab_edge_runtime_jni");
        int nativeVersion = nativeBridgeApiVersion();
        if (nativeVersion != BRIDGE_API_VERSION) {
            throw new UnsatisfiedLinkError("InferLab edge JNI API mismatch: Java="
                    + BRIDGE_API_VERSION + ", native=" + nativeVersion);
        }
    }

    public interface TokenCallback {
        boolean onToken(long tokenIndex, byte[] utf8Token);
    }

    public static final class ModelDescriptor {
        public String logicalModelId;
        public String modelFormat;
        public String quantization;
        public String tokenizerId;
        public String checkpointRevision;
        public String conversionProvenance;
        public String modelPath;
        public String tokenizerPath = "";
        public long contextLimit;
        public boolean preferMmap;
    }

    public static final class GenerationRequest {
        public byte[] utf8Prompt;
        public long maximumOutputTokens;
        public long contextTokens;
        public long batchSize = 1;
        public long threadCount = 1;
        public long seed;
        public float temperature;
        public float topP = 1.0f;

        public void setPrompt(String prompt) {
            utf8Prompt = Objects.requireNonNull(prompt, "prompt")
                    .getBytes(StandardCharsets.UTF_8);
        }
    }

    /** One caller-scheduled generation and its cooperative cancellation state. */
    public static final class GenerationTask {
        private static final int PHASE_QUEUED = 0;
        private static final int PHASE_RUNNING = 1;
        private static final int PHASE_NATIVE_RETURNED = 2;
        private static final int PHASE_FINISHED = 3;

        private final InferLabEdgeRuntime runtime;
        private final CompletableFuture<String> completion = new CompletableFuture<>();
        private final CompletionStage<String> completionView =
                completion.thenApply(result -> result);
        private final AtomicBoolean cancellationRequested = new AtomicBoolean();
        private final AtomicBoolean lifecycleClaimReleased = new AtomicBoolean();
        private final AtomicReference<RuntimeException> cancellationFailure =
                new AtomicReference<>();
        private final Object phaseLock = new Object();
        private int phase = PHASE_QUEUED;

        private GenerationTask(InferLabEdgeRuntime runtime) {
            this.runtime = runtime;
        }

        public CompletionStage<String> completion() {
            return completionView;
        }

        public boolean isDone() {
            return completion.isDone();
        }

        public boolean isCancellationRequested() {
            return cancellationRequested.get();
        }

        /**
         * Prevents queued work or requests native cooperative cancellation after start.
         * A running task completes with the canonical native generation result.
         */
        public boolean requestCancel() {
            return requestCancel(true);
        }

        private boolean requestCancelForClose() {
            return requestCancel(false);
        }

        private boolean requestCancel(boolean invokeNativeCancel) {
            boolean running;
            synchronized (phaseLock) {
                if (completion.isDone() || cancellationRequested.get() ||
                        phase >= PHASE_NATIVE_RETURNED) {
                    return false;
                }
                cancellationRequested.set(true);
                running = phase == PHASE_RUNNING;
            }
            if (!running) {
                markFinished();
                releaseLifecycleClaim();
                completion.completeExceptionally(new CancellationException(
                        "edge generation cancelled before native execution"));
                return true;
            }
            if (invokeNativeCancel) {
                try {
                    runtime.cancel();
                } catch (RuntimeException error) {
                    cancellationFailure.compareAndSet(null, error);
                }
            }
            return true;
        }

        private boolean beginNativeCall() {
            synchronized (phaseLock) {
                if (cancellationRequested.get() || phase != PHASE_QUEUED) {
                    return false;
                }
                phase = PHASE_RUNNING;
                return true;
            }
        }

        private void complete(String result) {
            completion.complete(result);
        }

        private void fail(Throwable error) {
            completion.completeExceptionally(error);
        }

        private RuntimeException cancellationError() {
            return cancellationFailure.get();
        }

        private void markNativeReturned() {
            synchronized (phaseLock) {
                phase = PHASE_NATIVE_RETURNED;
            }
        }

        private void markFinished() {
            synchronized (phaseLock) {
                phase = PHASE_FINISHED;
            }
        }

        private TokenCallback cancellationAwareCallback(TokenCallback delegate) {
            return (tokenIndex, utf8Token) ->
                    !cancellationRequested.get() &&
                    (delegate == null || delegate.onToken(tokenIndex, utf8Token));
        }

        private void releaseLifecycleClaim() {
            if (lifecycleClaimReleased.compareAndSet(false, true)) {
                runtime.releaseAsyncLifecycleClaim(this);
            }
        }
    }

    private final AtomicLong nativeHandle;
    private final AtomicBoolean lifecycleClaimed = new AtomicBoolean();
    private final AtomicReference<GenerationTask> asyncGenerationTask =
            new AtomicReference<>();

    private InferLabEdgeRuntime(long nativeHandle) {
        this.nativeHandle = new AtomicLong(nativeHandle);
    }

    public static InferLabEdgeRuntime create(String runtimeId) {
        long handle = nativeCreate(utf8(runtimeId, "runtimeId"));
        if (handle == 0) {
            throw new IllegalStateException("native runtime returned an invalid handle");
        }
        return new InferLabEdgeRuntime(handle);
    }

    public static String capabilities(String runtimeId) {
        return decode(nativeCapabilities(utf8(runtimeId, "runtimeId")));
    }

    public static int bridgeApiVersion() {
        return BRIDGE_API_VERSION;
    }

    public String load(ModelDescriptor model) {
        Objects.requireNonNull(model, "model");
        claimLifecycle("load");
        try {
            return decode(nativeLoad(requireHandle(),
                    utf8(model.logicalModelId, "logicalModelId"),
                    utf8(model.modelFormat, "modelFormat"),
                    utf8(model.quantization, "quantization"),
                    utf8(model.tokenizerId, "tokenizerId"),
                    utf8(model.checkpointRevision, "checkpointRevision"),
                    utf8(model.conversionProvenance, "conversionProvenance"),
                    utf8(model.modelPath, "modelPath"),
                    optionalUtf8(model.tokenizerPath),
                    model.contextLimit, model.preferMmap));
        } finally {
            releaseLifecycleClaim();
        }
    }

    /** Synchronous native generation; Android applications must call this off the UI thread. */
    public String generate(GenerationRequest request, TokenCallback callback) {
        claimLifecycle("generate");
        try {
            return generateNative(request, callback);
        } finally {
            releaseLifecycleClaim();
        }
    }

    private String generateNative(GenerationRequest request, TokenCallback callback) {
        Objects.requireNonNull(request, "request");
        return decode(nativeGenerate(requireHandle(),
                Objects.requireNonNull(request.utf8Prompt, "utf8Prompt"),
                request.maximumOutputTokens, request.contextTokens,
                request.batchSize, request.threadCount, request.seed,
                request.temperature, request.topP, callback));
    }

    /**
     * Schedules generation on a caller-owned executor without creating an InferLab pool.
     * The executor must not run work inline on the UI thread. Token callbacks use the
     * runtime callback thread; applications own UI marshalling and task cancellation.
     */
    public GenerationTask generateAsync(Executor executor,
            GenerationRequest request, TokenCallback callback) {
        Objects.requireNonNull(executor, "executor");
        requireHandle();
        GenerationRequest snapshot = snapshotRequest(request);
        claimLifecycle("generateAsync");
        GenerationTask task;
        TokenCallback taskCallback;
        try {
            task = new GenerationTask(this);
            taskCallback = task.cancellationAwareCallback(callback);
        } catch (RuntimeException | Error error) {
            releaseLifecycleClaim();
            throw error;
        }
        if (!asyncGenerationTask.compareAndSet(null, task)) {
            releaseLifecycleClaim();
            throw new IllegalStateException(
                    "async generation ownership is inconsistent");
        }
        if (nativeHandle.get() == 0) {
            task.requestCancel();
            return task;
        }
        try {
            executor.execute(() -> {
                if (!task.beginNativeCall()) {
                    return;
                }
                try {
                    String result = generateNative(snapshot, taskCallback);
                    task.markNativeReturned();
                    task.releaseLifecycleClaim();
                    RuntimeException cancellationError = task.cancellationError();
                    if (cancellationError == null) {
                        task.complete(result);
                    } else {
                        task.fail(cancellationError);
                    }
                    task.markFinished();
                } catch (Throwable error) {
                    task.markNativeReturned();
                    task.releaseLifecycleClaim();
                    task.fail(error);
                    task.markFinished();
                }
            });
        } catch (RuntimeException error) {
            task.markFinished();
            task.releaseLifecycleClaim();
            task.fail(error);
        } catch (Error error) {
            task.markFinished();
            task.releaseLifecycleClaim();
            throw error;
        }
        return task;
    }

    public void cancel() {
        nativeCancel(requireHandle());
    }

    public String reset() {
        claimLifecycle("reset");
        try {
            return decode(nativeReset(requireHandle()));
        } finally {
            releaseLifecycleClaim();
        }
    }

    public void unload() {
        claimLifecycle("unload");
        try {
            nativeUnload(requireHandle());
        } finally {
            releaseLifecycleClaim();
        }
    }

    @Override
    public void close() {
        long handle = nativeHandle.getAndSet(0);
        GenerationTask task = asyncGenerationTask.get();
        if (task != null) {
            task.requestCancelForClose();
        }
        if (handle != 0) {
            nativeDestroy(handle);
        }
    }

    private long requireHandle() {
        long handle = nativeHandle.get();
        if (handle == 0) {
            throw new IllegalStateException("edge runtime is closed");
        }
        return handle;
    }

    private void claimLifecycle(String operation) {
        Objects.requireNonNull(operation, "operation");
        if (!lifecycleClaimed.compareAndSet(false, true)) {
            throw new IllegalStateException(
                    "cannot start " + operation +
                    "; another edge lifecycle operation is already scheduled or running");
        }
    }

    private void releaseLifecycleClaim() {
        lifecycleClaimed.set(false);
    }

    private void releaseAsyncLifecycleClaim(GenerationTask owner) {
        if (asyncGenerationTask.compareAndSet(owner, null)) {
            releaseLifecycleClaim();
        }
    }

    private static byte[] utf8(String value, String field) {
        return Objects.requireNonNull(value, field).getBytes(StandardCharsets.UTF_8);
    }

    private static byte[] optionalUtf8(String value) {
        return value == null ? new byte[0] : value.getBytes(StandardCharsets.UTF_8);
    }

    private static GenerationRequest snapshotRequest(GenerationRequest request) {
        Objects.requireNonNull(request, "request");
        GenerationRequest snapshot = new GenerationRequest();
        snapshot.utf8Prompt = Objects.requireNonNull(
                request.utf8Prompt, "utf8Prompt").clone();
        snapshot.maximumOutputTokens = request.maximumOutputTokens;
        snapshot.contextTokens = request.contextTokens;
        snapshot.batchSize = request.batchSize;
        snapshot.threadCount = request.threadCount;
        snapshot.seed = request.seed;
        snapshot.temperature = request.temperature;
        snapshot.topP = request.topP;
        return snapshot;
    }

    private static String decode(byte[] value) {
        return new String(Objects.requireNonNull(value, "native result"),
                StandardCharsets.UTF_8);
    }

    private static native int nativeBridgeApiVersion();
    private static native byte[] nativeCapabilities(byte[] runtimeId);
    private static native long nativeCreate(byte[] runtimeId);
    private static native void nativeDestroy(long handle);
    private static native byte[] nativeLoad(long handle, byte[] logicalModelId,
            byte[] modelFormat, byte[] quantization, byte[] tokenizerId,
            byte[] checkpointRevision, byte[] conversionProvenance,
            byte[] modelPath, byte[] tokenizerPath, long contextLimit,
            boolean preferMmap);
    private static native byte[] nativeGenerate(long handle, byte[] utf8Prompt,
            long maximumOutputTokens, long contextTokens, long batchSize,
            long threadCount, long seed, float temperature, float topP,
            TokenCallback callback);
    private static native void nativeCancel(long handle);
    private static native byte[] nativeReset(long handle);
    private static native void nativeUnload(long handle);
}
