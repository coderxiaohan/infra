#pragma once

#include "product_replay_semantic_events_model.hpp"

#include <cstddef>
#include <vector>

namespace inferlab::product_replay_semantic_events {

std::size_t maximum_replay_sample_count() noexcept;

SampleSourceState parse_replay_sample_source(
    const ReplaySampleRequest& request, std::size_t source_index,
    std::vector<NormalizedEventRow>& events);

}  // namespace inferlab::product_replay_semantic_events
