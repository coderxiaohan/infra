#pragma once

#include "mature_product_artifact_dispatch.hpp"
#include "plan_json_utils.hpp"

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace inferlab_mature_product {

struct Field {
  std::string key;
  std::string value;
  bool quoted = false;
};

struct Record {
  std::string scenario;
  std::string product_alignment;
  std::string native_alignment;
  std::string decision;
  std::string claim_boundary;
  std::vector<Field> fields;
};

struct ArtifactSpec {
  std::string artifact_type;
  std::string claim_scope;
  std::vector<std::string> research_basis;
  std::vector<std::string> product_alignment;
  std::vector<Record> records;
};

inline std::string number(const double value) {
  return inferlab_plan::fixed6(value);
}

inline Field num(const std::string& key, const double value) {
  return {key, number(value), false};
}

inline Field str(const std::string& key, const std::string& value) {
  return {key, value, true};
}

inline Field boolean(const std::string& key, const bool value) {
  return {key, inferlab_plan::json_bool(value), false};
}

inline bool valid_unquoted_json_scalar(const std::string& value) {
  if (value == "true" || value == "false" || value == "null") return true;
  try {
    std::size_t consumed = 0;
    const double parsed = std::stod(value, &consumed);
    return consumed == value.size() && std::isfinite(parsed);
  } catch (...) {
    return false;
  }
}

inline void validate_artifact_spec(const ArtifactSpec& spec,
                                   const ArtifactFamily family) {
  const ArtifactDispatchSpec* registered = find_exact(spec.artifact_type);
  if (registered == nullptr || registered->family != family) {
    throw std::invalid_argument(
        "mature-product artifact spec is not registered in its dispatcher");
  }
  if (spec.claim_scope.empty() || spec.research_basis.empty() ||
      spec.product_alignment.empty() || spec.records.empty()) {
    throw std::invalid_argument(
        "mature-product artifact spec is missing required evidence metadata");
  }
  for (std::size_t i = 0; i < spec.records.size(); ++i) {
    const Record& record = spec.records[i];
    if (record.scenario.empty() || record.product_alignment.empty() ||
        record.native_alignment.empty() || record.decision.empty() ||
        record.claim_boundary.empty() || record.fields.empty()) {
      throw std::invalid_argument(
          "mature-product artifact record is missing required fields");
    }
    for (std::size_t j = 0; j < i; ++j) {
      if (record.scenario == spec.records[j].scenario) {
        throw std::invalid_argument(
            "mature-product artifact scenarios must be unique");
      }
    }
    for (std::size_t field_index = 0; field_index < record.fields.size();
         ++field_index) {
      const Field& field = record.fields[field_index];
      if (field.key.empty() ||
          (!field.quoted && !valid_unquoted_json_scalar(field.value))) {
        throw std::invalid_argument(
            "mature-product artifact metric is not a valid JSON scalar");
      }
      for (std::size_t previous = 0; previous < field_index; ++previous) {
        if (field.key == record.fields[previous].key) {
          throw std::invalid_argument(
              "mature-product artifact metric keys must be unique per record");
        }
      }
    }
  }
}

inline std::string field_json(const Field& field, const int indent) {
  const std::string pad(indent, ' ');
  std::ostringstream out;
  out << pad << inferlab_plan::json_string(field.key) << ": ";
  out << (field.quoted ? inferlab_plan::json_string(field.value) : field.value);
  return out.str();
}

inline std::string fields_json(const std::vector<Field>& fields,
                               const int indent) {
  const std::string pad(indent, ' ');
  std::ostringstream out;
  out << "{\n";
  for (std::size_t i = 0; i < fields.size(); ++i) {
    out << field_json(fields[i], indent + 2)
        << (i + 1 == fields.size() ? "\n" : ",\n");
  }
  out << pad << "}";
  return out.str();
}

inline std::string record_json(const Record& record, const int indent) {
  const std::string pad(indent, ' ');
  const std::string nested(indent + 2, ' ');
  std::ostringstream out;
  out << pad << "{\n";
  out << nested << "\"claim_boundary\": "
      << inferlab_plan::json_string(record.claim_boundary) << ",\n";
  out << nested << "\"decision\": "
      << inferlab_plan::json_string(record.decision) << ",\n";
  out << nested << "\"metrics\": " << fields_json(record.fields, indent + 2)
      << ",\n";
  out << nested << "\"native_alignment\": "
      << inferlab_plan::json_string(record.native_alignment) << ",\n";
  out << nested << "\"product_alignment\": "
      << inferlab_plan::json_string(record.product_alignment) << ",\n";
  out << nested << "\"scenario\": "
      << inferlab_plan::json_string(record.scenario) << "\n";
  out << pad << "}";
  return out.str();
}

inline std::string render_artifact(const ArtifactSpec& spec,
                                   const ArtifactFamily family) {
  validate_artifact_spec(spec, family);
  const std::string command = refresh_command(spec.artifact_type);
  const std::string artifact_path =
      "results/raw/" + spec.artifact_type + ".json";
  const std::string representative_scenario = spec.records.front().scenario;
  std::vector<std::string> evidence_bridge_inputs;
  evidence_bridge_inputs.reserve(spec.records.size());
  for (const auto& record : spec.records) {
    inferlab_plan::append_unique_nonempty_string(evidence_bridge_inputs,
                                                 record.native_alignment);
  }

  std::ostringstream out;
  out << "{\n";
  inferlab_plan::write_common_dry_run_metadata(
      out, spec.artifact_type, spec.claim_scope, spec.product_alignment,
      std::string(canonical_target(family)), artifact_path, command);
  out << "  \"schema_version\": 1,\n";
  out << "  \"status\": \"ok\",\n";
  out << "  \"executed\": false,\n";
  out << "  \"replayed\": false,\n";
  out << "  \"artifact_type\": "
      << inferlab_plan::json_string(spec.artifact_type) << ",\n";
  out << "  \"claim_scope\": "
      << inferlab_plan::json_string(spec.claim_scope) << ",\n";
  out << "  \"deep_dive_type\": "
      << inferlab_plan::json_string(spec.artifact_type) << ",\n";
  out << "  \"research_basis\": "
      << inferlab_plan::json_string_array(spec.research_basis, 2) << ",\n";
  out << "  \"product_alignment\": "
      << inferlab_plan::json_string_array(spec.product_alignment, 2) << ",\n";
  out << "  \"evidence_bridge_inputs\": "
      << inferlab_plan::json_string_array(evidence_bridge_inputs, 2)
      << ",\n";
  out << "  \"summary\": {\n";
  out << "    \"record_count\": " << spec.records.size() << ",\n";
  out << "    \"representative_scenario\": "
      << inferlab_plan::json_string(representative_scenario) << "\n";
  out << "  },\n";
  out << "  \"records\": [\n";
  for (std::size_t i = 0; i < spec.records.size(); ++i) {
    out << record_json(spec.records[i], 4)
        << (i + 1 == spec.records.size() ? "\n" : ",\n");
  }
  out << "  ],\n";
  out << "  \"recommended_next_commands\": "
      << inferlab_plan::json_string_array({command}, 2) << "\n";
  out << "}\n";
  return out.str();
}

inline const ArtifactDispatchSpec& resolve_dispatch_request(
    const std::string_view program,
    const std::string_view explicit_artifact,
    const ArtifactFamily family) {
  const std::string_view invocation = selector_basename(program);
  if (invocation == canonical_target(family)) {
    if (explicit_artifact.empty()) {
      throw std::invalid_argument(
          "--artifact is required when using a canonical mature-product "
          "dispatcher");
    }
    return resolve_selector(explicit_artifact, family);
  }

  const ArtifactDispatchSpec& compatibility =
      resolve_selector(invocation, family);
  if (explicit_artifact.empty()) return compatibility;
  const ArtifactDispatchSpec& requested =
      resolve_selector(explicit_artifact, family);
  if (requested.artifact_type != compatibility.artifact_type) {
    throw std::invalid_argument(
        "a mature-product compatibility target cannot dispatch a different "
        "artifact; use the canonical target with --artifact");
  }
  return compatibility;
}

template <typename SpecResolver>
int run_artifact_dispatcher(const int argc,
                            char** argv,
                            const ArtifactFamily family,
                            SpecResolver&& resolve_spec) {
  const char* program = inferlab_plan::cli_program_name(
      argc, argv, canonical_target(family).data());
  std::string repo_root_value = ".";
  std::string output_path;
  try {
    const auto& cli_options =
        inferlab_plan::repo_root_artifact_output_cli_spec();
    if (inferlab_plan::handle_cli_help(argc, argv, program, cli_options)) {
      return 0;
    }
    inferlab_plan::validate_cli_args(argc, argv, cli_options);
    const auto option_value = [&](const std::string& name,
                                  const std::string& fallback) {
      return inferlab_plan::arg_value_with_spec(argc, argv, name, fallback,
                                                cli_options);
    };
    repo_root_value = option_value("--repo-root", ".");
    const std::string artifact_option = option_value("--artifact", "");
    const ArtifactDispatchSpec& selected =
        resolve_dispatch_request(program, artifact_option, family);
    const std::filesystem::path output_arg = option_value("--output", "");
    output_path = inferlab_plan::resolve_optional_repo_relative_path(
                      std::filesystem::path(repo_root_value), output_arg)
                      .generic_string();

    const ArtifactSpec spec =
        resolve_spec(std::string(selected.artifact_type));
    inferlab_plan::write_or_print(output_path, render_artifact(spec, family));
  } catch (const std::exception& error) {
    inferlab_plan::report_cli_error(
        error, "mature-product artifact dispatcher failed: ",
        {repo_root_value, output_path});
    inferlab_plan::print_cli_usage(
        program, inferlab_plan::repo_root_artifact_output_cli_spec());
    return 1;
  }
  return 0;
}

}  // namespace inferlab_mature_product
