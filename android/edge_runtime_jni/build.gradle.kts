import com.inferlab.edge.build.AarContractSettings
import com.inferlab.edge.build.AarPackagingAssets
import com.inferlab.edge.build.AarPackagingInputs
import java.io.File

plugins {
    id("com.android.library") version "8.7.3"
}

val inferlabCompileSdk = 35
val inferlabMinSdk = 24
val inferlabNdkVersion = "27.0.12077973"
val inferlabCmakeVersion = "3.22.1"
val inferlabAbi = "arm64-v8a"
val inferlabPackageId = "com.inferlab.edge"
val inferlabNativeTarget = "inferlab_edge_runtime_jni"
val inferlabNativeLibrary = "libinferlab_edge_runtime_jni.so"

fun optionalAbsolutePathProperty(name: String): File? {
    val value = providers.gradleProperty(name).orNull?.trim()
        ?.takeIf { it.isNotEmpty() } ?: return null
    require('\u0000' !in value && '\n' !in value && '\r' !in value &&
            ';' !in value) {
        "$name must be a single filesystem path without control characters or semicolons"
    }
    val candidate = file(value)
    require(candidate.isAbsolute) { "$name must be an absolute path" }
    return candidate
}

val llamaCppSourceDir = optionalAbsolutePathProperty(
    "inferlabLlamaCppSourceDir")
val executorchSourceDir = optionalAbsolutePathProperty(
    "inferlabExecutorchSourceDir")
val executorchInstallDir = optionalAbsolutePathProperty(
    "inferlabExecutorchInstallDir")
val executorchBuildRevisionFile = optionalAbsolutePathProperty(
    "inferlabExecutorchBuildRevisionFile")
val inferlabProjectLicenseFile = optionalAbsolutePathProperty(
    "inferlabProjectLicenseFile")
val thirdPartyLicenseReviewFile = optionalAbsolutePathProperty(
    "inferlabThirdPartyLicenseReviewFile")

val executorchInputs = listOf(
    executorchSourceDir,
    executorchInstallDir,
    executorchBuildRevisionFile)
require(executorchInputs.all { it == null } ||
        executorchInputs.all { it != null }) {
    "ExecuTorch requires inferlabExecutorchSourceDir, " +
            "inferlabExecutorchInstallDir and " +
            "inferlabExecutorchBuildRevisionFile together"
}

val llamaCppEnabled = llamaCppSourceDir != null
val executorchEnabled = executorchInputs.all { it != null }
val cmakeArguments = mutableListOf(
    "-DINFERLAB_ENABLE_ANDROID_JNI=ON",
    "-DINFERLAB_ENABLE_ARM_NEON=ON",
    "-DINFERLAB_ENABLE_ARM_SVE=OFF",
    "-DINFERLAB_ENABLE_CUDA=OFF",
    "-DINFERLAB_ENABLE_NVTX=OFF",
    "-DINFERLAB_ENABLE_THIRD_PARTY_JSON=OFF",
    "-DINFERLAB_ENABLE_EVIDENCE_SQLITE=OFF",
    "-DINFERLAB_ENABLE_GOOGLE_BENCHMARK=OFF",
    "-DINFERLAB_FETCH_THIRD_PARTY_DEPS=OFF",
    "-DINFERLAB_ENABLE_EDGE_LLAMA_CPP=${if (llamaCppEnabled) "ON" else "OFF"}",
    "-DINFERLAB_ENABLE_EDGE_EXECUTORCH=${if (executorchEnabled) "ON" else "OFF"}")

if (llamaCppSourceDir != null) {
    cmakeArguments +=
        "-DINFERLAB_LLAMA_CPP_SOURCE_DIR:PATH=${llamaCppSourceDir.absolutePath}"
}
if (executorchEnabled) {
    cmakeArguments += listOf(
        "-DINFERLAB_EXECUTORCH_SOURCE_DIR:PATH=${executorchSourceDir!!.absolutePath}",
        "-DINFERLAB_EXECUTORCH_INSTALL_DIR:PATH=${executorchInstallDir!!.absolutePath}",
        "-DINFERLAB_EXECUTORCH_BUILD_REVISION_FILE:FILEPATH=${executorchBuildRevisionFile!!.absolutePath}")
}

val generatedPackagingAssets = layout.buildDirectory.dir(
    "generated/inferlabEdgePackaging/assets")
