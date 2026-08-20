#pragma once

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab::mature_product_profiles {

enum class ProductProfileScope {
  default_required,
  optional_on_demand,
};

struct ProductProfileSpec {
  std::string id;
  std::string display_name;
  std::string category;
  std::string report;
  std::string artifact;
  std::string benchmark_entry;
  ProductProfileScope scope = ProductProfileScope::optional_on_demand;
};

struct ProductProfileProjection {
  std::vector<std::string> ids;
  std::vector<std::string> display_names;
  std::vector<std::string> artifact_paths;
  std::vector<std::string> replay_commands;
};

inline std::string profile_module(const ProductProfileSpec& spec) {
  return "scripts.product_profiles.profile_" + spec.id;
}

inline std::string profile_source_file(const ProductProfileSpec& spec) {
  return "scripts/product_profiles/profile_" + spec.id + ".py";
}

inline std::string profile_config_path(const ProductProfileSpec& spec) {
  return "configs/product_profiles/" + spec.id + ".example.yaml";
}

inline std::string profile_output_path(const ProductProfileSpec& spec) {
  return "results/raw/" + spec.artifact;
}

inline std::string profile_command_prefix(const ProductProfileSpec& spec) {
  return "python3 -m " + profile_module(spec);
}

inline std::string profile_command_base(const ProductProfileSpec& spec) {
  return profile_command_prefix(spec) +
         " --repo-root . --config " + profile_config_path(spec) +
         " --output " + profile_output_path(spec) + " --report " +
         spec.report;
}

inline std::string profile_dry_run_command(const ProductProfileSpec& spec) {
  return profile_command_base(spec) + " --dry-run";
}

inline std::string profile_execute_command(const ProductProfileSpec& spec) {
  return profile_command_base(spec) + " --execute";
}

inline std::string profile_replay_command(const ProductProfileSpec& spec) {
  return profile_command_base(spec) +
         " --replay --evidence-dir profiling/" + spec.id;
}

inline const char* profile_scope_name(const ProductProfileScope scope) {
  switch (scope) {
    case ProductProfileScope::default_required:
      return "default_required";
    case ProductProfileScope::optional_on_demand:
      return "optional_on_demand";
  }
  throw std::invalid_argument("unknown mature product profile scope");
}

inline bool profile_is_default_required(const ProductProfileSpec& spec) {
  return spec.scope == ProductProfileScope::default_required;
}

inline void validate_product_profile_registry(
    const std::vector<ProductProfileSpec>& products) {
  if (products.empty()) {
    throw std::logic_error(
        "mature product profile registry must not be empty");
  }
  const auto valid_id = [](const std::string& id) {
    if (id.empty()) return false;
    if (!std::islower(static_cast<unsigned char>(id.front()))) return false;
    for (const char value : id) {
      const auto byte = static_cast<unsigned char>(value);
      if (!std::islower(byte) && !std::isdigit(byte) && value != '_') {
        return false;
      }
    }
    return true;
  };
  const auto valid_artifact = [](const std::string& artifact) {
    return artifact.size() > 5U &&
           artifact.compare(artifact.size() - 5U, 5U, ".json") == 0 &&
           artifact.find('/') == std::string::npos &&
           artifact.find('\\') == std::string::npos &&
           artifact.find("..") == std::string::npos;
  };
  const auto valid_report = [](const std::string& report) {
    return report.rfind("reports/", 0) == 0 && report.size() > 11U &&
           report.compare(report.size() - 3U, 3U, ".md") == 0 &&
           report.find("..") == std::string::npos &&
           report.find('\\') == std::string::npos;
  };
  std::size_t default_required_count = 0U;
  std::size_t optional_count = 0U;
  for (std::size_t index = 0; index < products.size(); ++index) {
    const auto& product = products[index];
    if (!valid_id(product.id) || !valid_id(product.benchmark_entry) ||
        product.display_name.empty() ||
        product.category.empty() || !valid_report(product.report) ||
        !valid_artifact(product.artifact)) {
      throw std::logic_error(
          "mature product profile registry fields are invalid");
    }
    if (profile_is_default_required(product)) {
      ++default_required_count;
    } else {
      ++optional_count;
    }
    for (std::size_t prior = 0; prior < index; ++prior) {
      if (products[prior].id == product.id) {
        throw std::logic_error(
            "duplicate mature product profile id: " + product.id);
      }
      if (products[prior].artifact == product.artifact) {
        throw std::logic_error(
            "duplicate mature product profile artifact: " +
            product.artifact);
      }
      if (products[prior].benchmark_entry == product.benchmark_entry) {
        throw std::logic_error(
            "duplicate mature product benchmark entry: " +
            product.benchmark_entry);
      }
    }
  }
  if (default_required_count == 0U || optional_count == 0U) {
    throw std::logic_error(
        "mature product profile registry scope partition is incomplete");
  }
}

inline const std::vector<ProductProfileSpec>& product_profiles() {
  static const std::vector<ProductProfileSpec> products = {
      {"vllm", "vLLM", "serving_engine",
       "reports/49_product_vllm_profile.md",
       "product_vllm_capability_profile.json",
       "product_vllm_capability_profile",
       ProductProfileScope::default_required},
      {"sglang", "SGLang", "serving_engine",
       "reports/50_product_sglang_profile.md",
       "product_sglang_capability_profile.json",
       "product_sglang_capability_profile",
       ProductProfileScope::default_required},
      {"kv_offload", "LMCache / Ray KV Offload / Mooncake",
       "kv_cache_system",
       "reports/51_product_kv_offload_profile.md",
       "product_lmcache_kv_offload_profile.json",
       "product_kv_offload_profile",
       ProductProfileScope::optional_on_demand},
      {"mooncake", "Mooncake", "kv_cache_system",
       "reports/51_product_kv_offload_profile.md",
       "product_mooncake_kv_profile.json",
       "product_mooncake_kv_profile",
       ProductProfileScope::optional_on_demand},
      {"tensorrtllm", "TensorRT-LLM / Triton", "runtime_backend",
       "reports/52_product_runtime_backend_profile.md",
       "product_tensorrtllm_runtime_profile.json",
       "product_tensorrtllm_runtime_profile",
       ProductProfileScope::default_required},
      {"lmdeploy", "LMDeploy", "runtime_backend",
       "reports/52_product_runtime_backend_profile.md",
       "product_lmdeploy_runtime_profile.json",
       "product_lmdeploy_runtime_profile",
       ProductProfileScope::optional_on_demand},
      {"flashinfer", "FlashInfer", "kernel_library",
       "reports/53_product_kernel_library_profile.md",
       "product_flashinfer_kernel_profile.json",
       "product_flashinfer_kernel_profile",
       ProductProfileScope::optional_on_demand},
      {"ktransformers", "KTransformers", "hybrid_runtime",
       "reports/53_product_kernel_library_profile.md",
       "product_ktransformers_hybrid_profile.json",
       "product_ktransformers_hybrid_profile",
       ProductProfileScope::optional_on_demand},
      {"dynamo", "NVIDIA Dynamo", "cloud_control_plane",
       "reports/54_product_cloud_control_plane_profile.md",
       "product_dynamo_serving_profile.json",
       "product_dynamo_serving_profile",
       ProductProfileScope::optional_on_demand},
      {"kserve_llmd", "KServe / llm-d", "cloud_control_plane",
       "reports/54_product_cloud_control_plane_profile.md",
       "product_kserve_llmd_profile.json",
       "product_kserve_llmd_profile",
       ProductProfileScope::optional_on_demand},
      {"ray_serve", "Ray Serve LLM", "cloud_control_plane",
       "reports/54_product_cloud_control_plane_profile.md",
       "product_ray_serve_profile.json",
       "product_ray_serve_profile",
       ProductProfileScope::optional_on_demand},
      {"tgi", "Hugging Face TGI", "serving_engine",
       "reports/52_product_runtime_backend_profile.md",
       "product_tgi_profile.json",
       "product_tgi_profile",
       ProductProfileScope::optional_on_demand},
  };
  static const bool validated = [] {
    validate_product_profile_registry(products);
    return true;
  }();
  (void)validated;
  return products;
}

inline std::size_t product_profile_count_for_scope(
    const ProductProfileScope scope) {
  const auto& products = product_profiles();
  return static_cast<std::size_t>(std::count_if(
      products.begin(), products.end(),
      [scope](const ProductProfileSpec& product) {
        return product.scope == scope;
      }));
}

inline const ProductProfileSpec* find_product_profile(
    const std::string& id) {
  const auto& products = product_profiles();
  const ProductProfileSpec* match = nullptr;
  for (const auto& product : products) {
    if (product.id != id) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "ambiguous mature product profile id: " + id);
    }
    match = &product;
  }
  return match;
}

