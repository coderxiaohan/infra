# Evidence Query

## Source Readiness And Downstream Deferral (2026-07-19)

- Query readiness now means that Root Cause, Catalog, direct Native Audit, and Warehouse/direct-scan contracts are usable. `blocked_result_count` describes the evidence being queried and no longer makes the Query producer itself unready.
- The output adds `queried_evidence_has_blockers`, `queried_evidence_blocks_query_execution=false`, `query_source_contract_ready`, and `query_readiness_scope` so an operator can distinguish “the tool ran” from “the queried release evidence is blocked.”
- Validation remains `not run per user request`; no CLI, JSON/text parity, or artifact refresh was executed.

## Purpose

This report tracks `results/raw/native_evidence_query_result.json`.

The query target makes the evidence warehouse usable from a release/debugging workflow. It can query artifact lineage, release blockers, claim scope, report coverage, freshness, product replay linkage, catalog-flow discrepancies, native-core audit status, and evidence confidence.

## Implemented Surface

- C++ target: `inferlab_evidence_query`
- Selector repair-card export target: `inferlab_evidence_query_selector_repair_card_export`
- Compatibility contract: the selector export target is a thin alias for `explain_blocker report_wiring_missing`; conflicting `--operation` or `--selector` overrides are rejected. It retains the established behavior of writing both the canonical query result and the standalone selector repair-card export when explicit output paths are not supplied.
- Sources: whole-file-reviewed thin `native_core/src/evidence_query_main.cpp`, request/parser/source owners, validated operation catalog, one final-result projection, and bounded text/filter/operation/support/repair/export/result view translation units under `native_core/src/evidence_query_*`
- Text view owners: `native_core/src/evidence_query_text_view.hpp/.cpp` plus overview, filter, Native Audit/snapshot, and repair/result section views; all consume the same immutable request and do not read artifacts or rederive policy.
- Operation catalog owners: `native_core/src/evidence_query_operation_catalog.hpp/.cpp` and `native_core/src/evidence_query_operation_contract_view.hpp/.cpp`; CLI positional and stable UX forms, readiness targets, operator actions, structured operation rows, parser usage, JSON/text supported-operation lists, and summary counts derive from one validated registry rather than parallel command tables or literal counts.
- Artifact support owners: `native_core/src/evidence_query_artifact_support_view.hpp/.cpp`, `native_core/src/evidence_query_repair_path_query_view.hpp/.cpp`, and `native_core/src/evidence_query_selector_export_artifact_view.hpp/.cpp`; Product Replay/source/query-plan/human-summary, repair-path, and selector-export JSON borrow the same immutable request and do not rerun source acquisition or release policy.
- Operation/readiness policy owner: `native_core/src/evidence_query_operation_policy.hpp`; operation metadata, selected-result kind/input readiness, selector action plan, repair narrative, readiness actionability, and payload count no longer live in the renderer/CLI main.
- RepairCard request projection/view owners: `native_core/src/evidence_query_repair_card_projection.hpp` and `native_core/src/evidence_query_repair_card_view.hpp`; one request-level producer builds checked card ids plus aligned typed extension rows from the canonical matcher/policy owners, and selected-operation, Query UX, and top-level JSON share the same projection instead of rescanning records.
- Explain/drilldown/source-location projection owner: `native_core/src/evidence_query_blocker_drilldown.hpp`; Explain Tree, blocker drilldown, and source-location category coverage consume one canonical selector/category/claim/location/environment-action model with checked counts.
- Source-location JSON view owner: `native_core/src/evidence_query_source_location_view.hpp`; category rows and source/fallback locations render from the typed model without a second record scan or category aggregate.
- Source-location worklist projection/view owners: `native_core/src/evidence_query_source_location_worklist_projection.hpp` and `native_core/src/evidence_query_source_location_worklist_view.hpp`; ranked first item/gap, priority, selectors and next action derive once, while exact-location/gap rows and claim boundaries serialize with checked display ids. Summary, Query UX, selected-operation and top-level worklist share that projection.
- Final result artifact owners: `native_core/src/evidence_query_result_artifact_projection.hpp/.cpp`, `native_core/src/evidence_query_result_artifact_sections.hpp`, bounded `evidence_query_result_artifact_summary_*_view.cpp`/body views, and `evidence_query_result_artifact_view.hpp/.cpp`; one request-backed projection feeds six stable schema sections and a thin ordered composer without returning source or policy ownership to the CLI main.
- Consumer filter signal/policy owner: `native_core/src/evidence_query_consumer_filter_signals.hpp`
- Cross-artifact adapter/domain/view owners: `native_core/src/evidence_query_cross_artifact_adapter.hpp`, `native_core/src/evidence_query_cross_artifact.hpp`, and `native_core/src/evidence_query_cross_artifact_view.hpp`
- Primary input: `results/raw/native_evidence_warehouse_index.json`
- Fallback input: direct managed-artifact scan from `results/raw` plus read-only source/config/report inspection under `--repo-root`
- Fixture input: `--fixture-dir configs/evidence_fixtures` for synthetic offline evidence-state queries
- Artifact: `results/raw/native_evidence_query_result.json`
- Supported filters: `--artifact <artifact-name-or-fragment>` and `--blocker <artifact_missing|schema_missing|claim_scope_missing|report_wiring_missing|freshness_stale|replay_missing|execute_missing|artifact_blocked>`
- Optional export: `--selector-repair-card-output <path>` writes a full selector-specific repair-card artifact when the operator wants every matched card instead of the bounded embedded sample.
- Full selector-export environment and source-gap counts consume the checked counters in `evidence_query_selector_view.hpp`; the export artifact no longer maintains a parallel card-classification loop.
- Embedded query-level `repair_cards` keep their existing field contract and remain read-only guidance. The JSON view accepts only a typed request projection; it cannot filter records, execute repairs, refresh evidence, or create replay/execute/benchmark evidence.
- Supported UX operations: `--operation list_artifacts`, `--operation show_artifact --selector <artifact>`, `--operation explain_blocker --selector <category|artifact>`, `--operation suggest_repair --selector <artifact-or-decision>`, `--operation repair_group_summary --selector <artifact-or-category>`, `--operation diff_snapshots --before <old> --after <new>`, and `--operation path --target <report_ready|replay_ready|execute_ready>`. These are compatibility wrappers over the existing read-only query engine; legacy `--query ...` commands remain supported.
- Positional UX aliases: the same read-only operations can be called as `list artifacts`, `show artifact <artifact>`, `explain blocker <category|artifact>`, `suggest repair <artifact-or-decision>`, `repair groups <artifact-or-category>`, `diff snapshots --before <old> --after <new>`, and `path report_ready|replay_ready|execute_ready`. Long options still take precedence, and positional tokens only fill missing operation, selector, or target fields.
- CLI validation boundary: multi-token positional commands must match the supported forms above; unsupported multi-token commands, extra positional tokens, invalid readiness targets, and unsupported `--format` values are rejected instead of being silently converted into a generic warehouse query. Single-token artifact selectors remain supported for compatibility.
- Supported output formats: `--format json` and `--format text`
- Release-debugging fields: `query_plan`, `operation_descriptor`, `operation_catalog`, `selector_resolution`, `selector_action_plan`, `selector_repair_card_export`, `cross_artifact_blocker_explanation`, `product_replay_semantic_linkage`, `selected_operation_result`, `top_blockers`, `repair_plan`, `query_ux`, `explain_tree`, `blocker_drilldown`, `repair_cards`, `repair_card_group_plan`, `repair_narrative`, `affected_claims`, `exact_wiring_locations`, `current_repo_actions`, `current_server_actions`, `target_machine_actions`, `target_product_actions`, `shortest_path_to_report_ready`, `shortest_path_to_replay_ready`, `shortest_path_to_execute_ready`, `readiness_path_drilldown`, `repair_card_rank_readiness_linkage`, `repair_card_source_exactness`, `source_location_category_coverage`, `environment_split`, and `human_readable_summary`
- Cross-artifact product replay family fields: the `product_replay_or_execute` family records direct `source_artifacts` for semantic events and replay alignment; Catalog-rule correlation over current native artifacts is nested in the canonical semantic-linkage projection. Summary fields expose blocker/linkage counts plus `cross_artifact_product_replay_family_target_product_replay_action_count` and `cross_artifact_product_replay_family_target_gpu_execute_action_count`. The older `cross_artifact_product_replay_family_target_or_validation_action_count` remains an aggregate compatibility field only. Family `primary_environment` and `next_action` select the typed replay or execute lane instead of always labeling the family as replay work.
- Cross-artifact release focus fields: `cross_artifact_blocker_explanation.release_focus` and summary fields `cross_artifact_release_focus_decision`, `cross_artifact_release_focus_repair_lane`, `cross_artifact_release_focus_affected_claim_count`, `cross_artifact_release_focus_repair_path_count`, `cross_artifact_release_focus_first_repair_family`, `cross_artifact_release_focus_first_repair_environment`, `cross_artifact_release_focus_first_repair_action`, `cross_artifact_release_focus_primary_action`, `cross_artifact_release_focus_current_repo_actionable`, `cross_artifact_release_focus_target_or_validation_required`, and `cross_artifact_release_focus_source_location_metadata_required` turn the blocker-family rollup into a release-review answer: what is blocking, which lane owns the next repair, which claims are affected, how many ranked repair paths exist, what the first path is, and whether the next action belongs in the current repo or a validation/product environment.
- Product replay semantic linkage fields: `product_replay_semantic_linkage` consumes the canonical strict projection over `product_replay_semantic_events.json` and `native_product_replay_alignment.json`, then evaluates the Catalog correlation-rule registry against current referenced native artifact contracts. It emits linkage status, semantic/alignment presence, event/rule/correlation counters, blocker count, first blocker, and next action. Required fields preserve missing/invalid/zero, malformed or wrong-level summaries fail closed, and Query owns the single request-local source projection. This lets Evidence Query explain whether product replay evidence is blocked by missing semantic events, missing alignment, unreadable or contract-invalid inputs, pending replay events, or blocked product-to-native correlations without a duplicate pseudo-SQL artifact.
- Native-core audit filter fields: `native_core_audit_filter_query` consumes `native_core_audit_register.json` when present, then summarizes source-root count, source-root issue/error count, scope blocker count, empty-scan status, in-scope file count, deferred file count, static issue count, source-unreadable count, file read-status counts, file read-error count, issue-group count, repair-card count, current-repo repair-card count, target-machine action count, review-worklist item count, high-priority review item count, subsystem review backlog count, closed review file count, open review item count, static audit completion percent, audit progress state, review-manifest configured/present/parse/override/applied/invalid/unmatched/closed-file counts, next review batch size, next high-priority batch count, audit closure shortest-path presence/state/step counts/source-location gaps/primary blocker/primary environment, top review priority, source readability, path-redaction review count, checked-arithmetic review count, artifact-contract review count, audit blocker count, and a `filter_decision` of `native_core_audit_ready`, `native_core_audit_pending`, `native_core_audit_scope_blocked`, `native_core_audit_summary_missing`, `native_core_audit_register_unreadable`, or `native_core_audit_register_missing`.
- Native-core audit projection owners: `native_core/src/native_core_audit_projection.hpp` defines the shared vocabulary; `native_core_audit_field_catalog.hpp` maps its 118 size, 17 boolean, and 28 string members to direct keys plus explicit Query aliases; `native_core_audit_register_projection.hpp` / `.cpp` own direct-register fallbacks; `native_core_audit_query_projection.hpp` / `.cpp` own Evidence Query-summary loading and conservative consumer blocker/filter policy; and `native_core_audit_merge.hpp` owns Root Cause numeric-max, boolean-OR, Query-first-string, source-envelope, and blocker reconciliation. Evidence Query retains only a thin repository lookup/wrapper. Missing legacy aggregates are reconstructed through checked owners rather than consumer-local readers.
- Source-location fields: `root_cause_source_location_card_count`, `repair_path_source_location_count`, and `repair_path_source_location_gap_count` show how many repair cards have concrete file/config/report entry points before a reviewer trusts the repair path, and how many still need source-location repair.
- Operation descriptor fields: `operation_descriptor` documents the normalized operation, selector/target presence, strict CLI validation flags, readiness target, snapshot input requirements, current-repo action, target-machine action, and non-execute claim boundary for the operator-facing CLI.
- Operation catalog fields: `operation_catalog` is a structured read-only CLI contract for `list_artifacts`, `show_artifact`, `explain_blocker`, `suggest_repair`, `repair_group_summary`, `diff_snapshots`, and `path report_ready|replay_ready|execute_ready`. Summary metrics include `operation_catalog_supported_count`, `operation_catalog_read_only_count`, `operation_catalog_snapshot_required_count`, `operation_catalog_selector_recommended_count`, and `operation_catalog_target_required_count` so summary, Prometheus, release gate, and final report can reason about UX support without parsing prose.
- Selector-resolution fields: `selector_resolution` records how the current selector was interpreted before the selected operation payload is trusted. It distinguishes `readiness_target`, `snapshot_pair`, `blocker_category`, `artifact_fragment`, `selector_missing`, and `none`; records `resolution_status`, artifact/blocker/total match counts, ambiguity, first matched artifact/category, first action, repair owner hint, affected claims, exact wiring locations, repair-card count, bounded `repair_cards`, emitted-card count, truncation flag, source-location gap count, and current-repo/current-server/target-machine/target-product action arrays. `selector_action_plan` condenses the selector result into a read-only next-step plan with decision, recommended readiness target, readiness path state/blocker count, current-repo step count, target-or-validation step count, source-location gap count, and bounded steps. `selector_repair_card_export` records the read-only export contract with `export_status`, total card count, embedded card count, embedded card limit, full card count, truncation state, optional external export path/status, and next actions for narrowing the selector when the embedded sample is capped. Summary metrics include `query_operation_selector_resolution_status`, `query_operation_selector_kind`, `query_operation_selector_match_count`, `query_operation_selector_ambiguous`, `query_operation_selector_repair_card_count`, `query_operation_selector_repair_card_emitted_count`, `query_operation_selector_repair_card_truncated`, `query_operation_selector_repair_card_export_status`, `query_operation_selector_repair_card_embedded_limit`, `query_operation_selector_repair_card_full_export_count`, `query_operation_selector_repair_card_external_export_configured`, `query_operation_selector_repair_card_external_export_written`, `query_operation_selector_repair_card_external_export_status`, `query_operation_selector_source_location_count`, `query_operation_selector_source_location_gap_count`, `query_operation_selector_current_repo_action_count`, `query_operation_selector_current_server_action_count`, `query_operation_selector_target_machine_action_count`, `query_operation_selector_target_product_action_count`, `query_operation_selector_action_plan_decision`, `query_operation_selector_action_plan_step_count`, `query_operation_selector_action_plan_current_repo_step_count`, `query_operation_selector_action_plan_target_or_validation_step_count`, `query_operation_selector_action_plan_source_location_gap_count`, `query_operation_selector_action_plan_readiness_blocker_count`, and `query_operation_selector_action_plan_readiness_state` so summary, Prometheus, release gate, and final report can surface ambiguous or missing selectors plus their repair context without parsing nested JSON.
- Selected-operation result fields: `selected_operation_result` is the operation-specific payload for the current `list_artifacts`, `show_artifact`, `explain_blocker`, `suggest_repair`, `repair_group_summary`, `diff_snapshots`, `path`, or fallback warehouse query operation. It records `result_kind`, selector/target/snapshot input readiness, payload count, read-only ownership actions, and exactly one focused payload such as `artifact_list`, `artifact_detail`, `explain_tree`, `blocker_drilldown`, `repair_cards`, `repair_narrative`, `repair_group_summary`, `snapshot_diff`, `readiness_path`, `readiness_path_drilldown`, `repair_card_rank_readiness_linkage`, `repair_card_source_exactness`, or `warehouse_query_results`. `show_artifact.artifact_detail` now includes candidate count, bounded candidate artifacts, exact-match status, selector-ambiguity status, selected-artifact strategy, affected claims, source-resolution status, source-exact flag, exact wiring locations, fallback investigation locations, source-location gap count, current-repo/current-server/target-machine/target-product ownership flags, and bounded next actions so an operator can inspect one artifact without jumping to the broader repair-card views or mistaking a fragment match for an exact artifact id. `explain_blocker.blocker_drilldown`, `suggest_repair.repair_narrative`, `repair_group_summary`, and `path.readiness_path_drilldown` now embed compact `selector_focus` objects with selector status, match counts, ambiguity, repair owner, first artifact/action, source-location gap count, and repair-card sample state, so each focused payload remains self-describing even when viewed outside the top-level `selector_resolution`. `explain_blocker.blocker_drilldown` condenses blocker records, source exactness, source-location gaps, affected claims, exact wiring locations, current-repo/current-server/target-machine/target-product ownership counts, first artifact/category/location/action, and bounded next actions. `suggest_repair.repair_narrative` condenses the primary repair group, selector decision, readiness target/state, blocker count, affected claims, exact wiring locations, current-repo actions, target/validation actions, and bounded narrative steps so an operator can see why a release remains blocked and where the next work belongs. `repair_group_summary` now combines the same selector focus with group count, card count, source-location gap count, primary group/action/source-location state, and navigation signals so a reviewer can tell whether the grouped repair view is broad, ambiguous, or scoped to one artifact/category. `path.readiness_path_drilldown` adds operator-facing target/state/blocker counts, path/navigation steps, current-repo versus target/validation step counts, source-location gaps, catalog-flow blockers, first artifact/category/action, command hints, and nested `actionability_focus` with actionability status, primary repair environment, blocker reason, current-repo/target-validation/source-location flags, and next read-only action.
- Explain-tree source coverage fields: `explain_tree` records blocker node count, source-exact node count, source-gap node count, current-repo source gaps, target-or-validation source gaps, source-exactness percent, and per-node `source_resolution_status`, `source_exact`, `source_location_count`, `source_location_gap`, ownership flags, `exact_wiring_locations`, and `next_action`. The companion `blocker_drilldown` object is a compact operator view of the same blocker set for `explain_blocker` and `query_ux`, so release reviewers can see action ownership and source-location readiness without scanning every node.
- Repair-card group fields: `repair_card_group_plan`, top-level `repair_group_summary`, top-level `repair_group_navigation`, top-level `source_location_repair_worklist`, top-level `repair_card_path_ranking`, top-level `repair_card_rank_readiness_linkage`, top-level `repair_card_source_exactness`, top-level `source_location_category_coverage`, `query_ux.repair_card_group_plan`, `query_ux.repair_narrative`, `query_ux.repair_group_summary`, `query_ux.repair_group_navigation`, `query_ux.source_location_repair_worklist`, `query_ux.repair_card_path_ranking`, `query_ux.repair_card_rank_readiness_linkage`, `query_ux.repair_card_source_exactness`, and `query_ux.source_location_category_coverage` group repair cards by `current_repo_fixable`, `current_server_when_validation_enabled`, `target_gpu_or_execute_machine`, and `target_product_or_replay_evidence`. The plan keeps card/category/artifact counts, first action, first wiring location, affected claims, and environment-specific action flags; the narrative turns the primary group, selector action plan, readiness path, source-location gaps, and affected claims into bounded operator steps with suggested read-only inspection commands; the summary adds compact group count, card count, source-location gap count, exact wiring-location count, affected-claim count, primary repair environment/category/artifact, primary action presence, and primary source-location presence. The navigation object adds bounded read-only drill-down steps with suggested `show_artifact`, `explain_blocker`, `suggest_repair`, and `repair_group_summary` inspection commands. The source-location worklist expands exact wiring locations into current-repo versus target/validation repair items and separately records source-location mapping gaps. The category coverage matrix enumerates known blocker categories plus observed unknown categories, reports active/exact/gap category counts, current-repo versus target/validation source-location gaps, gap repair-card count, exactness percent, exact wiring locations, and fallback investigation locations. The path ranking orders groups by current-repo-first ownership, source-location completeness, and repair-card volume so operators see a first-pass triage order without treating it as an automated repair decision. The readiness linkage maps each ranked group back to `report_ready`, `replay_ready`, and `execute_ready` path state, blocker count, command hint, primary target, and claim boundary so a reviewer can see which readiness decision a repair group is currently holding back. The source-exactness view checks each matching repair card for concrete source/config/report locations, emits a bounded card list, and separates current-repo source gaps from target/validation source gaps. This lets release gate and operators answer whether a blocker is fixable in the current repository, needs validation on the current server, or must wait for target GPU/product replay evidence without parsing every card.
- Query UX summary fields: `query_operation`, `query_operation_selector_present`, `query_operation_target_present`, `query_operation_snapshot_inputs_required`, `query_operation_snapshot_inputs_configured`, `query_operation_selected_result_present`, `query_operation_selected_result_input_ready`, `query_operation_selected_result_payload_count`, `query_operation_selected_result_kind`, `query_operation_show_artifact_candidate_count`, `query_operation_show_artifact_candidate_truncated`, `query_operation_show_artifact_exact_match`, `query_operation_show_artifact_selector_ambiguous`, `query_operation_show_artifact_selection_strategy`, `query_operation_payload_selector_focus_present`, `query_operation_payload_selector_focus_status`, `query_operation_payload_selector_focus_match_count`, `query_operation_payload_selector_focus_ambiguous`, `query_operation_payload_selector_focus_source_location_gap_count`, selector-resolution/export/action-plan fields, repair-card grouping fields, repair narrative decision/state/step/action/source-gap/readiness-blocker fields, source-location worklist/category fields, repair-card rank/readiness-link fields, source-exactness fields, blocker-drilldown record/category/source-location-gap/affected-claim/exact-location/action-owner counts, blocker category count, exact wiring location count, report/replay/execute blocker counts, and readiness-path drilldown target/state/blocker/step/source-gap/catalog-flow counts let summary and Prometheus surface the operator-facing query state without parsing the nested UX object.
- Snapshot diff fields: `snapshot_diff` and `query_ux.snapshot_diff` record `--before` and `--after` snapshot path status, configured/present flags, path issue count, read issue count, content-compared/content-changed flags, size bytes, size delta, content fingerprints, and `snapshot_diff_decision`. When both snapshots are readable and complete top-level JSON objects, the query also emits `semantic_compared`, `before_top_level_key_count`, `after_top_level_key_count`, `common_top_level_key_count`, `added_top_level_key_count`, `removed_top_level_key_count`, `changed_top_level_value_count`, bounded `added_top_level_keys`/`removed_top_level_keys`/`changed_top_level_value_keys`, and `semantic_diff_decision`. The semantic decision can be `snapshot_diff_semantic_changed`, `snapshot_diff_semantic_unchanged`, `snapshot_diff_semantic_not_json_object`, `snapshot_diff_semantic_parse_error`, or `snapshot_diff_semantic_not_compared`. For nested JSON debugging, the query also emits a bounded JSON Pointer path diff: `deep_semantic_compared`, `deep_semantic_truncated`, `before_deep_path_count`, `after_deep_path_count`, `common_deep_path_count`, `added_deep_path_count`, `removed_deep_path_count`, `changed_deep_value_count`, bounded `added_deep_paths`/`removed_deep_paths`/`changed_deep_value_paths`, and `deep_semantic_diff_decision`. The nested `snapshot_to_release_focus` object and summary fields `snapshot_to_release_focus_*` map the bounded diff to release-review guidance: focus presence, link status, blocker reason, review-required flag, current-repo action count, changed deep-path count, linked repair category, linked group id, and next action. This is read-only snapshot metadata and bounded JSON semantic diff guidance; it does not execute producers or create benchmark/replay/execute evidence.
- Text-mode catalog-flow line: `--format text` prints `catalog_flow_filter` so operators can see catalog-flow review presence, discrepancy counts, input gaps, missing surfaces, fallback-retained surfaces, single-source repair cards, parity-refresh-required count, surface migration ready/fallback/parity-refresh counts, surface-consumer migration/fallback-removal/parity/action counts, fallback-removal queue item/blocker/parity/action counts, fallback-removal shortest-path state/current-repo/target-machine/source-location-gap counts, fallback-removal parity-gate decision/blocker/parity/action/source-location counts, consumer field-coverage surface/fallback/required/gap/parity counts, consumer field-coverage repair/current-repo/target-machine card counts, metadata-policy card/artifact/field counts, missing steps, and repair-card counts without opening JSON. It also prints dedicated `catalog_flow_fallback_removal_queue`, `catalog_flow_fallback_removal_shortest_path`, and `catalog_flow_fallback_removal_parity_gate` lines with queue state, removal-blocking flag, repair environment, first surface, gate decision, removal allowance, and next action so operators do not need to infer fallback cleanup ownership from raw counters.

