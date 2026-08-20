#pragma once

#include <cstdint>
#include <optional>

namespace inferlab::edge_runtime_matrix {

struct Matrix;
struct RuntimeRow;

void prepare_edge_resource_plan(
    const std::optional<std::uint64_t>& model_file_bytes, RuntimeRow* row);
void finalize_edge_resource_observation(RuntimeRow* row);
void validate_edge_resource_row(const RuntimeRow& row);
void derive_edge_deployment_recommendation(Matrix* matrix);
void validate_edge_deployment_recommendation(const Matrix& matrix);

}  // namespace inferlab::edge_runtime_matrix
