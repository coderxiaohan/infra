#pragma once

#include "checked_index.hpp"
#include "evidence_domain_types.hpp"
#include "plan_json_utils.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace inferlab_evidence {

struct RepairCardJsonOptions {
  std::string id_key = "id";
  std::string artifact_key = "artifact";
  std::string category_key = "category";
  std::string severity_key;
  std::string issue_key;
  std::string repair_action_key = "repair_action";
  bool include_repair_environment = true;
  bool include_repair_action = true;
  bool include_affected_claims = true;
  bool include_exact_wiring_locations = true;
  bool include_location_counts = true;
  bool include_current_repo_actions = false;
  bool include_target_machine_actions = false;
  bool include_claim_boundary = true;
  std::string claim_boundary;
};

enum class EvidenceJsonFieldType {
  kNull,
  kString,
  kBoolean,
  kUnsigned,
  kStringArray,
  kNamedSignedCounterArray,
};

struct EvidenceNamedCounter {
  std::string name;
  std::int64_t value = 0;
};

struct EvidenceJsonField {
  std::string key;
  EvidenceJsonFieldType type = EvidenceJsonFieldType::kString;
  std::string string_value;
  bool boolean_value = false;
  std::size_t unsigned_value = 0;
  std::vector<std::string> string_array_value;
  std::vector<EvidenceNamedCounter> named_signed_counter_array_value;
};

using RepairCardJsonField = EvidenceJsonField;

struct EvidenceJsonObject {
  std::vector<EvidenceJsonField> fields;
};

using ExplainTreeNode = EvidenceJsonObject;

inline EvidenceJsonField evidence_json_null_field(const std::string& key) {
  EvidenceJsonField field;
  field.key = key;
  field.type = EvidenceJsonFieldType::kNull;
  return field;
}

inline EvidenceJsonField evidence_json_string_field(
    const std::string& key, const std::string& value) {
  EvidenceJsonField field;
  field.key = key;
  field.type = EvidenceJsonFieldType::kString;
  field.string_value = value;
  return field;
}

inline EvidenceJsonField evidence_json_boolean_field(
    const std::string& key, const bool value) {
  EvidenceJsonField field;
  field.key = key;
  field.type = EvidenceJsonFieldType::kBoolean;
  field.boolean_value = value;
  return field;
}

inline EvidenceJsonField evidence_json_unsigned_field(
    const std::string& key, const std::size_t value) {
  EvidenceJsonField field;
  field.key = key;
  field.type = EvidenceJsonFieldType::kUnsigned;
  field.unsigned_value = value;
  return field;
}

inline EvidenceJsonField evidence_json_string_array_field(
    const std::string& key, const std::vector<std::string>& value) {
  EvidenceJsonField field;
  field.key = key;
  field.type = EvidenceJsonFieldType::kStringArray;
  field.string_array_value = value;
  return field;
}

inline EvidenceJsonField evidence_json_named_signed_counter_array_field(
    const std::string& key,
    const std::vector<EvidenceNamedCounter>& counters) {
  EvidenceJsonField field;
  field.key = key;
  field.type = EvidenceJsonFieldType::kNamedSignedCounterArray;
  field.named_signed_counter_array_value = counters;
  return field;
}

inline RepairCardJsonField repair_card_string_field(
    const std::string& key, const std::string& value) {
  return evidence_json_string_field(key, value);
}

inline RepairCardJsonField repair_card_boolean_field(
    const std::string& key, const bool value) {
  return evidence_json_boolean_field(key, value);
}

inline RepairCardJsonField repair_card_unsigned_field(
    const std::string& key, const std::size_t value) {
  return evidence_json_unsigned_field(key, value);
}

inline RepairCardJsonField repair_card_string_array_field(
    const std::string& key, const std::vector<std::string>& value) {
  return evidence_json_string_array_field(key, value);
}