- Text-mode selected-operation line: `--format text` now prints `selector_resolution`, `selector_action_plan`, and `selected_operation_payload` immediately after the selected-result summary. The action-plan line shows decision, selector status, owner, repair-card count, step count, current-repo steps, target-or-validation steps, source-location gaps, recommended readiness target/state, and first step; it is read-only triage guidance, not an execution plan. For `show artifact`, the selected payload line also prints candidate count, exact-match status, selector-ambiguity status, selection strategy, first candidate, candidate truncation status, blocker category, confidence, repair environment, source-exact status, source-location gap state, first wiring/fallback location, and first action. For `explain blocker`, `suggest repair`, `repair_group_summary`, and `path`, the selected payload line now mirrors the compact selector-focus fragment (`selector_status`, `selector_kind`, match count, ambiguity, owner, source gaps, first artifact) before operation-specific details. For `explain blocker`, it prints category count, blocker-record count, source-exact/source-gap nodes, source-exactness percent, source-location gap count, affected-claim count, exact-location count, environment action counts, first artifact/location, and first action. For `suggest repair`, it now prints narrative step count, selector narrative decision, readiness target/state, primary repair environment, and first narrative step so a reviewer can see the repair direction without opening JSON. For `repair_group_summary`, it prints group/card counts, source-location gaps, source worklist size, path/readiness-link counts, primary repair environment/category/artifact, and primary action/source-location state.
- Text-mode native-core audit line: `--format text` prints `native_core_audit_filter` so operators can see audit-register presence, summary presence, read status, source-root issues, scope blockers, empty-scan status, in-scope file count, deferred file count, static issue count, source-unreadable count, issue-group count, repair-card count, current-repo repair-card count, target-machine action count, closed/open review progress, review-manifest override/applied/invalid/unmatched/closed counts, next review batch size, audit phase counts, phase worklist counts, and audit blocker count without opening JSON.
- Text-mode snapshot diff line: `--format text --operation diff_snapshots --before <old> --after <new>` prints `snapshot_diff` so operators can see configured inputs, before/after path status, path/read issue counts, content compared/changed flags, top-level JSON semantic compared flag, added/removed/changed top-level value counts, bounded deep JSON path compared/truncated flags, added/removed/changed path counts, size delta, release-link status, release blocker reason, current-repo action count, and read-only decisions without opening JSON.
- Fixture-mode fields: `fixture_used`, `fixture_dir`, `fixture_mode`, and `fixture_record_count`; these are synthetic/offline only and must not be reported as product replay or execute evidence.
- Catalog-aware fallback: direct scan recognizes `resolve_catalog_sources`, `contract_views()`, and `freshness_views()` so catalog-managed artifacts are not falsely marked as unwired just because they are not hardcoded in every downstream file.
- Explain/repair linkage: `explain_repair_linkage` points to `native_evidence_explain_repair.json` for explain trees, repair cards, exact wiring locations, and shortest paths, and to `native_new_artifact_flow_check.json` for standard artifact wiring checks.

