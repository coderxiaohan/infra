cmake_minimum_required(VERSION 3.20)

project(InferLab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(INFERLAB_ENABLE_CUDA "Build optional CUDA kernel experiments" OFF)
option(INFERLAB_ENABLE_THIRD_PARTY_JSON "Enable nlohmann/json artifact parsing" ON)
option(INFERLAB_ENABLE_EVIDENCE_SQLITE "Enable SQLite evidence warehouse backend" OFF)
option(INFERLAB_ENABLE_GOOGLE_BENCHMARK "Enable google/benchmark microbenchmark harness" OFF)
option(INFERLAB_ENABLE_NVTX "Enable CUDA NVTX profiling ranges when CUDA is enabled" OFF)
option(INFERLAB_FETCH_THIRD_PARTY_DEPS "Allow CMake to download optional third-party dependencies when they are not installed" OFF)
option(INFERLAB_ENABLE_EDGE_LLAMA_CPP "Request the optional embedded llama.cpp edge runtime integration" OFF)
option(INFERLAB_ENABLE_EDGE_EXECUTORCH "Request the optional ExecuTorch/XNNPACK edge runtime integration" OFF)
option(INFERLAB_REQUIRE_EDGE_RUNTIME_LINKED "Fail configuration unless at least one optional edge runtime is linked" OFF)
option(INFERLAB_ENABLE_ANDROID_JNI "Build the Android JNI bridge for the unified edge runtime" OFF)
option(INFERLAB_ENABLE_EDGE_CLOUD_CURL "Enable the optional libcurl OpenAI-compatible edge-cloud transport" OFF)
set(INFERLAB_LLAMA_CPP_SOURCE_DIR "" CACHE PATH
  "Path to an existing llama.cpp checkout pinned at e9fa078; InferLab never downloads it implicitly")
set(INFERLAB_LLAMA_CPP_EXPECTED_REVISION "e9fa078")
set(INFERLAB_EXECUTORCH_SOURCE_DIR "" CACHE PATH
  "Path to the ExecuTorch source checkout used to build the external SDK")
set(INFERLAB_EXECUTORCH_INSTALL_DIR "" CACHE PATH
  "Path to an installed ExecuTorch v1.2.0 SDK with LLM runner and XNNPACK targets")
set(INFERLAB_EXECUTORCH_BUILD_REVISION_FILE "" CACHE FILEPATH
  "File emitted with the ExecuTorch SDK containing the complete source commit used to build it")
set(INFERLAB_EXECUTORCH_EXPECTED_REVISION "0b0e2c5")
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
  set(INFERLAB_ARM_NEON_DEFAULT ON)
else()
  set(INFERLAB_ARM_NEON_DEFAULT OFF)
endif()
option(INFERLAB_ENABLE_ARM_NEON "Build ARM NEON edge microkernels on AArch64" ${INFERLAB_ARM_NEON_DEFAULT})
option(INFERLAB_ENABLE_ARM_SVE "Request the future ARM SVE/SVE2 edge backend (not implemented)" OFF)

function(inferlab_verify_pinned_git_revision source_dir expected_revision
         verified_out resolved_revision_out)
  set(_verified OFF)
  set(_current_revision "")
  find_package(Git QUIET)
  if(Git_FOUND)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} -C "${source_dir}" rev-parse HEAD^{commit}
      RESULT_VARIABLE _current_status
      OUTPUT_VARIABLE _current_revision
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} -C "${source_dir}"
              rev-parse ${expected_revision}^{commit}
      RESULT_VARIABLE _expected_status
      OUTPUT_VARIABLE _expected_full_revision
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    execute_process(
      COMMAND ${GIT_EXECUTABLE} -C "${source_dir}"
              status --porcelain=v1 --untracked-files=normal
      RESULT_VARIABLE _worktree_status
      OUTPUT_VARIABLE _worktree_changes
      ERROR_QUIET
      OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(_current_status EQUAL 0 AND _expected_status EQUAL 0 AND
       _worktree_status EQUAL 0 AND _worktree_changes STREQUAL "" AND
       _current_revision STREQUAL _expected_full_revision)
      set(_verified ON)
    endif()
  endif()
  set(${verified_out} ${_verified} PARENT_SCOPE)
  set(${resolved_revision_out} "${_current_revision}" PARENT_SCOPE)
endfunction()

if(INFERLAB_FETCH_THIRD_PARTY_DEPS)
  include(FetchContent)
endif()

if(INFERLAB_ENABLE_THIRD_PARTY_JSON)
  find_package(nlohmann_json 3.11.3 QUIET)
  if(NOT nlohmann_json_FOUND AND INFERLAB_FETCH_THIRD_PARTY_DEPS)
    FetchContent_Declare(
      nlohmann_json
      GIT_REPOSITORY https://github.com/nlohmann/json.git
      GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
  elseif(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found; high-value targets will use fallback artifact parsing. Set INFERLAB_FETCH_THIRD_PARTY_DEPS=ON to allow CMake to download v3.11.3.")
  endif()
endif()

if(INFERLAB_ENABLE_EVIDENCE_SQLITE)
  find_package(SQLite3 QUIET)
endif()

if(INFERLAB_ENABLE_EDGE_CLOUD_CURL)
  find_package(CURL 8.5 QUIET)
  if(NOT CURL_FOUND)
    message(STATUS "libcurl 8.5+ not found; edge-cloud routing remains capability-only and cannot execute cloud requests")
  endif()
endif()

if(INFERLAB_ENABLE_GOOGLE_BENCHMARK)
  find_package(benchmark QUIET)
  if(NOT benchmark_FOUND AND INFERLAB_FETCH_THIRD_PARTY_DEPS)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
      google_benchmark
      GIT_REPOSITORY https://github.com/google/benchmark.git
      GIT_TAG v1.8.5
    )
    FetchContent_MakeAvailable(google_benchmark)
  elseif(NOT benchmark_FOUND)
    message(STATUS "google/benchmark not found; microbenchmark harness will emit manifest/fallback metadata only. Set INFERLAB_FETCH_THIRD_PARTY_DEPS=ON to allow CMake to download v1.8.5.")
  endif()
endif()

if(INFERLAB_ENABLE_CUDA AND INFERLAB_ENABLE_NVTX)
  find_package(CUDAToolkit QUIET)
endif()

function(inferlab_configure_evidence_sqlite_target target_name)
  if(INFERLAB_ENABLE_EVIDENCE_SQLITE AND SQLite3_FOUND)
    target_link_libraries(${target_name} PRIVATE SQLite::SQLite3)
    target_compile_definitions(${target_name} PRIVATE INFERLAB_HAS_SQLITE=1)
  else()
    target_compile_definitions(${target_name} PRIVATE INFERLAB_HAS_SQLITE=0)
  endif()
endfunction()

function(inferlab_configure_kernel_microbench_target target_name)
  if(INFERLAB_ENABLE_GOOGLE_BENCHMARK AND TARGET benchmark::benchmark)
    target_link_libraries(${target_name} PRIVATE benchmark::benchmark)
    target_compile_definitions(${target_name} PRIVATE INFERLAB_HAS_GOOGLE_BENCHMARK=1)
  else()
    target_compile_definitions(${target_name} PRIVATE INFERLAB_HAS_GOOGLE_BENCHMARK=0)
  endif()

  if(INFERLAB_ENABLE_CUDA)
    target_compile_definitions(${target_name} PRIVATE INFERLAB_ENABLE_CUDA_BUILD=1)
  else()
    target_compile_definitions(${target_name} PRIVATE INFERLAB_ENABLE_CUDA_BUILD=0)
  endif()

  if(INFERLAB_ENABLE_CUDA AND INFERLAB_ENABLE_NVTX AND CUDAToolkit_FOUND AND TARGET CUDA::nvtx3)
    target_link_libraries(${target_name} PRIVATE CUDA::nvtx3)
    target_compile_definitions(${target_name} PRIVATE INFERLAB_HAS_NVTX=1 INFERLAB_NVTX3_HEADER=1)
  elseif(INFERLAB_ENABLE_CUDA AND INFERLAB_ENABLE_NVTX AND CUDAToolkit_FOUND AND TARGET CUDA::nvToolsExt)
    target_link_libraries(${target_name} PRIVATE CUDA::nvToolsExt)
    target_compile_definitions(${target_name} PRIVATE INFERLAB_HAS_NVTX=1 INFERLAB_NVTX3_HEADER=0)
  else()
    target_compile_definitions(${target_name} PRIVATE INFERLAB_HAS_NVTX=0 INFERLAB_NVTX3_HEADER=0)
  endif()
endfunction()

# Compile a command dispatcher once while preserving explicitly named
# compatibility executables during the internal caller migration window.
function(inferlab_add_shared_dispatch_group)
  set(options)
  set(one_value_args OBJECT_TARGET SOURCE CANONICAL_TARGET)
  set(multi_value_args SOURCES COMPATIBILITY_TARGETS)
  cmake_parse_arguments(DISPATCH "${options}" "${one_value_args}" "${multi_value_args}" ${ARGN})

  if(NOT DISPATCH_OBJECT_TARGET OR NOT DISPATCH_CANONICAL_TARGET)
    message(FATAL_ERROR "inferlab_add_shared_dispatch_group requires OBJECT_TARGET and CANONICAL_TARGET")
  endif()
  set(dispatch_sources ${DISPATCH_SOURCE} ${DISPATCH_SOURCES})
  if(NOT dispatch_sources)
    message(FATAL_ERROR "inferlab_add_shared_dispatch_group requires SOURCE or SOURCES")
  endif()

  add_library(${DISPATCH_OBJECT_TARGET} OBJECT ${dispatch_sources})
  target_link_libraries(${DISPATCH_OBJECT_TARGET} PRIVATE inferlab_native_core)

  set(dispatch_targets
    ${DISPATCH_CANONICAL_TARGET}
    ${DISPATCH_COMPATIBILITY_TARGETS}
  )
  foreach(target_name IN LISTS dispatch_targets)
    add_executable(${target_name} $<TARGET_OBJECTS:${DISPATCH_OBJECT_TARGET}>)
    target_link_libraries(${target_name} PRIVATE inferlab_native_core)
    # All dispatcher sources are compiled by the object target above. Keep
    # compile-time high-value capabilities there instead of duplicating them on
    # source-free canonical and compatibility executable shells.
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_CANONICAL_EXECUTABLE ${DISPATCH_CANONICAL_TARGET})
  endforeach()

  set_property(TARGET ${DISPATCH_CANONICAL_TARGET} PROPERTY
    INFERLAB_DISPATCH_ROLE canonical)
  set_property(TARGET ${DISPATCH_CANONICAL_TARGET} PROPERTY
    INFERLAB_COMPATIBILITY_STATUS canonical)
  foreach(target_name IN LISTS DISPATCH_COMPATIBILITY_TARGETS)
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_DISPATCH_ROLE compatibility)
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_COMPATIBILITY_OWNER ${DISPATCH_CANONICAL_TARGET})
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_COMPATIBILITY_STATUS active_compatibility)
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_COMPATIBILITY_CALLERS
      "repository_runtime_callers_require_bounded_scan;documentation_references_are_not_runtime_callers;external_callers_require_inventory")
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_COMPATIBILITY_REPOSITORY_RUNTIME_CALLER_STATUS
      runtime_scan_required)
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_COMPATIBILITY_DOCUMENTATION_REFERENCE_STATUS
      retained_non_runtime_reference)
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_COMPATIBILITY_EXTERNAL_CALLER_AUDIT_STATUS
      pending_external_inventory)
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_COMPATIBILITY_PARITY_STATUS evidence_import_required)
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_COMPATIBILITY_REMOVAL_ALLOWED FALSE)
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_COMPATIBILITY_REMOVAL_CONDITION
      "canonical command parity is validated and external callers have migrated")
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_COMPATIBILITY_REMOVAL_BLOCKER
      "runtime governance evidence must be imported by the lifecycle gate")
    set_property(TARGET ${target_name} PROPERTY
      INFERLAB_COMPATIBILITY_TARGET_RELEASE post_dispatch_parity_validation)
    set_property(GLOBAL APPEND PROPERTY INFERLAB_COMPATIBILITY_MANIFEST_ENTRIES
      "{\"entry_id\":\"cmake.${target_name}\",\"compatibility_target\":\"${target_name}\",\"canonical_target\":\"${DISPATCH_CANONICAL_TARGET}\",\"path\":\"$<TARGET_FILE:${target_name}>\",\"replacement\":\"$<TARGET_FILE:${DISPATCH_CANONICAL_TARGET}>\",\"target_release\":\"post_dispatch_parity_validation\"}")
  endforeach()
