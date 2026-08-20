#include "product_replay_semantic_events_fixture.hpp"

#include "advanced_serving_internal.hpp"
#include "mature_product_profile_registry.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace inferlab::product_replay_semantic_events {
namespace {

using product_replay_alignment::NativeArtifactSelector;
using product_replay_alignment::NativeIdentitySource;

NativeArtifactSelector core(std::string role, std::string foreign_key) {
  return {std::move(role), NativeIdentitySource::core_deep_dive_registry,
          std::move(foreign_key), "native_deep_dive"};
}

NativeArtifactSelector wiring(std::string role, std::string foreign_key,
                              std::string family) {
  return {std::move(role), NativeIdentitySource::deep_dive_wiring_registry,
          std::move(foreign_key), std::move(family)};
}

NativeArtifactSelector catalog(std::string role, std::string foreign_key,
                               std::string family) {
  return {std::move(role), NativeIdentitySource::high_value_catalog,
          std::move(foreign_key), std::move(family)};
}

bool contains_string(const std::vector<std::string>& values,
                     const std::string& expected) {
  return std::find(values.begin(), values.end(), expected) != values.end();
}

void validate_field_specs(const std::vector<EventFieldSpec>& fields) {
  if (fields.empty()) {
    throw std::logic_error(
        "product replay semantic event field registry must not be empty");
  }
  for (std::size_t index = 0U; index < fields.size(); ++index) {
    if (fields[index].name.empty()) {
      throw std::logic_error(
          "product replay semantic event field name must not be empty");
    }
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (fields[prior].name == fields[index].name) {
        throw std::logic_error(
            "duplicate product replay semantic event field: " +
            fields[index].name);
      }
    }
  }
}

void validate_rules(const std::vector<SemanticEventRuleSpec>& rules) {
  constexpr std::size_t kExpectedRuleCount = 15U;
  if (rules.size() != kExpectedRuleCount) {
    throw std::logic_error(
        "product replay semantic event rule count drift");
  }
  std::vector<std::string> used_fields = {
      "event_type", "product", "timestamp_ms", "claim_scope",
      "trace_id", "trace_ids", "request_span_id", "request_span_ids"};
  for (std::size_t index = 0U; index < rules.size(); ++index) {
    const auto& rule = rules[index];
    if (rule.event_type.empty() || rule.event_domain.empty() ||
        rule.native_destination.role.empty() ||
        rule.native_destination.foreign_key.empty() ||
        rule.native_destination.family.empty() ||
        rule.required_fields.empty() || rule.claim_boundary.empty() ||
        rule.next_action_when_missing.empty()) {
      throw std::logic_error(
          "product replay semantic event rule is incomplete: " +
          rule.event_type);
    }
    advanced_serving_internal::require_unique_nonempty_strings(
        rule.required_fields,
        "product replay semantic event required fields");
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (rules[prior].event_type == rule.event_type) {
        throw std::logic_error(
            "duplicate product replay semantic event type: " +
            rule.event_type);
      }
    }
    for (const auto& field : rule.required_fields) {
      (void)require_event_field(field);
      if (!contains_string(used_fields, field)) used_fields.push_back(field);
    }
    const auto destination =
        product_replay_alignment::resolve_native_artifact(
            rule.native_destination);
    if (destination.artifact.empty() ||
        destination.refresh_command.empty() ||
        destination.claim_boundary.empty()) {
      throw std::logic_error(
          "product replay semantic event native destination is incomplete: " +
          rule.event_type);
    }
  }
  if (used_fields.size() != event_field_specs().size()) {
    throw std::logic_error(
        "product replay semantic event field registry contains an unused field");
  }
}