## Claim Boundary

This target queries evidence. It does not create new benchmark numbers, product runtime claims, or CUDA timing. Missing warehouse data falls back to direct scan and remains a weaker evidence mode.


`repair_path_source_location_gap_count` is a reviewability signal only. It means a repair card lacks enough concrete source-location coverage for clean handoff; it is not benchmark, replay, execute, or CUDA timing evidence.

`first_action` and the text-mode shortest-path lines are repair guidance only. They do not run the suggested command, mutate repository files, refresh artifacts, or convert dry-run/replay evidence into execute evidence.

`query_ux`, `explain_tree`, `repair_cards`, `repair_card_group_plan`, action arrays, and `shortest_path_to_*` are read-only UX projections of existing evidence. They do not execute commands, mutate source files, refresh artifacts, render final reports, or create benchmark/replay/execute evidence.

`repair_card_group_plan` is grouping metadata for operator triage only. It does not mean grouped repair actions have been executed, validated, benchmarked, replayed, or promoted to product evidence.

`repair_narrative` is a compact read-only explanation of `suggest_repair`. It summarizes the first repair group, selector decision, readiness state, next narrative steps, affected claims, and source-location coverage, but it does not execute commands, mutate files, refresh artifacts, or prove repair completion.

`repair_card_rank_readiness_linkage` is read-only linkage metadata between ranked repair-card groups and readiness paths. It explains which group blocks `report_ready`, `replay_ready`, or `execute_ready`, but it does not execute the linked path command, refresh artifacts, validate repairs, or create replay/execute evidence.