endfunction()

find_package(Threads REQUIRED)

enable_testing()

add_library(inferlab_native_core
  native_core/src/advanced_serving_capacity.cpp
  native_core/src/advanced_serving_chunked_prefill.cpp
  native_core/src/advanced_serving_control_plane.cpp
  native_core/src/advanced_serving_deployment_planning.cpp
  native_core/src/advanced_serving_replica_placement.cpp
  native_core/src/advanced_serving_replica_placement_fixture.cpp
  native_core/src/advanced_serving_generation_strategy.cpp
  native_core/src/advanced_serving_kv_compression_blend.cpp
  native_core/src/advanced_serving_kv_offload_hierarchy.cpp
  native_core/src/advanced_serving_lora.cpp
  native_core/src/advanced_serving_observability.cpp
  native_core/src/advanced_serving_reuse_cache.cpp
  native_core/src/advanced_serving_routing.cpp
  native_core/src/advanced_serving_gateway_policy.cpp
  native_core/src/kv_transport_replay_contract.cpp
  native_core/src/advanced_serving_kv_transport_replay.cpp
  native_core/src/advanced_serving_pd_split_crossover.cpp
  native_core/src/advanced_serving_pd_split_crossover_fixture.cpp
  native_core/src/pd_transfer_overlap_contract.cpp
  native_core/src/advanced_serving_pd_transfer_overlap.cpp
  native_core/src/advanced_serving_runtime_communication.cpp
  native_core/src/advanced_serving_structured_delivery.cpp
  native_core/src/attention.cpp
  native_core/src/artifact_contract_fixture.cpp
  native_core/src/artifact_evidence_contract.cpp
  native_core/src/artifact_profile_section.cpp
  native_core/src/artifact_semantic_index.cpp
  native_core/src/evidence_fixture_corpus_model.cpp
  native_core/src/catalog_derived_policy.cpp
  native_core/src/catalog_derived_surface_contract.cpp
  native_core/src/catalog_derived_surface_views.cpp
  native_core/src/catalog_required_metrics.cpp
  native_core/src/catalog_required_metrics_evidence_query_core.cpp
  native_core/src/catalog_required_metrics_evidence_query_extended.cpp
  native_core/src/catalog_required_metrics_governance.cpp
  native_core/src/catalog_required_metrics_helpers.cpp
  native_core/src/catalog_required_metrics_release_pipeline_native.cpp
  native_core/src/catalog_required_metrics_release_pipeline_query.cpp
  native_core/src/claim_scope_contract_matrix.cpp
  native_core/src/continuous_batch_scheduler.cpp
  native_core/src/decoder_block.cpp
  native_core/src/edge_cloud.cpp
  native_core/src/edge_cloud_budget_policy.cpp
  native_core/src/edge_cloud_session_recovery_policy.cpp
  native_core/src/edge_kernels.cpp
  native_core/src/edge_process_metrics.cpp
  native_core/src/edge_runtime.cpp
  native_core/src/edge_runtime_adapter_utils.cpp
  native_core/src/decoder_kv.cpp
  native_core/src/deep_dive_contract_check_model.cpp
  native_core/src/deep_dive_evidence_bundle_fixture.cpp
  native_core/src/deep_dive_artifact_identity.cpp
  native_core/src/dependency_governance_cli.cpp
  native_core/src/dependency_governance_model.cpp
  native_core/src/dependency_governance_view.cpp
  native_core/src/runtime_evidence_freshness_gate_legacy_targets.cpp
  native_core/src/runtime_evidence_freshness_gate_model.cpp
  native_core/src/evidence_query_cli_parser.cpp
  native_core/src/evidence_query_operation_catalog.cpp
  native_core/src/exact_one_profile_match.cpp
  native_core/src/gguf.cpp
  native_core/src/kv_cache.cpp
  native_core/src/kv_paged_attention_deep_dive_model.cpp
  native_core/src/kv_paged_attention_deep_dive_view.cpp
  native_core/src/kv_tier_decision_deep_dive_model.cpp
  native_core/src/kv_tier_decision_deep_dive_view.cpp
  native_core/src/kserve_llmd_plan_model.cpp
  native_core/src/kserve_llmd_plan_view.cpp
  native_core/src/matrix_ops.cpp
  native_core/src/product_profile_artifact_contract.cpp
  native_core/src/mature_product_profile_contract.cpp
  native_core/src/native_artifact_semantic_contract.cpp
  native_core/src/native_trace_context.cpp
  native_core/src/native_trace_context_view.cpp
  native_core/src/product_replay_alignment_fixture.cpp
  native_core/src/native_core_audit_query_projection.cpp
  native_core/src/native_core_audit_register_projection.cpp
  native_core/src/normalization.cpp
  native_core/src/paged_attention.cpp
  native_core/src/paged_kv_cache.cpp
  native_core/src/paged_kv_locality.cpp
  native_core/src/product_replay_semantic_linkage_projection.cpp
  native_core/src/python_source_contract.cpp
  native_core/src/quantization.cpp
  native_core/src/quantization_reference_support.cpp
  native_core/src/release_evidence_pipeline_stage.cpp
  native_core/src/release_kv_budget_profile.cpp
  native_core/src/release_kv_budget_profile_view.cpp
  native_core/src/release_lora_prefix_isolation_profile.cpp
  native_core/src/release_lora_prefix_isolation_profile_view.cpp
  native_core/src/release_deployment_resource_profile.cpp
  native_core/src/release_deployment_resource_profile_view.cpp
  native_core/src/release_traffic_profile.cpp
  native_core/src/release_traffic_profile_view.cpp
  native_core/src/repository_evidence_consumer_registry.cpp
  native_core/src/rope.cpp
  native_core/src/scheduler_deep_dive_cli.cpp
  native_core/src/scheduler_deep_dive_model.cpp
  native_core/src/scheduler_deep_dive_view.cpp
  native_core/src/shape_trace.cpp
  native_core/src/static_kv_cache.cpp
  native_core/src/trace_clock_calibration.cpp
  native_core/src/trace_clock_calibration_view.cpp
  native_core/src/trace_correlation_contract.cpp
  native_core/src/trace_span_lifecycle_contract.cpp
  native_core/src/vector_add.cpp
)
target_include_directories(inferlab_native_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/native_core/include
)
target_link_libraries(inferlab_native_core PUBLIC Threads::Threads)
if(INFERLAB_ENABLE_THIRD_PARTY_JSON AND TARGET nlohmann_json::nlohmann_json)
  target_link_libraries(inferlab_native_core PUBLIC nlohmann_json::nlohmann_json)
  target_compile_definitions(inferlab_native_core PUBLIC
    INFERLAB_HAS_NLOHMANN_JSON=1)