val aarContractSettings = AarContractSettings(
    expectedConsumerRulesFile = file("consumer-rules.pro"),
    expectedPackageId = inferlabPackageId,
    expectedAbi = inferlabAbi,
    expectedMinSdk = inferlabMinSdk,
    expectedCompileSdk = inferlabCompileSdk,
    expectedNdkVersion = inferlabNdkVersion,
    expectedCmakeVersion = inferlabCmakeVersion,
    expectedNativeTarget = inferlabNativeTarget,
    expectedNativeLibrary = inferlabNativeLibrary,
    forbiddenHostPaths = listOfNotNull(
        llamaCppSourceDir,
        executorchSourceDir,
        executorchInstallDir,
        executorchBuildRevisionFile,
        inferlabProjectLicenseFile,
        thirdPartyLicenseReviewFile).map { it.absolutePath })
val aarPackagingInputs = AarPackagingInputs(
    contract = aarContractSettings,
    llamaCppSourceDir = llamaCppSourceDir,
    executorchSourceDir = executorchSourceDir,
    projectLicenseFile = inferlabProjectLicenseFile,
    thirdPartyLicenseReviewFile = thirdPartyLicenseReviewFile,
)

val prepareInferLabEdgePackagingAssets = tasks.register(
    "prepareInferLabEdgePackagingAssets") {
    inputs.property("abi", inferlabAbi)
    inputs.property("packageId", aarContractSettings.expectedPackageId)
    inputs.property("compileSdk", inferlabCompileSdk)
    inputs.property("minSdk", inferlabMinSdk)
    inputs.property("ndkVersion", inferlabNdkVersion)
    inputs.property("cmakeVersion", inferlabCmakeVersion)
    inputs.property("nativeTarget", inferlabNativeTarget)
    inputs.property("nativeLibrary", inferlabNativeLibrary)
    inputs.property("llamaCppEnabled", llamaCppEnabled)
    inputs.property("executorchEnabled", executorchEnabled)
    inferlabProjectLicenseFile?.let { inputs.file(it) }
    thirdPartyLicenseReviewFile?.let { inputs.file(it) }
    llamaCppSourceDir?.let { inputs.file(File(it, "LICENSE")) }
    executorchSourceDir?.let { inputs.file(File(it, "LICENSE")) }
    outputs.dir(generatedPackagingAssets)

    doLast {
        AarPackagingAssets.prepare(
            generatedPackagingAssets.get().asFile,
            aarPackagingInputs,
        )
    }
}

val validateInferLabReleasePackaging = tasks.register(
    "validateInferLabReleasePackaging") {
    dependsOn(prepareInferLabEdgePackagingAssets)
    doLast {
        AarPackagingAssets.validateRelease(aarPackagingInputs)
    }
}

android {
    namespace = inferlabPackageId
    compileSdk = inferlabCompileSdk
    ndkVersion = inferlabNdkVersion

    defaultConfig {
        minSdk = inferlabMinSdk
        consumerProguardFiles("consumer-rules.pro")
        ndk {
            abiFilters += inferlabAbi
        }
        externalNativeBuild {
            cmake {
                arguments += cmakeArguments
                targets += inferlabNativeTarget
            }
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../../CMakeLists.txt")
            version = inferlabCmakeVersion
        }
    }

    buildTypes {
        debug {
            externalNativeBuild {
                cmake {
                    arguments += "-DINFERLAB_REQUIRE_EDGE_RUNTIME_LINKED=OFF"
                }
            }
        }
        release {
            isMinifyEnabled = false
            externalNativeBuild {
                cmake {
                    arguments += "-DINFERLAB_REQUIRE_EDGE_RUNTIME_LINKED=ON"
                }
            }
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    buildFeatures {
        buildConfig = false
    }

    packaging {
        jniLibs {
            useLegacyPackaging = false
        }
    }

    sourceSets.getByName("main") {
        assets.srcDir(generatedPackagingAssets)
    }

    publishing {
        singleVariant("release") {
            withSourcesJar()
        }
    }
}

tasks.matching { it.name == "preBuild" }.configureEach {
    dependsOn(prepareInferLabEdgePackagingAssets)
}
tasks.matching { it.name == "preReleaseBuild" }.configureEach {
    dependsOn(validateInferLabReleasePackaging)
}

extensions.extraProperties["inferlabAarContractSettings"] = aarContractSettings
apply(from = "gradle/aar-verification.gradle.kts")