`repair_card_source_exactness` is read-only source-location coverage metadata. It explains whether each repair card has concrete source/config/report wiring locations and where source-location mappings are still missing; it does not execute repairs, mutate files, refresh artifacts, or prove benchmark/replay/execute evidence.

`source_location_category_coverage` is read-only source-location mapping metadata. It separates exact source/config/report mappings from fallback investigation locations by blocker category; it does not execute repairs, mutate files, refresh artifacts, or prove benchmark/replay/execute evidence.

`explain_tree` source coverage is read-only blocker explanation metadata. It identifies blocker categories that still need category-to-source mapping before operators treat the explanation as source-exact; it does not repair those mappings, refresh artifacts, or prove target-machine readiness.

`blocker_drilldown` is a compact read-only explanation of the currently selected blocker set. It summarizes action ownership, affected claims, source-location gaps, and first repair hints, but it does not execute repairs, mutate files, refresh artifacts, or prove benchmark/replay/execute evidence.

`readiness_path_drilldown` is a compact read-only explanation of the selected `path report_ready|replay_ready|execute_ready` result. It separates current-repo steps from target/validation steps, carries source-location and catalog-flow blocker counts, and provides command hints for inspection only; it does not execute commands, refresh artifacts, repair code, or prove readiness.

`readiness_path_drilldown.actionability_focus` is the release-triage summary of that path. It turns the existing path, selector, and catalog-flow signals into `actionability_status`, `primary_environment`, `blocker_reason`, current-repo and target/validation ownership flags, source-location metadata requirements, and a bounded next action. It is still read-only guidance and does not run the suggested action, refresh artifacts, mutate files, or create replay/execute evidence.