else()
  target_compile_definitions(inferlab_native_core PUBLIC
    INFERLAB_HAS_NLOHMANN_JSON=0)
endif()

set(INFERLAB_EDGE_LLAMA_CPP_LINKED OFF)
if(INFERLAB_ENABLE_EDGE_LLAMA_CPP)
  if(NOT INFERLAB_LLAMA_CPP_SOURCE_DIR)
    message(STATUS
      "llama.cpp edge adapter requested but INFERLAB_LLAMA_CPP_SOURCE_DIR is empty; capability remains dependency-unavailable")
  elseif(NOT EXISTS "${INFERLAB_LLAMA_CPP_SOURCE_DIR}/CMakeLists.txt")
    message(STATUS
      "llama.cpp edge adapter requested but source CMakeLists.txt is missing; capability remains dependency-unavailable")
  else()
    inferlab_verify_pinned_git_revision(
      "${INFERLAB_LLAMA_CPP_SOURCE_DIR}"
      "${INFERLAB_LLAMA_CPP_EXPECTED_REVISION}"
      _inferlab_llama_revision_verified
      _inferlab_llama_revision)
    if(NOT _inferlab_llama_revision_verified)
      message(STATUS
        "llama.cpp source must have a clean worktree and HEAD exactly matching ${INFERLAB_LLAMA_CPP_EXPECTED_REVISION}; adapter remains dependency-unavailable")
    else()
      set(LLAMA_BUILD_COMMON OFF CACHE BOOL "" FORCE)
      set(LLAMA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
      set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
      set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
      set(LLAMA_BUILD_SERVER OFF CACHE BOOL "" FORCE)
      add_subdirectory(
        "${INFERLAB_LLAMA_CPP_SOURCE_DIR}"
        "${CMAKE_BINARY_DIR}/_deps/llama_cpp"
        EXCLUDE_FROM_ALL)
      if(TARGET llama)
        set(_inferlab_llama_target llama)
      elseif(TARGET llama::llama)
        set(_inferlab_llama_target llama::llama)
      else()
        set(_inferlab_llama_target "")
      endif()
      if(_inferlab_llama_target)
        target_sources(inferlab_native_core PRIVATE
          native_core/src/edge_llama_cpp_runtime.cpp)
        target_link_libraries(inferlab_native_core PRIVATE
          ${_inferlab_llama_target})
        set(INFERLAB_EDGE_LLAMA_CPP_LINKED ON)
      else()
        message(STATUS
          "pinned llama.cpp source did not define target llama or llama::llama; adapter remains dependency-unavailable")
      endif()
    endif()
  endif()
endif()

if(INFERLAB_EDGE_LLAMA_CPP_LINKED)
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_EDGE_LLAMA_CPP_LINKED=1)
else()
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_EDGE_LLAMA_CPP_LINKED=0)
endif()

set(INFERLAB_EDGE_EXECUTORCH_LINKED OFF)
if(INFERLAB_ENABLE_EDGE_EXECUTORCH)
  if(NOT INFERLAB_EXECUTORCH_SOURCE_DIR OR
     NOT INFERLAB_EXECUTORCH_INSTALL_DIR OR
     NOT INFERLAB_EXECUTORCH_BUILD_REVISION_FILE)
    message(STATUS
      "ExecuTorch edge adapter requested but source/install/revision metadata is missing; capability remains dependency-unavailable")
  elseif(NOT EXISTS "${INFERLAB_EXECUTORCH_SOURCE_DIR}/CMakeLists.txt")
    message(STATUS
      "ExecuTorch source CMakeLists.txt is missing; capability remains dependency-unavailable")
  else()
    inferlab_verify_pinned_git_revision(
      "${INFERLAB_EXECUTORCH_SOURCE_DIR}"
      "${INFERLAB_EXECUTORCH_EXPECTED_REVISION}"
      _inferlab_executorch_revision_verified
      _inferlab_executorch_revision)
    if(NOT _inferlab_executorch_revision_verified)
      message(STATUS
        "ExecuTorch source must have a clean worktree and HEAD exactly matching ${INFERLAB_EXECUTORCH_EXPECTED_REVISION}; adapter remains dependency-unavailable")
    elseif(NOT EXISTS "${INFERLAB_EXECUTORCH_BUILD_REVISION_FILE}")
      message(STATUS
        "ExecuTorch SDK build revision file is missing; adapter remains dependency-unavailable")
    elseif(IS_DIRECTORY "${INFERLAB_EXECUTORCH_BUILD_REVISION_FILE}")
      message(STATUS
        "ExecuTorch SDK build revision metadata path is not a file; adapter remains dependency-unavailable")
    else()
      file(READ "${INFERLAB_EXECUTORCH_BUILD_REVISION_FILE}"
        _inferlab_executorch_sdk_revision LIMIT 128)
      string(STRIP "${_inferlab_executorch_sdk_revision}"
        _inferlab_executorch_sdk_revision)
      if(NOT _inferlab_executorch_sdk_revision STREQUAL
             _inferlab_executorch_revision)
        message(STATUS
          "ExecuTorch SDK build revision does not match the verified source checkout; adapter remains dependency-unavailable")
      else()
        find_package(executorch CONFIG QUIET
          PATHS "${INFERLAB_EXECUTORCH_INSTALL_DIR}"
          NO_DEFAULT_PATH)
        if(TARGET extension_llm_runner AND TARGET xnnpack_backend)
          target_sources(inferlab_native_core PRIVATE
            native_core/src/edge_executorch_runtime.cpp)
          target_link_libraries(inferlab_native_core PRIVATE
            extension_llm_runner xnnpack_backend)
          foreach(_inferlab_executorch_target IN ITEMS
              XNNPACK
              xnnpack-microkernels-prod
              portable_ops_lib
              portable_kernels
              quantized_kernels
              quantized_ops_lib
              optimized_native_cpu_ops_lib
              optimized_kernels
              custom_ops
              torchao_ops_executorch)
            if(TARGET ${_inferlab_executorch_target})
              target_link_libraries(inferlab_native_core PRIVATE
                ${_inferlab_executorch_target})
            endif()
          endforeach()
          if(COMMAND executorch_target_link_options_shared_lib)
            executorch_target_link_options_shared_lib(xnnpack_backend)
            if(TARGET portable_ops_lib)
              executorch_target_link_options_shared_lib(portable_ops_lib)
            endif()
            if(TARGET quantized_ops_lib)
              executorch_target_link_options_shared_lib(quantized_ops_lib)
            endif()
            if(TARGET optimized_native_cpu_ops_lib)
              executorch_target_link_options_shared_lib(
                optimized_native_cpu_ops_lib)
            endif()
            if(TARGET custom_ops)
              executorch_target_link_options_shared_lib(custom_ops)
            endif()
            if(TARGET torchao_ops_executorch)
              executorch_target_link_options_shared_lib(torchao_ops_executorch)
            endif()
            set(INFERLAB_EDGE_EXECUTORCH_LINKED ON)
          else()
            message(STATUS
              "ExecuTorch package lacks executorch_target_link_options_shared_lib; static backend registration cannot be guaranteed")
          endif()
        else()
          message(STATUS
            "ExecuTorch SDK lacks extension_llm_runner or xnnpack_backend; rebuild/install v1.2.0 with LLM runner and XNNPACK enabled")
        endif()
      endif()
    endif()
  endif()
