#pragma once

#include "evidence_catalog_consumer_field_coverage_projection.hpp"
#include "evidence_catalog_fallback_removal_projection.hpp"

#include <cstddef>
#include <string>

namespace inferlab_release_pipeline {

struct EvidenceQueryCatalogFlowClosureProjection {
  bool object_present = false;
  inferlab_evidence::CatalogFallbackRemovalSignals fallback_removal;
  inferlab_evidence::ConsumerFieldCoverageSignals consumer_field_coverage;
  std::size_t contract_issue_count = 0;
};

EvidenceQueryCatalogFlowClosureProjection
project_evidence_query_catalog_flow_closure(
    const std::string& evidence_query_document, bool summary_usable,
    bool source_present);

}  // namespace inferlab_release_pipeline
