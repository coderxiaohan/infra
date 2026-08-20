import com.inferlab.edge.build.AarContractRequest
import com.inferlab.edge.build.AarContractSettings
import com.inferlab.edge.build.AarContractVerifier

val contractSettings =
    extensions.extraProperties["inferlabAarContractSettings"] as? AarContractSettings
        ?: throw GradleException("InferLab typed AAR contract settings are missing")
val releaseAar = layout.buildDirectory.file(
    "outputs/aar/${project.name}-release.aar",
)

tasks.register("verifyInferLabReleaseAar") {
    group = "verification"
    description = "Assembles and verifies the bounded InferLab release AAR contract"
    dependsOn("bundleReleaseAar")
    inputs.file(releaseAar)
    inputs.file(contractSettings.expectedConsumerRulesFile)
    inputs.property("expectedPackageId", contractSettings.expectedPackageId)
    inputs.property("expectedAbi", contractSettings.expectedAbi)
    inputs.property("expectedMinSdk", contractSettings.expectedMinSdk)
    inputs.property("expectedCompileSdk", contractSettings.expectedCompileSdk)
    inputs.property("expectedNdkVersion", contractSettings.expectedNdkVersion)
    inputs.property("expectedCmakeVersion", contractSettings.expectedCmakeVersion)
    inputs.property("expectedNativeTarget", contractSettings.expectedNativeTarget)
    inputs.property("expectedNativeLibrary", contractSettings.expectedNativeLibrary)

    doLast {
        AarContractVerifier.verify(
            AarContractRequest(
                aar = releaseAar.get().asFile,
                settings = contractSettings,
            ),
        )
        logger.lifecycle("InferLab release AAR packaging contract verified")
    }
}