endif()

if(INFERLAB_EDGE_EXECUTORCH_LINKED)
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_EDGE_EXECUTORCH_LINKED=1)
else()
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_EDGE_EXECUTORCH_LINKED=0)
endif()
if(INFERLAB_REQUIRE_EDGE_RUNTIME_LINKED AND
   NOT INFERLAB_EDGE_LLAMA_CPP_LINKED AND
   NOT INFERLAB_EDGE_EXECUTORCH_LINKED)
  message(FATAL_ERROR
    "INFERLAB_REQUIRE_EDGE_RUNTIME_LINKED requires an exact-pinned llama.cpp or ExecuTorch adapter to pass its dependency and target-link gates")
endif()
if(INFERLAB_ENABLE_ARM_NEON)
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_ENABLE_ARM_NEON_BUILD=1)
else()
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_ENABLE_ARM_NEON_BUILD=0)
endif()
if(INFERLAB_ENABLE_ARM_SVE)
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_ENABLE_ARM_SVE_REQUESTED=1)
  message(STATUS
    "ARM SVE/SVE2 edge backend was requested but is not implemented; capability remains unavailable")
else()
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_ENABLE_ARM_SVE_REQUESTED=0)
endif()
if(INFERLAB_ENABLE_EDGE_LLAMA_CPP)
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_ENABLE_EDGE_LLAMA_CPP_BUILD=1)
else()
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_ENABLE_EDGE_LLAMA_CPP_BUILD=0)
endif()
if(INFERLAB_ENABLE_EDGE_EXECUTORCH)
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_ENABLE_EDGE_EXECUTORCH_BUILD=1)
else()
  target_compile_definitions(inferlab_native_core PRIVATE
    INFERLAB_ENABLE_EDGE_EXECUTORCH_BUILD=0)
endif()

if(INFERLAB_ENABLE_ANDROID_JNI)
  if(NOT ANDROID)
    message(FATAL_ERROR
      "INFERLAB_ENABLE_ANDROID_JNI requires an Android NDK toolchain")
  endif()
  set_property(TARGET inferlab_native_core PROPERTY POSITION_INDEPENDENT_CODE ON)
  add_library(inferlab_edge_runtime_jni SHARED
    native_core/src/android_edge_runtime_jni.cpp
  )
  target_compile_features(inferlab_edge_runtime_jni PRIVATE cxx_std_17)
  target_compile_options(inferlab_edge_runtime_jni PRIVATE
    -Wall -Wextra -Wpedantic)
  target_link_libraries(inferlab_edge_runtime_jni PRIVATE inferlab_native_core)
  set_target_properties(inferlab_edge_runtime_jni PROPERTIES
    OUTPUT_NAME inferlab_edge_runtime_jni)
endif()

add_executable(inferlab_shape_probe
  native_core/src/shape_probe_main.cpp
)
target_link_libraries(inferlab_shape_probe PRIVATE inferlab_native_core)

add_executable(inferlab_scheduler_trace
  native_core/src/scheduler_trace_main.cpp
)
target_link_libraries(inferlab_scheduler_trace PRIVATE inferlab_native_core)

add_executable(inferlab_scheduler_pressure_sweep
  native_core/src/scheduler_pressure_sweep_main.cpp
  native_core/src/scheduler_pressure_sweep_model.cpp
  native_core/src/scheduler_pressure_sweep_view.cpp
)
target_link_libraries(inferlab_scheduler_pressure_sweep PRIVATE inferlab_native_core)

add_executable(inferlab_scheduler_workload_sweep
  native_core/src/scheduler_workload_sweep_main.cpp
  native_core/src/scheduler_workload_sweep_model.cpp
  native_core/src/scheduler_workload_sweep_view.cpp
)
target_link_libraries(inferlab_scheduler_workload_sweep PRIVATE inferlab_native_core)

add_executable(inferlab_scheduler_capacity_sweep
  native_core/src/scheduler_capacity_sweep_main.cpp
  native_core/src/scheduler_capacity_sweep_model.cpp
  native_core/src/scheduler_capacity_sweep_view.cpp
)
target_link_libraries(inferlab_scheduler_capacity_sweep PRIVATE inferlab_native_core)

add_executable(inferlab_scheduler_admission_policy_sweep
  native_core/src/scheduler_admission_policy_sweep_main.cpp
  native_core/src/scheduler_admission_policy_sweep_model.cpp
  native_core/src/scheduler_admission_policy_sweep_view.cpp
)
target_link_libraries(inferlab_scheduler_admission_policy_sweep PRIVATE inferlab_native_core)

add_executable(inferlab_scheduler_runtime_policy_sweep
  native_core/src/scheduler_runtime_policy_sweep_main.cpp
  native_core/src/scheduler_runtime_policy_sweep_model.cpp
  native_core/src/scheduler_runtime_policy_sweep_view.cpp
)
target_link_libraries(inferlab_scheduler_runtime_policy_sweep PRIVATE inferlab_native_core)

add_executable(inferlab_scheduler_fairness_sweep
  native_core/src/scheduler_fairness_sweep_main.cpp
  native_core/src/scheduler_fairness_sweep_model.cpp
  native_core/src/scheduler_fairness_sweep_view.cpp
)
target_link_libraries(inferlab_scheduler_fairness_sweep PRIVATE inferlab_native_core)

add_executable(inferlab_kv_allocation_compare
  native_core/src/kv_allocation_compare_main.cpp
  native_core/src/kv_allocation_compare_model.cpp
  native_core/src/kv_allocation_compare_view.cpp
)
target_link_libraries(inferlab_kv_allocation_compare PRIVATE inferlab_native_core)

add_executable(inferlab_prefix_cache_compare
  native_core/src/prefix_cache_compare_main.cpp
  native_core/src/prefix_cache_compare_model.cpp
  native_core/src/prefix_cache_compare_view.cpp
)
target_link_libraries(inferlab_prefix_cache_compare PRIVATE inferlab_native_core)

add_executable(inferlab_kv_fragmentation_probe
  native_core/src/kv_fragmentation_probe_main.cpp
  native_core/src/kv_fragmentation_probe_model.cpp
  native_core/src/kv_fragmentation_probe_view.cpp
)
target_link_libraries(inferlab_kv_fragmentation_probe PRIVATE inferlab_native_core)

add_executable(inferlab_paged_kv_locality_probe
  native_core/src/paged_kv_locality_probe_main.cpp
  native_core/src/paged_kv_locality_probe_model.cpp
  native_core/src/paged_kv_locality_probe_view.cpp
)
target_link_libraries(inferlab_paged_kv_locality_probe PRIVATE inferlab_native_core)

