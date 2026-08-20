void parse_clock_calibration_contract(
    const std::string& document,
    const inferlab_artifact::ArtifactSemanticIndex& index,
    const std::string& base_pointer,
    const std::string& normalization_pointer,
    const std::string& relative_values_pointer,
    TraceTokenSourceObservation& source) {
  const std::size_t issue_count_before = source.issues.size();
  bool source_present = false;
  bool source_readable = false;
  bool source_contract_ready = false;
  bool raw_trace_identity_emitted = true;
  bool clock_anchor_values_emitted = true;
  bool fingerprint_cryptographic = true;
  bool absolute_values_emitted = true;
  bool relative_values_emitted = false;
  required_bool(document, index, "source_supplied",
                base_pointer + "/source_supplied",
                source.clock_calibration_source_supplied, source.issues);
  required_bool(document, index, "source_present",
                base_pointer + "/source_present", source_present,
                source.issues);
  required_bool(document, index, "source_readable",
                base_pointer + "/source_readable", source_readable,
                source.issues);
  required_bool(document, index, "source_contract_ready",
                base_pointer + "/source_contract_ready",
                source_contract_ready, source.issues);
  required_bool(document, index, "all_lookups_calibrated",
                base_pointer + "/all_lookups_calibrated",
                source.clock_calibration_all_lookups_calibrated,
                source.issues);
  required_bool(document, index, "raw_trace_identity_values_emitted",
                base_pointer + "/raw_trace_identity_values_emitted",
                raw_trace_identity_emitted, source.issues);
  required_bool(document, index, "clock_anchor_values_emitted",
                base_pointer + "/clock_anchor_values_emitted",
                clock_anchor_values_emitted, source.issues);
  required_bool(document, index, "fingerprint_is_cryptographic",
                base_pointer + "/fingerprint_is_cryptographic",
                fingerprint_cryptographic, source.issues);
  required_bool(document, index, "clock_normalization_performed",
                normalization_pointer, source.clock_normalization_performed,
                source.issues);
  required_bool(document, index, "absolute_timestamp_values_emitted",
                base_pointer.substr(0U, base_pointer.find_last_of('/')) +
                    "/absolute_timestamp_values_emitted",
                absolute_values_emitted, source.issues);
  required_bool(document, index, "relative_timestamp_values_emitted",
                relative_values_pointer, relative_values_emitted,
                source.issues);

  required_unsigned(
      document, index, "lookup_count", base_pointer + "/lookup_count",
      kMaximumProjectedScalarValues,
      source.declared_clock_calibration_lookup_count, source.issues);
  required_unsigned(
      document, index, "calibrated_timestamp_count",
      base_pointer + "/calibrated_timestamp_count",
      kMaximumProjectedScalarValues,
      source.declared_clock_calibrated_timestamp_count, source.issues);
  required_unsigned(
      document, index, "uncalibrated_timestamp_count",
      base_pointer + "/uncalibrated_timestamp_count",
      kMaximumProjectedScalarValues,
      source.declared_clock_uncalibrated_timestamp_count, source.issues);
  required_string(document, index, "contract_fingerprint",
                  base_pointer + "/contract_fingerprint",
                  source.clock_calibration_fingerprint, source.issues);
  required_string(document, index, "fingerprint_algorithm",
                  base_pointer + "/fingerprint_algorithm",
                  source.clock_calibration_fingerprint_algorithm,
                  source.issues);
  required_string(document, index, "decision",
                  base_pointer + "/decision",
                  source.clock_calibration_decision, source.issues);
  std::string normalized_unit;
  std::string calibration_scope;
  required_string(document, index, "normalized_timestamp_unit",
                  base_pointer + "/normalized_timestamp_unit",
                  normalized_unit, source.issues);
  required_string(document, index, "calibration_scope",
                  base_pointer + "/calibration_scope",
                  calibration_scope, source.issues);

  const bool source_flags_match =
      source.clock_calibration_source_supplied == source_present &&
      source.clock_calibration_source_supplied == source_readable &&
      source.clock_calibration_source_supplied == source_contract_ready;
  const bool fingerprint_matches_source =
      inferlab_trace_clock::valid_clock_calibration_fingerprint(
          source.clock_calibration_fingerprint) &&
      (source.clock_calibration_source_supplied
           ? source.clock_calibration_fingerprint != "none"
           : source.clock_calibration_fingerprint == "none");
  const bool partition_matches =
      inferlab::internal::checked_add_size(
          source.declared_clock_calibrated_timestamp_count,
          source.declared_clock_uncalibrated_timestamp_count,
          "trace-token calibration usage partition") ==
      source.declared_clock_calibration_lookup_count;
  const bool all_lookups_match =
      source.clock_calibration_all_lookups_calibrated ==
      (source.declared_clock_calibration_lookup_count > 0U &&
       source.declared_clock_calibrated_timestamp_count ==
           source.declared_clock_calibration_lookup_count);
  const bool decision_matches =
      source.clock_calibration_decision ==
      inferlab_trace_clock::clock_calibration_usage_decision(
          source.clock_calibration_source_supplied,
          source.declared_clock_calibration_lookup_count,
          source.declared_clock_calibrated_timestamp_count);
  if (!source_flags_match || raw_trace_identity_emitted ||
      clock_anchor_values_emitted || fingerprint_cryptographic ||
      absolute_values_emitted ||
      relative_values_emitted !=
          (source.declared_clock_calibrated_timestamp_count > 0U) ||
      source.clock_normalization_performed !=
          (source.declared_clock_calibrated_timestamp_count > 0U) ||
      !fingerprint_matches_source || !partition_matches ||
      !all_lookups_match || !decision_matches ||
      source.clock_calibration_fingerprint_algorithm !=
          inferlab_trace_clock::clock_calibration_fingerprint_algorithm() ||
      normalized_unit != "trace_relative_nanoseconds" ||
      calibration_scope !=
          "trace_id_source_scope_clock_domain_affine_anchor_v1") {
    append_unique(source.issues,
                  "trace_clock_calibration_contract_invalid");
  }
  source.clock_calibration_contract_evaluated = true;
  source.clock_calibration_contract_ready =
      source.issues.size() == issue_count_before;
}
