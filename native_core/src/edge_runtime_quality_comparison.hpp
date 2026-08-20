#pragma once

namespace inferlab::edge_runtime_matrix {

struct Matrix;

void derive_quality_comparison(Matrix* matrix);
void validate_quality_comparison(const Matrix& matrix);

}  // namespace inferlab::edge_runtime_matrix