add_executable(inferlab_quantization_probe
  native_core/src/quantization_probe_main.cpp
  native_core/src/quantization_probe_model.cpp
  native_core/src/quantization_probe_view.cpp
)
target_link_libraries(inferlab_quantization_probe PRIVATE inferlab_native_core)

add_executable(inferlab_quantization_sweep
  native_core/src/quantization_sweep_main.cpp
  native_core/src/quantization_sweep_model.cpp
  native_core/src/quantization_sweep_view.cpp
)
target_link_libraries(inferlab_quantization_sweep PRIVATE inferlab_native_core)

add_executable(inferlab_quantization_channel_compare
  native_core/src/quantization_channel_compare_main.cpp
  native_core/src/quantization_channel_compare_model.cpp
  native_core/src/quantization_channel_compare_view.cpp
)
target_link_libraries(inferlab_quantization_channel_compare PRIVATE inferlab_native_core)

add_executable(inferlab_attention_probe
  native_core/src/attention_probe_main.cpp
  native_core/src/attention_probe_model.cpp
  native_core/src/attention_probe_view.cpp
)
target_link_libraries(inferlab_attention_probe PRIVATE inferlab_native_core)

add_executable(inferlab_matrix_ops_profile
  native_core/src/matrix_ops_profile_main.cpp
  native_core/src/matrix_ops_profile_model.cpp
  native_core/src/matrix_ops_profile_view.cpp
)
target_link_libraries(inferlab_matrix_ops_profile PRIVATE inferlab_native_core)

add_executable(inferlab_decoder_probe
  native_core/src/decoder_probe_main.cpp
  native_core/src/decoder_probe_model.cpp
  native_core/src/decoder_probe_view.cpp
)
target_link_libraries(inferlab_decoder_probe PRIVATE inferlab_native_core)

add_executable(inferlab_decoder_kv_probe
  native_core/src/decoder_kv_probe_main.cpp
  native_core/src/decoder_kv_probe_model.cpp
  native_core/src/decoder_kv_probe_view.cpp
)
target_link_libraries(inferlab_decoder_kv_probe PRIVATE inferlab_native_core)

add_executable(inferlab_gguf_probe
  native_core/src/gguf_probe_main.cpp
  native_core/src/gguf_probe_runner.cpp
)
target_link_libraries(inferlab_gguf_probe PRIVATE inferlab_native_core)

add_executable(inferlab_advanced_kv_router
  native_core/src/advanced_kv_router_main.cpp
  native_core/src/advanced_kv_router_artifact_model.cpp
  native_core/src/advanced_kv_router_artifact_view.cpp
)
target_link_libraries(inferlab_advanced_kv_router PRIVATE inferlab_native_core)

add_executable(inferlab_disaggregated_serving
  native_core/src/disaggregated_serving_main.cpp
  native_core/src/disaggregated_serving_model.cpp
  native_core/src/disaggregated_serving_view.cpp
)
target_link_libraries(inferlab_disaggregated_serving PRIVATE inferlab_native_core)

add_executable(inferlab_slo_capacity_plan
  native_core/src/slo_capacity_plan_main.cpp
  native_core/src/slo_capacity_plan_model.cpp
  native_core/src/slo_capacity_plan_view.cpp
)
target_link_libraries(inferlab_slo_capacity_plan PRIVATE inferlab_native_core)

add_executable(inferlab_long_context_plan
  native_core/src/long_context_plan_main.cpp
  native_core/src/long_context_plan_model.cpp
  native_core/src/long_context_plan_view.cpp
)
target_link_libraries(inferlab_long_context_plan PRIVATE inferlab_native_core)

add_executable(inferlab_speculative_decode_plan
  native_core/src/speculative_decode_plan_main.cpp
  native_core/src/speculative_decode_plan_model.cpp
  native_core/src/speculative_decode_plan_view.cpp
)
target_link_libraries(inferlab_speculative_decode_plan PRIVATE inferlab_native_core)

add_executable(inferlab_observability_evidence
  native_core/src/observability_evidence_main.cpp
  native_core/src/observability_evidence_view.cpp
  native_core/src/observability_slo_plan_model.cpp
  native_core/src/otel_trace_plan_model.cpp
  native_core/src/otel_sampling_budget_plan_model.cpp
  native_core/src/failure_drill_model.cpp
)
target_link_libraries(inferlab_observability_evidence PRIVATE inferlab_native_core)

add_executable(inferlab_streaming_sse_plan
  native_core/src/streaming_sse_plan_main.cpp
  native_core/src/streaming_sse_plan_model.cpp
  native_core/src/streaming_sse_plan_view.cpp
)
target_link_libraries(inferlab_streaming_sse_plan PRIVATE inferlab_native_core)

add_executable(inferlab_edge_runtime_matrix
  native_core/src/edge_runtime_matrix_main.cpp
  native_core/src/edge_runtime_matrix_execution.cpp
  native_core/src/edge_runtime_input_snapshot.cpp
  native_core/src/edge_runtime_matrix_model.cpp
  native_core/src/edge_runtime_quality_comparison.cpp
  native_core/src/edge_runtime_resource_plan.cpp
  native_core/src/edge_runtime_matrix_view.cpp
)
target_link_libraries(inferlab_edge_runtime_matrix PRIVATE inferlab_native_core)

add_executable(inferlab_edge_cloud_routing
  native_core/src/edge_cloud_routing_main.cpp
  native_core/src/edge_cloud_routing_model.cpp
  native_core/src/edge_cloud_routing_session_view.cpp
  native_core/src/edge_cloud_transport.cpp
  native_core/src/edge_cloud_routing_view.cpp
)
target_link_libraries(inferlab_edge_cloud_routing PRIVATE
  inferlab_native_core)
if(INFERLAB_ENABLE_EDGE_CLOUD_CURL AND CURL_FOUND)
  target_link_libraries(inferlab_edge_cloud_routing PRIVATE CURL::libcurl)
  target_compile_definitions(inferlab_edge_cloud_routing PRIVATE
    INFERLAB_HAS_EDGE_CLOUD_CURL=1)
else()
  target_compile_definitions(inferlab_edge_cloud_routing PRIVATE
    INFERLAB_HAS_EDGE_CLOUD_CURL=0)
endif()

add_executable(inferlab_radix_prefix_cache_plan
  native_core/src/radix_prefix_cache_plan_main.cpp
  native_core/src/radix_prefix_cache_plan_model.cpp
  native_core/src/radix_prefix_cache_plan_view.cpp
)
target_link_libraries(inferlab_radix_prefix_cache_plan PRIVATE inferlab_native_core)

add_executable(inferlab_tensor_parallel_comm_plan
  native_core/src/tensor_parallel_comm_plan_main.cpp
  native_core/src/tensor_parallel_comm_plan_model.cpp
  native_core/src/tensor_parallel_comm_plan_view.cpp
)
target_link_libraries(inferlab_tensor_parallel_comm_plan PRIVATE inferlab_native_core)

add_executable(inferlab_kserve_llm_d_plan
  native_core/src/kserve_llm_d_plan_main.cpp
)
target_link_libraries(inferlab_kserve_llm_d_plan PRIVATE inferlab_native_core)

add_executable(inferlab_lora_adapter_cache_plan
  native_core/src/lora_adapter_cache_plan_main.cpp
  native_core/src/lora_adapter_cache_plan_model.cpp
  native_core/src/lora_adapter_cache_plan_view.cpp
)
target_link_libraries(inferlab_lora_adapter_cache_plan PRIVATE inferlab_native_core)

add_executable(inferlab_chunked_prefill_scheduler_plan
  native_core/src/chunked_prefill_scheduler_plan_main.cpp
  native_core/src/chunked_prefill_scheduler_plan_model.cpp
  native_core/src/chunked_prefill_scheduler_plan_view.cpp
)
target_link_libraries(inferlab_chunked_prefill_scheduler_plan PRIVATE inferlab_native_core)

add_executable(inferlab_cost_accounting_plan
  native_core/src/cost_accounting_plan_main.cpp
  native_core/src/cost_accounting_plan_model.cpp
  native_core/src/cost_accounting_plan_view.cpp
)
target_link_libraries(inferlab_cost_accounting_plan PRIVATE inferlab_native_core)