void validate_default_samples(const std::vector<DefaultSampleSpec>& samples) {
  if (samples.size() !=
      mature_product_profiles::product_profile_count_for_scope(
          mature_product_profiles::
              ProductProfileScope::default_required)) {
    throw std::logic_error(
        "product replay semantic default sample count drift");
  }
  for (std::size_t index = 0U; index < samples.size(); ++index) {
    const auto& sample = samples[index];
    if (sample.repository_path.rfind("configs/product_profiles/", 0U) != 0U ||
        sample.repository_path.size() <= 6U ||
        sample.repository_path.compare(sample.repository_path.size() - 6U,
                                       6U, ".jsonl") != 0U ||
        sample.repository_path.find("..") != std::string::npos ||
        sample.repository_path.find('\\') != std::string::npos ||
        sample.product_profile_id.empty()) {
      throw std::logic_error(
          "product replay semantic default sample is invalid");
    }
    const auto& profile = mature_product_profiles::require_product_profile(
        sample.product_profile_id);
    if (!mature_product_profiles::profile_is_default_required(profile)) {
      throw std::logic_error(
          "product replay default sample contains an optional profile");
    }
    for (std::size_t prior = 0U; prior < index; ++prior) {
      if (samples[prior].repository_path == sample.repository_path ||
          samples[prior].product_profile_id == sample.product_profile_id) {
        throw std::logic_error(
            "duplicate product replay semantic default sample identity");
      }
    }
  }
  for (const auto& profile : mature_product_profiles::product_profiles()) {
    if (!mature_product_profiles::profile_is_default_required(profile)) {
      continue;
    }
    const auto matches = static_cast<std::size_t>(std::count_if(
        samples.begin(), samples.end(), [&](const auto& sample) {
          return sample.product_profile_id == profile.id;
        }));
    if (matches != 1U) {
      throw std::logic_error(
          "product replay default sample coverage drift: " + profile.id);
    }
  }
}

}  // namespace

const char* event_field_kind_name(const EventFieldKind kind) {
  switch (kind) {
    case EventFieldKind::string:
      return "string";
    case EventFieldKind::unsigned_integer:
      return "unsigned_integer";
    case EventFieldKind::non_negative_number:
      return "non_negative_number";
    case EventFieldKind::string_array:
      return "string_array";
  }
  throw std::logic_error("unknown product replay semantic event field kind");
}

const char* sample_input_origin_name(const SampleInputOrigin origin) {
  switch (origin) {
    case SampleInputOrigin::default_fixture:
      return "default_fixture";
    case SampleInputOrigin::explicit_replay:
      return "explicit_replay";
  }
  throw std::logic_error("unknown product replay sample input origin");
}

const std::vector<EventFieldSpec>& event_field_specs() {
  static const std::vector<EventFieldSpec> fields = {
      {"event_type", EventFieldKind::string},
      {"product", EventFieldKind::string},
      {"timestamp_ms", EventFieldKind::unsigned_integer},
      {"claim_scope", EventFieldKind::string},
      {"request_id", EventFieldKind::string},
      {"trace_id", EventFieldKind::string},
      {"request_span_id", EventFieldKind::string},
      {"arrival_ms", EventFieldKind::unsigned_integer},
      {"tenant_id", EventFieldKind::string},
      {"prefill_start_ms", EventFieldKind::unsigned_integer},
      {"prompt_tokens", EventFieldKind::unsigned_integer},
      {"prefill_end_ms", EventFieldKind::unsigned_integer},
      {"kv_bytes", EventFieldKind::unsigned_integer},
      {"decode_step_ms", EventFieldKind::unsigned_integer},
      {"token_index", EventFieldKind::unsigned_integer},
      {"batch_id", EventFieldKind::string},
      {"request_ids", EventFieldKind::string_array},
      {"trace_ids", EventFieldKind::string_array},
      {"request_span_ids", EventFieldKind::string_array},
      {"scheduler_step", EventFieldKind::unsigned_integer},
      {"reason", EventFieldKind::string},
      {"cache_key", EventFieldKind::string},
      {"tier", EventFieldKind::string},
      {"fallback", EventFieldKind::string},
      {"source_worker", EventFieldKind::string},
      {"target_worker", EventFieldKind::string},
      {"transfer_ms", EventFieldKind::non_negative_number},
      {"status", EventFieldKind::string},
      {"route_target", EventFieldKind::string},
      {"reject_reason", EventFieldKind::string},
      {"operator", EventFieldKind::string},
      {"shape_signature", EventFieldKind::string},
      {"fallback_backend", EventFieldKind::string},
      {"artifact", EventFieldKind::string},
      {"error_type", EventFieldKind::string},
      {"message", EventFieldKind::string},
  };
  static const bool validated = [] {
    validate_field_specs(fields);
    return true;
  }();
  (void)validated;
  return fields;
}