`cross_artifact_blocker_explanation`, `product_replay_semantic_linkage`, and the `release_focus` projection are read-only evidence correlation across existing artifacts. They can identify the dominant blocker family, product replay semantic-linkage blocker, affected claim count, repair lane, and whether the next action belongs in the current repo, a validation-enabled current server, target GPU/execute machine, or product replay evidence path, but they do not execute repairs, refresh artifacts, replay products, run benchmarks, or create execute evidence.

`operation_descriptor` documents CLI semantics only. It does not execute repairs, refresh artifacts, compare snapshot contents, run validation, or create benchmark/replay/execute evidence.

`operation_catalog` documents supported CLI operations and their required selector/target/snapshot inputs only. It does not execute those operations, mutate files, refresh artifacts, or prove target-machine readiness.

`selector_resolution` is read-only query-routing and repair-context metadata. It helps operators see whether a selector matched an artifact, blocker category, readiness target, or snapshot pair, and where the matching blocker would be repaired. It does not execute a repair, mutate repository files, refresh artifacts, or create benchmark/replay/execute evidence.

`selector_action_plan` is read-only next-step guidance derived from the current selector, matching repair cards, and shortest readiness paths. It can separate current-repo work from current-server validation, target GPU/product replay work, source-location mapping gaps, and readiness blockers, but it does not execute commands, mutate repository files, refresh artifacts, or convert dry-run/replay metadata into execute evidence.

`selector_resolution.repair_cards` is a bounded read-only sample of selector-specific repair cards. `repair_card_count` is the total matching count, while `repair_card_emitted_count` and `repair_card_truncated` explain whether the embedded card list was capped. The card list is for triage only and does not execute repair commands.

`selector_repair_card_export` is an export contract by default. It explains whether the embedded selector repair-card list is complete or truncated and what action should narrow the query. When `--selector-repair-card-output` is explicitly configured, the CLI writes a separate full selector repair-card artifact, but that file is still read-only metadata and does not run commands, execute repairs, mutate repository files, refresh upstream evidence, or create benchmark/replay/execute evidence.

`selector_repair_card_export.export_review_focus` adds the operator-facing release review layer: export review status, blocker reason, whether an external full export file is still required, repair-card ownership counts by environment, source-location gap-card count, and the next read-only action. The external export artifact `native_evidence_query_selector_repair_card_export.json` mirrors the same focus and summary fields so reviewers can hand off the full card list without treating it as repair execution or validation evidence.

`selected_operation_result` is a read-only projection of the currently requested operation. It helps operators see the single most relevant payload without parsing every UX section, but it does not execute the operation, refresh evidence, mutate files, or create benchmark/replay/execute evidence.

The `selected_operation_payload` text line is the human-readable mirror of the same read-only projection. It is for triage only and must not be interpreted as validation output, repair execution, product replay, or benchmark evidence.

