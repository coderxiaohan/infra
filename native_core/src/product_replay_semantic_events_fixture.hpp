#pragma once

#include "high_value_artifact_catalog.hpp"
#include "product_replay_alignment_fixture.hpp"

#include <string>
#include <vector>

namespace inferlab::product_replay_semantic_events {

enum class EventFieldKind {
  string,
  unsigned_integer,
  non_negative_number,
  string_array,
};

struct EventFieldSpec {
  std::string name;
  EventFieldKind kind = EventFieldKind::string;
};

struct SemanticEventRuleSpec {
  std::string event_type;
  std::string event_domain;
  product_replay_alignment::NativeArtifactSelector native_destination;
  std::vector<std::string> required_fields;
  std::string claim_boundary;
  std::string next_action_when_missing;
};

enum class SampleInputOrigin {
  default_fixture,
  explicit_replay,
};

struct DefaultSampleSpec {
  std::string repository_path;
  std::string product_profile_id;
};

const char* event_field_kind_name(EventFieldKind kind);
const char* sample_input_origin_name(SampleInputOrigin origin);
const std::vector<EventFieldSpec>& event_field_specs();
const EventFieldSpec& require_event_field(const std::string& name);
const std::vector<SemanticEventRuleSpec>& semantic_event_rules();
const SemanticEventRuleSpec* find_semantic_event_rule(
    const std::string& event_type);
const std::vector<DefaultSampleSpec>& default_sample_specs();
const product_replay_alignment::NativeArtifactIdentity&
semantic_events_artifact_identity();
const inferlab_high_value::ManagedArtifact&
release_pipeline_artifact_identity();

}  // namespace inferlab::product_replay_semantic_events