inline const ProductProfileSpec& require_product_profile(
    const std::string& id) {
  const ProductProfileSpec* match = find_product_profile(id);
  if (match == nullptr) {
    throw std::out_of_range("unknown mature product profile id: " + id);
  }
  return *match;
}

inline ProductProfileProjection project_product_profiles(
    const std::vector<std::string>& ids) {
  if (ids.empty()) {
    throw std::invalid_argument(
        "mature product profile projection requires ids");
  }
  ProductProfileProjection projection;
  projection.ids.reserve(ids.size());
  projection.display_names.reserve(ids.size());
  projection.artifact_paths.reserve(ids.size());
  projection.replay_commands.reserve(ids.size());
  for (const auto& id : ids) {
    if (std::find(projection.ids.begin(), projection.ids.end(), id) !=
        projection.ids.end()) {
      continue;
    }
    const auto& profile = require_product_profile(id);
    projection.ids.push_back(profile.id);
    projection.display_names.push_back(profile.display_name);
    projection.artifact_paths.push_back(profile_output_path(profile));
    projection.replay_commands.push_back(profile_replay_command(profile));
  }
  if (projection.ids.empty() ||
      projection.ids.size() != projection.display_names.size() ||
      projection.ids.size() != projection.artifact_paths.size() ||
      projection.ids.size() != projection.replay_commands.size()) {
    throw std::logic_error(
        "mature product profile projection reconciliation failed");
  }
  return projection;
}

}  // namespace inferlab::mature_product_profiles