`snapshot_diff` reads configured snapshot files only to compute bounded size, fingerprint, top-level JSON-object key/value differences, and bounded JSON Pointer path differences. The deep path comparison is intentionally capped by depth and path count; `deep_semantic_truncated=true` means the output is a review hint rather than a complete recursive diff. It does not execute commands, mutate files, refresh artifacts, render reports, compare benchmark performance, or create replay/execute evidence.


Catalog-flow single-source repair/parity counters are read-only migration signals. They explain remaining fallback cleanup and downstream refresh needs; they do not prove that catalog consumers were regenerated.

Catalog-flow surface, surface-consumer migration, and consumer field-coverage counters are also read-only migration signals. They explain whether catalog consumer surfaces have reached ready state, still retain fallback behavior, are blocked from fallback removal, require parity refresh, or lack required catalog fields; they do not run refresh commands or prove target-machine readiness.

Catalog-flow fallback-removal queue counters and queue state fields are read-only cleanup signals. They explain which retained fallback surfaces still need current-repo repair, validation-machine refresh, or target-machine parity refresh before deletion, but they do not remove fallback lists, refresh downstream artifacts, or prove target-machine readiness.

Catalog-flow metadata-policy counters are weaker maintainability signals. They explain where catalog entries still rely on derived defaults for claim boundary, freshness, summary, final report, Prometheus, or report-kind metadata; they do not imply invalid runtime evidence or block release by themselves.




`native_core_audit_filter_query` is a read-only interpretation of `native_core_audit_register.json`. It does not compile native code, execute native binaries, run tests, run benchmark/CUDA timing, or prove full human review completion.

Native-core audit review-manifest fields (`native_core_audit_review_manifest_*`) are read-only handoff metadata from the audit register. They can show whether explicit staged review rows were configured, parsed, applied, invalid, unmatched, or closed static audit rows, but they do not run validation or prove that the reviewed files compile, execute, benchmark, replay, or pass target-machine checks.

Native-core audit shortest-path fields (`native_core_audit_audit_closure_shortest_path_*`) are read-only closure-navigation metadata. They help release gate and final report explain whether the next audit closure step belongs in the current repository or on a validation/target machine, but they do not execute that step or prove audit completion.

The native-core audit filter also exposes review-batch closure fields from the
audit register: `native_core_audit_review_batch_closure_present`,
`native_core_audit_review_batch_closure_state`,
`native_core_audit_review_batch_closure_file_count`,
`native_core_audit_review_batch_closure_high_priority_count`,
`native_core_audit_review_batch_closure_current_repo_action_count`,
`native_core_audit_review_batch_closure_target_machine_action_count`,
`native_core_audit_review_batch_closure_source_location_gap_count`, and
`native_core_audit_review_batch_closure_lane_count`. These fields describe the
next static review batch, repo-fixable actions, target-machine validation
actions, source-location gaps, and subsystem lanes. They are triage guidance
only; they do not mean the files were compiled, executed, benchmarked, or fully
human-reviewed.

The native-core audit filter also forwards audit phase-gate fields:
`native_core_audit_audit_phase_gate_present`,
`native_core_audit_audit_phase_count`,
`native_core_audit_audit_phase_ready_count`,
`native_core_audit_audit_phase_blocked_count`,
`native_core_audit_audit_phase_high_priority_blocked_count`,
`native_core_audit_audit_phase_review_backlog_count`,
`native_core_audit_audit_phase_current_repo_action_count`,
`native_core_audit_audit_phase_target_machine_action_count`,
`native_core_audit_audit_phase_source_location_gap_count`,
`native_core_audit_audit_phase_worklist_present`,
`native_core_audit_audit_phase_worklist_phase_count`,
`native_core_audit_audit_phase_worklist_open_review_item_count`,
`native_core_audit_audit_phase_worklist_high_priority_open_review_item_count`,
`native_core_audit_audit_phase_worklist_item_count`,
`native_core_audit_audit_phase_worklist_high_priority_item_count`,
`native_core_audit_audit_phase_worklist_current_repo_action_count`,
`native_core_audit_audit_phase_worklist_target_machine_action_count`, and
`native_core_audit_audit_phase_worklist_source_location_gap_count`,
`native_core_audit_audit_phase_file_worklist_present`,
`native_core_audit_audit_phase_file_worklist_phase_count`,
`native_core_audit_audit_phase_file_worklist_item_count`,
`native_core_audit_audit_phase_file_worklist_high_priority_item_count`,
`native_core_audit_audit_phase_file_worklist_current_repo_action_count`,
`native_core_audit_audit_phase_file_worklist_target_machine_action_count`, and
`native_core_audit_audit_phase_file_worklist_source_location_gap_count`, plus
`native_core_audit_file_review_closure_*` fields for phase/file closure state,
blocked phase count, open file count, trusted closed file count, source-location
gaps, and action ownership, plus `native_core_audit_file_review_checklist_*`
fields for file-level checklist state, check counts, blocked/manual-review
checks, first blocked/manual-review file/check pointers, source-location gaps,
and current-repo versus target-machine action ownership. The filter also forwards
`native_core_audit_file_review_completion_*` fields for file-level completion
backlog, high-priority file count, source-location gaps, and action ownership.
The filter also forwards
`native_core_audit_audit_review_coverage_*` fields for full in-scope static
review coverage state, open/high-priority/deferred file counts, completion
percent, primary blocker/environment, first open file, next action, and closure
blocking status. These fields
group the audit ledger into shared-infrastructure, core-mechanism,
artifact-entrypoint, and evidence/query/release phases, then expose the blocked
phase worklist as compact counts for release/query consumers. The `phase_count`
field counts phase rows; `open_review_item_count` counts file-level backlog;
legacy `item_count` remains as a phase-row compatibility alias. The
`audit_phase_file_worklist_*` fields expose a bounded file-level queue grouped by
phase for review UX and release triage. The `file_review_closure_*` fields expose
whether native-core file review is scope-blocked, backlog-blocked, or statically
closed at the current evidence level. The `file_review_checklist_*` fields expose
which static check category is blocking a file before the broader completion plan
is closed. The `file_review_completion_*` fields expose which open file backlog
remains actionable in the current repo before target-machine validation can be
treated as meaningful. The `audit_review_coverage_*` fields provide the compact
answer to which native audit coverage gate still blocks full static audit
closure. They are static
source-review routing metadata only; they do not execute phase validation,
repair files, or prove readiness.

## Refresh Command

