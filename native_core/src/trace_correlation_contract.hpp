#pragma once

#include <string>
#include <vector>

namespace inferlab_trace {

const std::string& trace_context_fingerprint_algorithm();

const std::string& trace_context_join_token_algorithm();

bool valid_w3c_trace_id(const std::string& trace_id) noexcept;

bool valid_w3c_span_id(const std::string& span_id) noexcept;

std::string trace_context_set_fingerprint(
    const std::vector<std::string>& trace_ids);

bool valid_trace_context_set_fingerprint(
    const std::string& fingerprint) noexcept;

std::string trace_context_join_token(const std::string& trace_id);

bool valid_trace_context_join_token(const std::string& token) noexcept;

std::string span_context_join_token(const std::string& trace_id,
                                    const std::string& span_id);

bool valid_span_context_join_token(const std::string& token) noexcept;

}  // namespace inferlab_trace