inline std::vector<std::string> repair_card_json_field_names(
    const RepairCardJsonOptions& options) {
  std::vector<std::string> keys = {
      options.id_key, options.artifact_key, options.category_key};
  if (!options.severity_key.empty()) keys.push_back(options.severity_key);
  if (options.include_repair_environment) {
    keys.push_back("repair_environment");
  }
  if (!options.issue_key.empty()) keys.push_back(options.issue_key);
  if (options.include_repair_action) {
    keys.push_back(options.repair_action_key);
  }
  if (options.include_affected_claims) keys.push_back("affected_claims");
  if (options.include_exact_wiring_locations) {
    keys.push_back("exact_wiring_locations");
  }
  if (options.include_location_counts) {
    keys.push_back("exact_wiring_location_count");
    keys.push_back("source_location_gap_count");
  }
  if (options.include_current_repo_actions) {
    keys.push_back("current_repo_actions");
  }
  if (options.include_target_machine_actions) {
    keys.push_back("target_machine_actions");
  }
  if (options.include_claim_boundary) keys.push_back("claim_boundary");
  return keys;
}

inline void validate_repair_card_json_options(
    const RepairCardJsonOptions& options) {
  std::map<std::string, bool> keys;
  for (const auto& key : repair_card_json_field_names(options)) {
    if (key.empty()) {
      throw std::invalid_argument(
          "repair-card JSON field name must not be empty");
    }
    if (!keys.emplace(key, true).second) {
      throw std::invalid_argument("duplicate repair-card JSON field name: " +
                                  key);
    }
  }
}

inline std::string evidence_json_field_value(
    const EvidenceJsonField& field, const int indent) {
  switch (field.type) {
    case EvidenceJsonFieldType::kNull:
      return "null";
    case EvidenceJsonFieldType::kString:
      return inferlab_plan::json_string(field.string_value);
    case EvidenceJsonFieldType::kBoolean:
      return inferlab_plan::json_bool(field.boolean_value);
    case EvidenceJsonFieldType::kUnsigned:
      return std::to_string(field.unsigned_value);
    case EvidenceJsonFieldType::kStringArray:
      return inferlab_plan::json_string_array(field.string_array_value,
                                               indent);
    case EvidenceJsonFieldType::kNamedSignedCounterArray: {
      const int nested_indent = inferlab::internal::checked_add_int(
          indent, 2, "evidence JSON named-counter nested indent");
      const std::string pad = inferlab_plan::json_indent(indent);
      const std::string nested = inferlab_plan::json_indent(nested_indent);
      std::ostringstream out;
      out << "[";
      for (std::size_t i = 0;
           i < field.named_signed_counter_array_value.size(); ++i) {
        if (i > 0) out << ",";
        const auto& counter = field.named_signed_counter_array_value[i];
        if (counter.name.empty()) {
          throw std::invalid_argument(
              "named evidence counter must have a non-empty name");
        }
        out << "\n" << nested << "{\"name\": "
            << inferlab_plan::json_string(counter.name)
            << ", \"value\": " << counter.value << "}";
      }
      if (!field.named_signed_counter_array_value.empty()) {
        out << "\n" << pad;
      }
      out << "]";
      return out.str();
    }
  }
  throw std::invalid_argument("unsupported evidence JSON field type");
}