const EventFieldSpec& require_event_field(const std::string& name) {
  const auto& fields = event_field_specs();
  const EventFieldSpec* match = nullptr;
  for (const auto& field : fields) {
    if (field.name != name) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "ambiguous product replay semantic event field: " + name);
    }
    match = &field;
  }
  if (match == nullptr) {
    throw std::out_of_range(
        "unknown product replay semantic event field: " + name);
  }
  return *match;
}

const std::vector<SemanticEventRuleSpec>& semantic_event_rules() {
  static const std::vector<SemanticEventRuleSpec> rules = {
      {"request_arrival", "request_lifecycle",
       wiring("scheduler_root_cause",
              "native_scheduler_root_cause_deep_dive",
              "native_deep_dive"),
       {"request_id", "arrival_ms", "tenant_id"},
       "Request arrival events can explain scheduler traces but do not prove product latency without imported replay or execute evidence",
       "import a product-scoped request arrival replay event"},
      {"prefill_start", "prefill_lifecycle",
       wiring("scheduler_root_cause",
              "native_scheduler_root_cause_deep_dive",
              "native_deep_dive"),
       {"request_id", "prefill_start_ms", "prompt_tokens"},
       "Prefill events are replay semantics only unless backed by product execute logs",
       "import a product-scoped prefill start replay event"},
      {"prefill_end", "prefill_lifecycle",
       wiring("pd_split_crossover",
              "native_pd_split_crossover_deep_dive",
              "native_deep_dive"),
       {"request_id", "prefill_end_ms", "kv_bytes"},
       "Prefill completion can support PD crossover explanation but not standalone speedup claims",
       "import a product-scoped prefill end replay event"},
      {"decode_step", "decode_lifecycle",
       wiring("scheduler_root_cause",
              "native_scheduler_root_cause_deep_dive",
              "native_deep_dive"),
       {"request_id", "decode_step_ms", "token_index"},
       "Decode step semantics explain TPOT and streaming gaps; they are not kernel timing evidence",
       "import a product-scoped decode step replay event"},
      {"request_terminal", "request_lifecycle",
       wiring("scheduler_root_cause",
              "native_scheduler_root_cause_deep_dive",
              "native_deep_dive"),
       {"request_id", "status"},
       "Request terminal events close a declared replay lifecycle only when paired with one request arrival and bounded timestamps; they are not execute latency or observed causality",
       "import one product-scoped request terminal replay event with canonical status"},
      {"batch_merge", "scheduler_batching",
       core("scheduler_batching", "scheduler_batching_deep_dive"),
       {"batch_id", "request_ids", "scheduler_step"},
       "Batch merge events explain scheduler behavior but mature product batching remains external",
       "import a batch merge replay event with typed request membership"},
      {"batch_split", "scheduler_batching",
       core("scheduler_batching", "scheduler_batching_deep_dive"),
       {"batch_id", "request_ids", "reason"},
       "Batch split events explain chunked prefill behavior but do not claim InferLab implements product schedulers",
       "import a batch split replay event with a typed reason"},
      {"kv_hit", "kv_cache",
       wiring("kv_tier_decision", "native_kv_tier_decision_deep_dive",
              "native_deep_dive"),
       {"request_id", "cache_key", "tier"},
       "KV hit events explain reuse decisions; correctness remains product-owned unless separately verified",
       "import a product-scoped KV hit replay event"},
      {"kv_miss", "kv_cache",
       wiring("kv_tier_decision", "native_kv_tier_decision_deep_dive",
              "native_deep_dive"),
       {"request_id", "cache_key", "fallback"},
       "KV miss events explain recompute fallback but do not prove cache policy quality",
       "import a product-scoped KV miss replay event"},
      {"kv_transfer_start", "kv_transport",
       wiring("pd_split_crossover",
              "native_pd_split_crossover_deep_dive",
              "native_deep_dive"),
       {"request_id", "kv_bytes", "source_worker", "target_worker"},
       "KV transfer start events explain PD transfer cost but do not benchmark an external transport",
       "import a product-scoped KV transfer start replay event"},
      {"kv_transfer_end", "kv_transport",
       wiring("pd_split_crossover",
              "native_pd_split_crossover_deep_dive",
              "native_deep_dive"),
       {"request_id", "transfer_ms", "status"},
       "KV transfer end events can feed crossover estimates only within replay claim scope",
       "import a product-scoped KV transfer end replay event"},
      {"route_decision", "control_plane",
       wiring("scheduler_root_cause",
              "native_scheduler_root_cause_deep_dive",
              "native_deep_dive"),
       {"request_id", "route_target", "reason"},
       "Routing events explain control-plane choices; external products own routing implementations",
       "import a product-scoped route decision replay event"},
      {"admission_reject", "control_plane",
       wiring("scheduler_root_cause",
              "native_scheduler_root_cause_deep_dive",
              "native_deep_dive"),
       {"request_id", "tenant_id", "reject_reason"},
       "Admission reject events explain SLO and tenant risk but do not prove a production SLA",
       "import a product-scoped admission reject replay event"},
      {"fallback", "kernel_runtime",
       wiring("kernel_compatibility",
              "native_kernel_compatibility_matrix",
              "native_kernel_contract"),
       {"operator", "shape_signature", "fallback_backend"},
       "Fallback events explain backend compatibility and must not be converted into performance claims",
       "import a product-scoped kernel or runtime fallback replay event"},
      {"error", "cross_product_error",
       catalog("release_pipeline", "release_evidence_pipeline",
               "native_release_gate"),
       {"artifact", "error_type", "message"},
       "Error events feed the downstream canonical release pipeline but do not read or close that pipeline",
       "import a product-scoped error replay event and refresh the release pipeline"},
  };
  static const bool validated = [] {
    validate_rules(rules);
    return true;
  }();
  (void)validated;
  return rules;
}