```bash
./build/inferlab_native_core_audit_register --repo-root . --review-manifest configs/native_core_audit_review_manifest.json --output results/raw/native_core_audit_register.json --format json
./build/inferlab_release_evidence_pipeline --repo-root . --results results/raw --output results/raw/native_release_evidence_pipeline.json --format json
./build/inferlab_release_root_cause_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --results results/raw --output results/raw/native_release_root_cause_query.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query confidence_summary --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query catalog_flow_discrepancy --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query action_source_review --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query review_bundle_manifest --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query native_core_audit --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --query release_blockers --blocker report_wiring_missing --format text
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --operation list_artifacts --format text
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json list artifacts --format text
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --operation show_artifact --selector native_release_evidence_pipeline.json --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json show artifact native_release_evidence_pipeline.json --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --operation explain_blocker --selector report_wiring_missing --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_evidence_query_selector_repair_card_export --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json explain blocker report_wiring_missing --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --operation diff_snapshots --before results/raw/native_evidence_query_result.previous.json --after results/raw/native_evidence_query_result.json --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json --operation path --target report_ready --format text
./build/inferlab_evidence_query --repo-root . --warehouse results/raw/native_evidence_warehouse_index.json path report_ready --format text
./build/inferlab_evidence_query --repo-root . --fixture-dir configs/evidence_fixtures --query release_blockers --output results/raw/native_evidence_query_result.json --format json
./build/inferlab_new_artifact_flow_check --repo-root . --output results/raw/native_new_artifact_flow_check.json --format json
./build/inferlab_evidence_explain_repair --repo-root . --results results/raw --output results/raw/native_evidence_explain_repair.json --format json
```

Current server status: `not run per user request`.

## Source-Location Worklist Focus

`source_location_repair_worklist` now includes `source_location_worklist_focus`, a compact first-item / first-gap projection for release reviewers. It records worklist ownership counts, first actionable source-location item, first source-location metadata gap, current-repo versus target/validation action flags, command hints, release-gate key, next action, and a read-only claim boundary.

Evidence Query summary forwards the same state through `query_ux_source_location_worklist_focus_*` fields so Release Pipeline, benchmark summary, Prometheus numeric metrics, and final-report scorecards can consume the focus without parsing the full worklist array. This is operator triage metadata only; it does not execute repairs, mutate files, refresh artifacts, run benchmarks, replay products, render reports, or prove target-machine readiness.

## Repair Group View Ownership

RepairCard group summary, navigation, and plan JSON are now rendered only by `evidence_query_repair_group_view.hpp`. The view consumes canonical group counts, deterministic ordering, repair-environment policy, readiness semantics, and selector-focus rendering rather than rebuilding those rules in the Evidence Query main.

Navigation steps and group-plan ids use checked `size_t` increments. Path ranking/readiness linkage and RepairCard source-exactness remain separate pending convergence so this view stays below the anti-redundancy size boundary. This is read-only explain/repair presentation metadata; it does not execute commands, mutate files, refresh artifacts, create benchmark/replay/execute evidence, or prove release readiness.

## Repair Path View Ownership

RepairCard path ranking and rank-to-readiness linkage are now rendered only by `evidence_query_repair_path_view.hpp`. The view consumes canonical group ranking, priority, readiness blocking, primary-target, and shortest-path lookup policy rather than rebuilding these decisions in the Evidence Query main.

Path and linkage rank ids use checked `size_t` increments. Report/replay/execute path state and blocker counts are invariant across ranked rows and are projected once per render invocation. Shortest-path detail also lives in this owner and selects targets through `find_repair_path`. This remains read-only triage metadata; it does not execute commands, mutate files, refresh artifacts, create benchmark/replay/execute evidence, or prove release readiness.

## Repair Source-Exactness Ownership

`evidence_query_repair_source_projection.hpp` now derives complete source-exactness statistics and the bounded 32-row sample in one pass over the canonical RepairCard matcher. Summary-only callers delegate to the same projection with a zero-row limit, so stats and view rows cannot drift through separate classification loops.

`evidence_query_repair_source_view.hpp` only renders that typed projection. Display ids use checked `size_t`, emitted count is collection-owned, and exact/fallback location arrays reference immutable row data. This is read-only source-location coverage metadata; it does not execute repairs, mutate files, refresh artifacts, create benchmark/replay/execute evidence, or prove readiness.

## Selected-Operation Text Ownership

`evidence_query_selected_operation_text_view.hpp` now owns text payloads for all selected operations. Record result status comes from `evidence_query_record_policy.hpp`; show-artifact confidence, affected claims, and environment ownership consume typed record policy; readiness paths consume canonical lookup and cached actionability state.

The default blocker count is checked. The text view is read-only presentation and does not execute commands, mutate files, refresh artifacts, create benchmark/replay/execute evidence, or prove release readiness. The later selected-operation projection/JSON-view batch made this text view consume the same selected-result contract.

## Repair Collection Ownership

`evidence_query_repair_collection_projection.hpp` now derives affected claims, exact wiring locations, matching blocker categories, and four repair-environment action groups in one checked filtered scan. A JSON artifact shares the same projection across selected-operation JSON, Query UX, and top-level arrays; text creates one projection for its action sections.

`evidence_query_repair_collection_view.hpp` owns only array serialization and empty-state wording. This removes repeated record traversal without creating evidence or executing repairs. The completed selected-operation JSON view composes this projection without reopening raw-record access.

## Snapshot View Ownership

`evidence_query_snapshot_diff_view.hpp` now owns snapshot-to-release focus and complete snapshot-diff JSON. It consumes the canonical snapshot signal model, release-review policy, bounded deep-change counts, and optional-path redaction.

The view performs no file I/O, JSON parsing, fingerprinting, diff computation, release action, or artifact refresh. This is read-only evidence inspection and does not create benchmark/replay/execute evidence.

## Readiness-Path Projection And View Ownership

`evidence_query_operation_policy.hpp` now builds one `ReadinessPathActionabilityProjection` for the selected path and selector resolution. State, checked step/source-gap/catalog-flow counts, actionability status, primary repair environment, blocker reason, next action, and actionability flags are derived once and shared by JSON, text, and Query UX consumers.

`evidence_query_readiness_path_view.hpp` owns actionability-focus and readiness-drilldown JSON only. It consumes the canonical selected path/target, selector action plan/focus, and typed projection. Catalog field-coverage gaps retain their own blocker reason instead of being hidden by the aggregate source-gap count. This is read-only operator guidance; it does not execute repairs, refresh artifacts, change release decisions, or create benchmark/replay/execute evidence.

## Blocker Request Projection And View Ownership

`evidence_query_blocker_drilldown.hpp` now builds category counts, blocker records, claims, exact locations, repair-environment actions, first-item fields, and source statistics in one matched-record pass. Source-location category coverage derives from that immutable request projection.

`evidence_query_blocker_drilldown_view.hpp` owns Evidence Query explain-tree and blocker-drilldown JSON. JSON and text render orchestration each create one selector resolution and one blocker projection, then share them across selected-operation, Query UX, top-level, and text views. Records-based convenience overloads were removed so nested views cannot silently rescan evidence. These objects remain read-only explanation metadata and do not execute repairs, refresh artifacts, or create benchmark/replay/execute evidence.