inline std::string render_evidence_json_objects(
    const std::vector<EvidenceJsonObject>& objects, const int indent,
    const std::string& object_kind) {
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "evidence JSON object nested indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << "[";
  for (std::size_t i = 0; i < objects.size(); ++i) {
    if (objects[i].fields.empty()) {
      throw std::invalid_argument(
          object_kind + " JSON object must contain at least one field");
    }
    if (i > 0) out << ",";
    out << "\n" << nested << "{\n";
    std::map<std::string, bool> used_keys;
    for (std::size_t j = 0; j < objects[i].fields.size(); ++j) {
      const auto& field = objects[i].fields[j];
      if (field.key.empty()) {
        throw std::invalid_argument(
            object_kind + " JSON field name must not be empty");
      }
      if (!used_keys.emplace(field.key, true).second) {
        throw std::invalid_argument(
            "duplicate " + object_kind + " JSON field name: " + field.key);
      }
      out << nested << "  " << inferlab_plan::json_string(field.key) << ": "
          << evidence_json_field_value(field, nested_indent)
          << (j == objects[i].fields.size() - 1U ? "\n" : ",\n");
    }
    out << nested << "}";
  }
  if (!objects.empty()) out << "\n" << pad;
  out << "]";
  return out.str();
}

inline std::string render_explain_tree_nodes_json(
    const std::vector<ExplainTreeNode>& nodes, const int indent) {
  return render_evidence_json_objects(nodes, indent, "explain-tree");
}

inline std::string render_repair_cards_json(
    const std::vector<RepairCard>& cards,
    const int indent,
    const RepairCardJsonOptions& options,
    const std::vector<std::vector<RepairCardJsonField>>& extension_fields = {}) {
  validate_repair_card_json_options(options);
  if (!extension_fields.empty() && extension_fields.size() != cards.size()) {
    throw std::invalid_argument(
        "repair-card JSON extension rows must match card count");
  }
  const std::vector<std::string> base_field_names =
      repair_card_json_field_names(options);
  const int nested_indent = inferlab::internal::checked_add_int(
      indent, 2, "repair-card JSON nested indent");
  const std::string pad = inferlab_plan::json_indent(indent);
  const std::string nested = inferlab_plan::json_indent(nested_indent);
  std::ostringstream out;
  out << "[";
  for (std::size_t i = 0; i < cards.size(); ++i) {
    if (i > 0) out << ",";
    const auto& card = cards[i];
    out << "\n" << nested << "{\n";
    bool first_field = true;
    const auto append_field = [&](const std::string& key,
                                  const std::string& json_value) {
      if (!first_field) out << ",\n";
      first_field = false;
      out << nested << "  " << inferlab_plan::json_string(key) << ": "
          << json_value;
    };
    append_field(options.id_key, inferlab_plan::json_string(card.id));
    append_field(options.artifact_key,
                 inferlab_plan::json_string(card.artifact));
    append_field(options.category_key,
                 inferlab_plan::json_string(card.category));
    if (!options.severity_key.empty()) {
      append_field(options.severity_key,
                   inferlab_plan::json_string(card.severity));
    }
    if (options.include_repair_environment) {
      append_field(
          "repair_environment",
          inferlab_plan::json_string(
              repair_environment_name(card.repair_environment)));
    }
    if (!options.issue_key.empty()) {
      append_field(options.issue_key,
                   inferlab_plan::json_string(card.issue));
    }
    if (options.include_repair_action) {
      append_field(options.repair_action_key,
                   inferlab_plan::json_string(card.repair_action));
    }
    if (options.include_affected_claims) {
      append_field(
          "affected_claims",
          inferlab_plan::json_string_array(card.affected_claims,
                                           nested_indent));
    }
    if (options.include_exact_wiring_locations) {
      append_field("exact_wiring_locations",
                   inferlab_plan::json_string_array(
                       card.exact_wiring_locations, nested_indent));
    }
    if (options.include_location_counts) {
      append_field("exact_wiring_location_count",
                   std::to_string(card.exact_wiring_location_count));
      append_field("source_location_gap_count",
                   std::to_string(card.source_location_gap_count));
    }
    if (options.include_current_repo_actions) {
      append_field(
          "current_repo_actions",
          inferlab_plan::json_string_array(card.current_repo_actions,
                                           nested_indent));
    }
    if (options.include_target_machine_actions) {
      append_field(
          "target_machine_actions",
          inferlab_plan::json_string_array(card.target_machine_actions,
                                           nested_indent));
    }
    if (!extension_fields.empty()) {
      std::map<std::string, bool> used_keys;
      for (const auto& key : base_field_names) used_keys.emplace(key, true);
      for (const auto& field : extension_fields[i]) {
        if (field.key.empty()) {
          throw std::invalid_argument(
              "repair-card JSON extension field name must not be empty");
        }
        if (!used_keys.emplace(field.key, true).second) {
          throw std::invalid_argument(
              "duplicate repair-card JSON extension field name: " +
              field.key);
        }
        append_field(field.key,
                     evidence_json_field_value(field, nested_indent));
      }
    }
    if (options.include_claim_boundary) {
      append_field("claim_boundary",
                   inferlab_plan::json_string(options.claim_boundary));
    }
    out << "\n";
    out << nested << "}";
  }
  if (!cards.empty()) out << "\n" << pad;
  out << "]";
  return out.str();
}

inline std::string repair_cards_json(
    const std::vector<RepairCard>& cards,
    const int indent,
    const std::string& claim_boundary) {
  RepairCardJsonOptions options;
  options.claim_boundary = claim_boundary;
  return render_repair_cards_json(cards, indent, options);
}

}  // namespace inferlab_evidence