add_executable(inferlab_kv_offload_hierarchy_plan
  native_core/src/kv_offload_hierarchy_budget_profile.cpp
  native_core/src/kv_offload_hierarchy_plan_main.cpp
  native_core/src/kv_offload_hierarchy_plan_view.cpp
)
target_link_libraries(inferlab_kv_offload_hierarchy_plan PRIVATE inferlab_native_core)

inferlab_add_shared_dispatch_group(
  OBJECT_TARGET inferlab_engine_memory_planner_objects
  SOURCE native_core/src/engine_memory_planner_main.cpp
  SOURCES
    native_core/src/engine_memory_planner_model.cpp
    native_core/src/engine_memory_planner_view.cpp
  CANONICAL_TARGET inferlab_engine_memory_planner
  COMPATIBILITY_TARGETS inferlab_engine_memory_plan
)

add_executable(inferlab_gateway_policy_matrix_plan
  native_core/src/gateway_policy_matrix_plan_main.cpp
  native_core/src/gateway_policy_matrix_plan_view.cpp
)
target_link_libraries(inferlab_gateway_policy_matrix_plan PRIVATE inferlab_native_core)

add_executable(inferlab_replica_placement_plan
  native_core/src/replica_placement_plan_main.cpp
  native_core/src/replica_placement_plan_view.cpp
)
target_link_libraries(inferlab_replica_placement_plan PRIVATE inferlab_native_core)

add_executable(inferlab_kv_paged_attention_deep_dive
  native_core/src/kv_paged_attention_deep_dive_main.cpp
)
target_link_libraries(inferlab_kv_paged_attention_deep_dive PRIVATE inferlab_native_core)

add_executable(inferlab_kv_lifecycle_deep_dive
  native_core/src/kv_lifecycle_deep_dive_main.cpp
  native_core/src/kv_lifecycle_deep_dive_model.cpp
  native_core/src/kv_lifecycle_deep_dive_view.cpp
)
target_link_libraries(inferlab_kv_lifecycle_deep_dive PRIVATE inferlab_native_core)

add_executable(inferlab_scheduler_batching_deep_dive
  native_core/src/scheduler_batching_deep_dive_main.cpp
)
target_link_libraries(inferlab_scheduler_batching_deep_dive PRIVATE inferlab_native_core)

add_executable(inferlab_cuda_quant_microkernel_deep_dive
  native_core/src/cuda_quant_microkernel_deep_dive_main.cpp
  native_core/src/cuda_quant_microkernel_deep_dive_model.cpp
  native_core/src/cuda_quant_microkernel_deep_dive_view.cpp
)
target_link_libraries(inferlab_cuda_quant_microkernel_deep_dive PRIVATE inferlab_native_core)

add_executable(inferlab_kv_tier_decision_deep_dive
  native_core/src/kv_tier_decision_deep_dive_main.cpp
)
target_link_libraries(inferlab_kv_tier_decision_deep_dive PRIVATE inferlab_native_core)

add_executable(inferlab_pd_split_crossover_deep_dive
  native_core/src/pd_split_crossover_deep_dive_main.cpp
  native_core/src/pd_split_crossover_deep_dive_model.cpp
  native_core/src/pd_split_crossover_deep_dive_view.cpp
)
target_link_libraries(inferlab_pd_split_crossover_deep_dive PRIVATE inferlab_native_core)

add_executable(inferlab_scheduler_root_cause_deep_dive
  native_core/src/scheduler_root_cause_deep_dive_main.cpp
)
target_link_libraries(inferlab_scheduler_root_cause_deep_dive PRIVATE inferlab_native_core)

add_executable(inferlab_kernel_compatibility_matrix
  native_core/src/kernel_compatibility_matrix_main.cpp
  native_core/src/kernel_compatibility_matrix_model.cpp
  native_core/src/kernel_compatibility_matrix_view.cpp
)
target_link_libraries(inferlab_kernel_compatibility_matrix PRIVATE inferlab_native_core)

add_executable(inferlab_evidence_warehouse_index
  native_core/src/evidence_warehouse_index_main.cpp
  native_core/src/evidence_warehouse_index_cli.cpp
  native_core/src/evidence_warehouse_index_model.cpp
  native_core/src/evidence_warehouse_index_sqlite.cpp
  native_core/src/evidence_warehouse_index_view.cpp
)
target_link_libraries(inferlab_evidence_warehouse_index PRIVATE inferlab_native_core)
inferlab_configure_evidence_sqlite_target(inferlab_evidence_warehouse_index)

add_executable(inferlab_kernel_microbench_harness
  native_core/src/kernel_microbench_harness_main.cpp
  native_core/src/kernel_microbench_harness_cli.cpp
  native_core/src/kernel_microbench_harness_model.cpp
  native_core/src/kernel_microbench_harness_runner.cpp
  native_core/src/kernel_microbench_harness_view.cpp
)
target_link_libraries(inferlab_kernel_microbench_harness PRIVATE inferlab_native_core)
inferlab_configure_kernel_microbench_target(inferlab_kernel_microbench_harness)

add_executable(inferlab_third_party_dependency_gate
  native_core/src/third_party_dependency_gate_main.cpp
)
target_link_libraries(inferlab_third_party_dependency_gate PRIVATE inferlab_native_core)

add_executable(inferlab_release_evidence_pipeline
  native_core/src/release_evidence_pipeline_alignment_projection.cpp
  native_core/src/release_evidence_pipeline_alignment_view.cpp
  native_core/src/release_evidence_pipeline_artifact_body_view.cpp
  native_core/src/release_evidence_pipeline_artifact_projection.cpp
  native_core/src/release_evidence_pipeline_artifact_summary_core_view.cpp
  native_core/src/release_evidence_pipeline_artifact_summary_native_audit_view.cpp
  native_core/src/release_evidence_pipeline_artifact_summary_query_view.cpp
  native_core/src/release_evidence_pipeline_artifact_view.cpp
  native_core/src/release_evidence_pipeline_cli_view.cpp
  native_core/src/release_evidence_pipeline_decision_projection.cpp
  native_core/src/release_evidence_pipeline_decision_view.cpp
  native_core/src/release_evidence_pipeline_evidence_query_closure_navigation_view.cpp
  native_core/src/release_evidence_pipeline_evidence_query_closure_operation_view.cpp
  native_core/src/release_evidence_pipeline_evidence_query_closure_catalog_view.cpp
  native_core/src/release_evidence_pipeline_evidence_query_closure_cross_artifact_view.cpp
  native_core/src/release_evidence_pipeline_evidence_query_closure_query_ux_overview_view.cpp
  native_core/src/release_evidence_pipeline_evidence_query_closure_query_ux_repair_view.cpp
  native_core/src/release_evidence_pipeline_evidence_query_summary_query_ux_view.cpp
  native_core/src/release_evidence_pipeline_evidence_query_catalog_flow_projection.cpp
  native_core/src/release_evidence_pipeline_evidence_query_cross_artifact_projection.cpp
  native_core/src/release_evidence_pipeline_evidence_query_query_ux_projection.cpp
  native_core/src/release_evidence_pipeline_evidence_query_closure_projection.cpp
  native_core/src/release_evidence_pipeline_main.cpp
  native_core/src/release_evidence_pipeline_native_audit_closure_projection.cpp
  native_core/src/release_evidence_pipeline_native_audit_closure_view.cpp
  native_core/src/release_evidence_pipeline_non_gpu_review_view.cpp
  native_core/src/release_evidence_pipeline_repair_builder.cpp
  native_core/src/release_evidence_pipeline_repair_evidence_query_producer.cpp
  native_core/src/release_evidence_pipeline_repair_group_projection.cpp
  native_core/src/release_evidence_pipeline_repair_group_view.cpp
  native_core/src/release_evidence_pipeline_repair_link_view.cpp
  native_core/src/release_evidence_pipeline_repair_metrics.cpp
  native_core/src/release_evidence_pipeline_repair_native_audit_producer.cpp
  native_core/src/release_evidence_pipeline_repair_path_view.cpp
  native_core/src/release_evidence_pipeline_repair_projection.cpp
  native_core/src/release_evidence_pipeline_repair_source_policy.cpp
  native_core/src/release_evidence_pipeline_root_cause_projection.cpp
  native_core/src/release_evidence_pipeline_request.cpp
  native_core/src/release_evidence_pipeline_source_context.cpp
  native_core/src/release_evidence_pipeline_snapshot_diff_link_projection.cpp
  native_core/src/release_evidence_pipeline_stage_view.cpp
  native_core/src/release_lora_prefix_isolation_alignment_contract.cpp
  native_core/src/release_deployment_resource_alignment_contract.cpp
  native_core/src/release_kv_budget_alignment_contract.cpp
  native_core/src/release_traffic_alignment_contract.cpp
  native_core/src/repository_evidence_dag_composition.cpp
  native_core/src/repository_evidence_dag_contract.cpp
  native_core/src/repository_evidence_dag_view.cpp
)
target_link_libraries(inferlab_release_evidence_pipeline PRIVATE inferlab_native_core)