## Source-Exactness Request Projection Ownership

JSON rendering now creates one bounded `RepairCardSourceExactnessProjection` and shares it across selected-operation, Query UX, top-level JSON, and summary fields. Text rendering creates one zero-row projection for the same checked statistics. The projection owns `row_limit`, rows, totals, truncation, and environment/source classification; the JSON view no longer accepts raw records or guesses the embedded limit.

This projection is read-only source-location coverage metadata. It does not execute repairs, mutate files, refresh artifacts, or create benchmark/replay/execute evidence.

## Artifact Request Projection And View Ownership

`evidence_query_artifact_projection.hpp` now builds one borrowed matched-record view per JSON or text request after record ordering is stable. It owns checked confidence/blocker/category statistics, bounded exact-first show-artifact candidates, selected detail, canonical affected claims, repair environment, and source exactness. No consumer mutates the source records while those pointers are used.

`evidence_query_artifact_view.hpp` owns record results, full results, artifact list, and show-artifact JSON. Summary, selected-operation payloads, Query UX, top-level results, and text consume the same projection instead of filtering, selecting, or copying records independently. This is read-only query presentation and does not execute commands, refresh artifacts, mutate files, or create benchmark/replay/execute evidence.

## Operation Guidance Request Projection And View Ownership

`evidence_query_operation_guidance_projection.hpp` now owns the request-level selector action plan, repair narrative, selected readiness target/path/actionability, primary repair group, checked group/card/environment/source totals, affected claims, and exact wiring locations. JSON and text each build one projection after groups and paths are stable, and do not mutate those source vectors while borrowed pointers are used.

`evidence_query_operation_guidance_view.hpp` owns selector action-plan JSON/text and repair-narrative JSON. Selected-operation、Query UX、summary、payload count、readiness drilldown and text consume the same immutable guidance instead of rebuilding steps, recounting groups, or looking up paths independently. This is read-only operator guidance and does not execute commands, mutate files, refresh artifacts, or create benchmark/replay/execute evidence.

## Selected-Operation Projection And JSON/Text View Ownership

`evidence_query_operation_policy.hpp::SelectedOperationProjection` now owns result kind, input readiness, and payload count once per JSON or text render. JSON summary fields, the selected-operation object, and text output consume that request projection rather than independently calling scalar helpers.

`evidence_query_selected_operation_view.hpp` owns the list/show/explain/suggest/group/diff/path/default JSON dispatcher through a typed reference context. It composes existing artifact, guidance, RepairCard, repair collection, blocker, source-location, selector, snapshot, readiness, and canonical typed group/path collections without accepting raw warehouse records, rebuilding group/path state, or executing commands. `evidence_query_selected_operation_text_view.hpp` consumes the same selected-result projection. This is read-only presentation and does not refresh artifacts, execute repairs, create benchmark/replay/execute evidence, or prove release readiness.

## Blocker-List Projection And Query-UX View Ownership

`evidence_query_blocker_list_projection.hpp` now derives top-level blocker rows, checked category counts, deterministic top-blocker ranking, the fixed eight-category repair plan, and the legacy environment split in one records/blocker-filter traversal. `evidence_query_blocker_list_view.hpp` only renders that projection; blockers, top blockers, repair plan, and environment split no longer rescan raw warehouse records independently.

`evidence_query_view_context.hpp` defines the shared layered request dependency contract. JSON and text share the operation-level artifact, operation, query, paths, groups, guidance, selector, blocker, and snapshot references. JSON adds rendered RepairCards, repair collections, source coverage, and source-exactness projections; text is not required to construct those unused JSON-only objects.

`evidence_query_query_ux_view.hpp` owns complete Query UX JSON composition and delegates nested artifacts, blocker drilldown, RepairCards, repair groups/paths, source-location views, selector guidance, readiness paths, and snapshot diff to their canonical view owners. These views are read-only evidence presentation. They do not execute repairs, mutate files, refresh artifacts, create benchmark/replay/execute evidence, or prove release readiness.





Catalog-flow consumers now read scalar metrics from strict `summary` objects with an explicit old flattened-root fallback. Product replay linkage requires parse-valid sources and presence-aware typed required fields; missing/invalid fields fail closed, while a missing artifact is not counted again as a contract error. These projections remain read-only evidence interpretation; they do not execute repairs, import replay logs, refresh artifacts, run products, create benchmark timing, or prove release readiness.

## Single-Request Source Orchestration

`evidence_query_cli_parser.hpp/.cpp` now owns positional grammar, query normalization, and operation selection. `evidence_query_source_projection.hpp/.cpp` owns one warehouse/fixture/direct-scan source pass plus Catalog Flow and Native Audit source projections. `evidence_query_request.hpp/.cpp` owns the snapshot repository and every pointer-bearing Query projection in dependency order and is intentionally noncopyable/nonmovable.

The selector repair-card compatibility export, primary JSON output, and text output now consume the same request. They no longer reread warehouse/artifacts, re-resolve records, reparse operations, or independently derive release sources, repair paths, Query UX, selector guidance, Product Replay linkage, and readiness. Nested query-plan and repair-path views also borrow request-owned state.

The fixture adapter now writes `metadata.release_decision` from the fixture's `expected_decision`; blocker category remains a separate field. The existing target, compatibility executable, CLI, artifact name, schema, and claim boundary are unchanged. The main decreased from 5,448 to 4,695 lines in this orchestration batch; its large presentation surfaces remained open at that point.

## Text View Convergence

`evidence_query_text_view.hpp/.cpp` now owns the ordered text composition. Four bounded projection-only sections serialize operation/query overview, Catalog/Review filter status, Native Audit/snapshot status, and repair/result guidance from the same `EvidenceQueryRequest`. The former 792-line main-local `render_text()` is deleted rather than retained as a compatibility implementation.

All text-side blocker, confidence, readiness, repair environment/action, source exactness, fallback-removal, and Native Audit totals continue to consume their existing canonical owners. The section views perform no source I/O, raw artifact scan, record filtering, policy recomputation, command execution, or artifact refresh. `evidence_query_main.cpp` decreased from 4,695 to 3,897 lines; its remaining JSON/filter presentation and CLI orchestration still prevent whole-file audit closure.

## JSON Filter View Convergence


The main-local filter renderers are deleted; final artifact composition retains the established calls and ordering. The migrated views use shared bounded indentation and canonical path redaction rather than owning local boundary helpers. `evidence_query_main.cpp` decreased from 3,897 to 3,002 lines, but final artifact composition, repair-path/selector-export views, CLI orchestration, and whole-file closure remain open.

Validation status: `not run per user request`. This is static read-only evidence orchestration and view convergence; it does not execute repairs, refresh artifacts, run product replay/execute, create benchmark timing, or prove release/GPU readiness.