const SemanticEventRuleSpec* find_semantic_event_rule(
    const std::string& event_type) {
  const SemanticEventRuleSpec* match = nullptr;
  for (const auto& rule : semantic_event_rules()) {
    if (rule.event_type != event_type) continue;
    if (match != nullptr) {
      throw std::logic_error(
          "ambiguous product replay semantic event type: " + event_type);
    }
    match = &rule;
  }
  return match;
}

const std::vector<DefaultSampleSpec>& default_sample_specs() {
  static const std::vector<DefaultSampleSpec> samples = {
      {"configs/product_profiles/vllm_replay_sample.jsonl", "vllm"},
      {"configs/product_profiles/sglang_replay_sample.jsonl", "sglang"},
      {"configs/product_profiles/tensorrtllm_replay_sample.jsonl",
       "tensorrtllm"},
  };
  static const bool validated = [] {
    validate_default_samples(samples);
    return true;
  }();
  (void)validated;
  return samples;
}

const product_replay_alignment::NativeArtifactIdentity&
semantic_events_artifact_identity() {
  static const auto identity =
      product_replay_alignment::resolve_native_artifact(
          wiring("semantic_events", "product_replay_semantic_events",
                 "product_replay_semantics"));
  if (identity.canonical_id != "product_replay_semantic_events" ||
      identity.artifact != "product_replay_semantic_events.json") {
    throw std::logic_error(
        "product replay semantic event output identity drift");
  }
  return identity;
}

const inferlab_high_value::ManagedArtifact&
release_pipeline_artifact_identity() {
  const auto& identity = inferlab_high_value::require_managed_artifact(
      "release_evidence_pipeline");
  if (identity.id != "release_evidence_pipeline" ||
      identity.artifact != "native_release_evidence_pipeline.json" ||
      identity.target != "inferlab_release_evidence_pipeline") {
    throw std::logic_error(
        "product replay semantic downstream release pipeline identity drift");
  }
  return identity;
}

}  // namespace inferlab::product_replay_semantic_events