inferlab_add_shared_dispatch_group(
  OBJECT_TARGET inferlab_evidence_query_objects
  SOURCE native_core/src/evidence_query_main.cpp
  SOURCES
    native_core/src/evidence_warehouse_index_model.cpp
    native_core/src/evidence_query_operation_contract_view.cpp
    native_core/src/evidence_query_source_projection.cpp
    native_core/src/evidence_query_native_trace_projection.cpp
    native_core/src/evidence_query_request.cpp
    native_core/src/evidence_query_artifact_support_view.cpp
    native_core/src/evidence_query_result_artifact_projection.cpp
    native_core/src/evidence_query_result_artifact_summary_core_view.cpp
    native_core/src/evidence_query_result_artifact_summary_query_ux_view.cpp
    native_core/src/evidence_query_result_artifact_summary_catalog_review_view.cpp
    native_core/src/evidence_query_result_artifact_summary_native_audit_core_view.cpp
    native_core/src/evidence_query_result_artifact_summary_snapshot_view.cpp
    native_core/src/evidence_query_result_artifact_body_view.cpp
    native_core/src/evidence_query_result_artifact_view.cpp
    native_core/src/evidence_query_repair_path_query_view.cpp
    native_core/src/evidence_query_selector_export_artifact_view.cpp
    native_core/src/evidence_query_text_view.cpp
    native_core/src/evidence_query_text_overview_view.cpp
    native_core/src/evidence_query_text_filter_view.cpp
    native_core/src/evidence_query_text_native_audit_view.cpp
    native_core/src/evidence_query_text_repair_view.cpp
    native_core/src/evidence_query_catalog_filter_view.cpp
    native_core/src/evidence_query_native_audit_filter_view.cpp
    native_core/src/deep_dive_trace_time_window.cpp
    native_core/src/deep_dive_trace_token_join.cpp
    native_core/src/deep_dive_trace_token_join_policy.cpp
    native_core/src/deep_dive_trace_token_join_validation.cpp
    native_core/src/deep_dive_trace_token_join_view.cpp
    native_core/src/deep_dive_trace_span_lineage.cpp
    native_core/src/deep_dive_trace_span_lifecycle.cpp
  CANONICAL_TARGET inferlab_evidence_query
  COMPATIBILITY_TARGETS inferlab_evidence_query_selector_repair_card_export
)

add_executable(inferlab_dependency_sbom_gate
  native_core/src/dependency_sbom_gate_main.cpp
)
target_link_libraries(inferlab_dependency_sbom_gate PRIVATE inferlab_native_core)

add_executable(inferlab_artifact_schema_registry
  native_core/src/artifact_schema_registry_main.cpp
  native_core/src/artifact_schema_registry_model.cpp
  native_core/src/artifact_schema_registry_view.cpp
)
target_link_libraries(inferlab_artifact_schema_registry PRIVATE inferlab_native_core)

add_executable(inferlab_artifact_catalog_single_source
  native_core/src/artifact_catalog_single_source_main.cpp
  native_core/src/artifact_catalog_single_source_model.cpp
  native_core/src/artifact_catalog_single_source_registry_view.cpp
  native_core/src/artifact_catalog_single_source_surface_view.cpp
  native_core/src/artifact_catalog_single_source_fallback_view.cpp
  native_core/src/artifact_catalog_single_source_artifact_view.cpp
)
target_link_libraries(inferlab_artifact_catalog_single_source PRIVATE inferlab_native_core)

add_executable(inferlab_native_core_audit_register
  native_core/src/native_core_audit_register_artifact_view.cpp
  native_core/src/native_core_audit_register_file_policy.cpp
  native_core/src/native_core_audit_register_file_view.cpp
  native_core/src/native_core_audit_register_input_view.cpp
  native_core/src/native_core_audit_register_manifest.cpp
  native_core/src/native_core_audit_register_main.cpp
  native_core/src/native_core_audit_register_request.cpp
  native_core/src/native_core_audit_register_source.cpp
  native_core/src/native_core_audit_register_summary_projection.cpp
  native_core/src/native_core_audit_register_summary_view.cpp
)
target_link_libraries(inferlab_native_core_audit_register PRIVATE inferlab_native_core)

add_executable(inferlab_product_replay_semantic_events
  native_core/src/product_replay_semantic_events_main.cpp
  native_core/src/product_replay_semantic_events_fixture.cpp
  native_core/src/product_replay_semantic_events_sample_parser.cpp
  native_core/src/product_replay_request_trace_model.cpp
  native_core/src/product_replay_request_trace_view.cpp
  native_core/src/product_replay_semantic_events_model.cpp
  native_core/src/product_replay_semantic_events_view.cpp
)
target_link_libraries(inferlab_product_replay_semantic_events PRIVATE inferlab_native_core)

# Canonical mature-product runners select an artifact with --artifact. The old
# artifact-named executables remain thin compatibility dispatch names until
# target-machine parity and external caller migration are complete.
inferlab_add_shared_dispatch_group(
  OBJECT_TARGET inferlab_mature_product_p2_artifacts_objects
  SOURCE native_core/src/mature_product_p2_artifacts_main.cpp
  CANONICAL_TARGET inferlab_mature_product_p2_artifacts
  COMPATIBILITY_TARGETS
    inferlab_streaming_backpressure_deep_dive
    inferlab_structured_output_parser_contract
    inferlab_autoscaler_cost_slo_deep_dive
    inferlab_lora_adapter_cache_deep_dive
    inferlab_grammar_backend_matrix
    inferlab_edge_compiler_artifact_matrix
)

inferlab_add_shared_dispatch_group(
  OBJECT_TARGET inferlab_mature_product_borrowed_gap_artifacts_objects
  SOURCE native_core/src/mature_product_borrowed_gap_artifacts_main.cpp
  CANONICAL_TARGET inferlab_mature_product_borrowed_gap_artifacts
  COMPATIBILITY_TARGETS
    inferlab_kv_transport_timeline_deep_dive
    inferlab_radix_prefix_cache_deep_dive
    inferlab_kv_compression_blend_risk
    inferlab_chunked_prefill_stall_free_deep_dive
    inferlab_tenant_quota_admission_deep_dive
    inferlab_live_request_migration_deep_dive
    inferlab_quant_correctness_contract
    inferlab_paged_kv_gather_kernel_contract
    inferlab_flashinfer_replay_alignment
    inferlab_trtllm_engine_quant_alignment
    inferlab_gateway_policy_matrix
    inferlab_deployment_readiness_matrix
    inferlab_openai_api_conformance_matrix
)

get_property(inferlab_compatibility_manifest_entries GLOBAL PROPERTY
  INFERLAB_COMPATIBILITY_MANIFEST_ENTRIES)
if(inferlab_compatibility_manifest_entries)
  list(JOIN inferlab_compatibility_manifest_entries ",\n    "
    inferlab_compatibility_manifest_body)
  file(GENERATE
    OUTPUT "${CMAKE_BINARY_DIR}/inferlab_compatibility_manifest.json"
    CONTENT "{\n  \"schema_version\": 3,\n  \"entries\": [\n    ${inferlab_compatibility_manifest_body}\n  ]\n}\n")
endif()

add_executable(inferlab_product_replay_alignment
  native_core/src/product_replay_alignment_main.cpp
  native_core/src/product_replay_alignment_model.cpp
  native_core/src/product_replay_alignment_view.cpp
)
target_link_libraries(inferlab_product_replay_alignment PRIVATE inferlab_native_core)

add_executable(inferlab_deep_dive_contract_check
  native_core/src/deep_dive_contract_check_main.cpp
  native_core/src/deep_dive_contract_check_view.cpp
)
target_link_libraries(inferlab_deep_dive_contract_check PRIVATE inferlab_native_core)

add_executable(inferlab_product_profile_contract_check
  native_core/src/product_profile_contract_check_main.cpp
  native_core/src/product_profile_contract_check_model.cpp
  native_core/src/product_profile_contract_check_view.cpp
)
target_link_libraries(inferlab_product_profile_contract_check PRIVATE inferlab_native_core)

add_executable(inferlab_product_integration_inventory
  native_core/src/product_integration_inventory_main.cpp
  native_core/src/product_integration_inventory_model.cpp
  native_core/src/product_integration_inventory_view.cpp
)
target_link_libraries(inferlab_product_integration_inventory PRIVATE inferlab_native_core)

add_executable(inferlab_claim_scope_contract
  native_core/src/claim_scope_contract_main.cpp
  native_core/src/claim_scope_contract_model.cpp
  native_core/src/claim_scope_contract_view.cpp
)
target_link_libraries(inferlab_claim_scope_contract PRIVATE inferlab_native_core)

add_executable(inferlab_runtime_evidence_freshness_gate
  native_core/src/runtime_evidence_freshness_gate_main.cpp
  native_core/src/runtime_evidence_freshness_gate_view.cpp
)
target_link_libraries(inferlab_runtime_evidence_freshness_gate PRIVATE inferlab_native_core)

add_executable(kv_cache_test
  native_core/tests/kv_cache_test.cpp
)
target_link_libraries(kv_cache_test PRIVATE inferlab_native_core)
add_test(NAME kv_cache_test COMMAND kv_cache_test)

add_executable(shape_trace_test
  native_core/tests/shape_trace_test.cpp
)
target_link_libraries(shape_trace_test PRIVATE inferlab_native_core)
add_test(NAME shape_trace_test COMMAND shape_trace_test)

add_executable(paged_kv_cache_test
  native_core/tests/paged_kv_cache_test.cpp
)
target_link_libraries(paged_kv_cache_test PRIVATE inferlab_native_core)
add_test(NAME paged_kv_cache_test COMMAND paged_kv_cache_test)

add_executable(paged_attention_test
  native_core/tests/paged_attention_test.cpp
)
target_link_libraries(paged_attention_test PRIVATE inferlab_native_core)
add_test(NAME paged_attention_test COMMAND paged_attention_test)

add_executable(paged_kv_locality_test
  native_core/tests/paged_kv_locality_test.cpp
)
target_link_libraries(paged_kv_locality_test PRIVATE inferlab_native_core)
add_test(NAME paged_kv_locality_test COMMAND paged_kv_locality_test)

add_executable(continuous_batch_scheduler_test
  native_core/tests/continuous_batch_scheduler_test.cpp
)
target_link_libraries(continuous_batch_scheduler_test PRIVATE inferlab_native_core)
add_test(NAME continuous_batch_scheduler_test COMMAND continuous_batch_scheduler_test)

add_executable(static_kv_cache_test
  native_core/tests/static_kv_cache_test.cpp
)
target_link_libraries(static_kv_cache_test PRIVATE inferlab_native_core)
add_test(NAME static_kv_cache_test COMMAND static_kv_cache_test)

add_executable(quantization_test
  native_core/tests/quantization_test.cpp
)
target_link_libraries(quantization_test PRIVATE inferlab_native_core)
add_test(NAME quantization_test COMMAND quantization_test)

add_executable(vector_add_test
  native_core/tests/vector_add_test.cpp
)
target_link_libraries(vector_add_test PRIVATE inferlab_native_core)
add_test(NAME vector_add_test COMMAND vector_add_test)

add_executable(matrix_ops_test
  native_core/tests/matrix_ops_test.cpp
)
target_link_libraries(matrix_ops_test PRIVATE inferlab_native_core)
add_test(NAME matrix_ops_test COMMAND matrix_ops_test)

add_executable(attention_test
  native_core/tests/attention_test.cpp
)
target_link_libraries(attention_test PRIVATE inferlab_native_core)
add_test(NAME attention_test COMMAND attention_test)

add_executable(normalization_test
  native_core/tests/normalization_test.cpp
)
target_link_libraries(normalization_test PRIVATE inferlab_native_core)
add_test(NAME normalization_test COMMAND normalization_test)

add_executable(rope_test
  native_core/tests/rope_test.cpp
)
target_link_libraries(rope_test PRIVATE inferlab_native_core)
add_test(NAME rope_test COMMAND rope_test)

add_executable(decoder_block_test
  native_core/tests/decoder_block_test.cpp
)
target_link_libraries(decoder_block_test PRIVATE inferlab_native_core)
add_test(NAME decoder_block_test COMMAND decoder_block_test)

add_executable(decoder_kv_test
  native_core/tests/decoder_kv_test.cpp
)
target_link_libraries(decoder_kv_test PRIVATE inferlab_native_core)
add_test(NAME decoder_kv_test COMMAND decoder_kv_test)

add_executable(gguf_test
  native_core/tests/gguf_test.cpp
)
target_link_libraries(gguf_test PRIVATE inferlab_native_core)
add_test(NAME gguf_test COMMAND gguf_test)

if(INFERLAB_ENABLE_CUDA)
  enable_language(CUDA)
  add_library(inferlab_cuda_kernels
    cuda_kernels/attention_runtime.cu
    cuda_kernels/matrix_ops_runtime.cu
    cuda_kernels/normalization_runtime.cu
    cuda_kernels/paged_kv_runtime.cu
    cuda_kernels/quantization_runtime.cu
    cuda_kernels/rope_runtime.cu
    cuda_kernels/softmax.cu
    cuda_kernels/vector_add.cu
    cuda_kernels/vector_add_runtime.cu
  )
  set_target_properties(inferlab_cuda_kernels PROPERTIES
    CUDA_STANDARD 17
    CUDA_STANDARD_REQUIRED ON
  )
  target_include_directories(inferlab_cuda_kernels
    PUBLIC
      ${CMAKE_CURRENT_SOURCE_DIR}/cuda_kernels
  )
  target_link_libraries(inferlab_cuda_kernels PUBLIC inferlab_native_core)

  target_link_libraries(inferlab_kernel_microbench_harness
    PRIVATE inferlab_cuda_kernels
  )

  add_executable(vector_add_cuda_test
    cuda_kernels/vector_add_cuda_test.cpp
  )
  target_link_libraries(vector_add_cuda_test
    PRIVATE
      inferlab_cuda_kernels
      inferlab_native_core
  )
  add_test(NAME vector_add_cuda_test COMMAND vector_add_cuda_test)

  add_executable(matrix_ops_cuda_test
    cuda_kernels/matrix_ops_cuda_test.cpp
  )
  target_link_libraries(matrix_ops_cuda_test
    PRIVATE
      inferlab_cuda_kernels
      inferlab_native_core
  )
  add_test(NAME matrix_ops_cuda_test COMMAND matrix_ops_cuda_test)

  add_executable(normalization_cuda_test
    cuda_kernels/normalization_cuda_test.cpp
  )
  target_link_libraries(normalization_cuda_test
    PRIVATE
      inferlab_cuda_kernels
      inferlab_native_core
  )
  add_test(NAME normalization_cuda_test COMMAND normalization_cuda_test)

  add_executable(rope_cuda_test
    cuda_kernels/rope_cuda_test.cpp
  )
  target_link_libraries(rope_cuda_test
    PRIVATE
      inferlab_cuda_kernels
      inferlab_native_core
  )
  add_test(NAME rope_cuda_test COMMAND rope_cuda_test)

  add_executable(attention_cuda_test
    cuda_kernels/attention_cuda_test.cpp
  )
  target_link_libraries(attention_cuda_test
    PRIVATE
      inferlab_cuda_kernels
      inferlab_native_core
  )
  add_test(NAME attention_cuda_test COMMAND attention_cuda_test)

  add_executable(quantization_cuda_test
    cuda_kernels/quantization_cuda_test.cpp
  )
  target_link_libraries(quantization_cuda_test
    PRIVATE
      inferlab_cuda_kernels
      inferlab_native_core
  )
  add_test(NAME quantization_cuda_test COMMAND quantization_cuda_test)

  add_executable(paged_kv_cuda_test
    cuda_kernels/paged_kv_cuda_test.cpp
  )
  target_link_libraries(paged_kv_cuda_test
    PRIVATE
      inferlab_cuda_kernels
      inferlab_native_core
  )
  add_test(NAME paged_kv_cuda_test COMMAND paged_kv_cuda_test)
endif()
