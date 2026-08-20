#pragma once

#include "inferlab/matrix_ops.hpp"

namespace inferlab::matrix_ops_profile {

struct Request {
  int m = 0;
  int k = 0;
  int n = 0;
  int tile_m = 0;
  int tile_k = 0;
  int tile_n = 0;
  int softmax_rows = 0;
  int softmax_cols = 0;
};

struct Plan {
  Request request;
  DenseOpRooflineProfile matmul;
  DenseOpRooflineProfile matvec;
  TiledMatmulRooflineProfile tiled_matmul;
  RowSoftmaxRooflineProfile row_softmax;
};

Plan build_plan(const Request& request);

void validate_plan(const Plan& plan);

}  // namespace inferlab::matrix_ops_profile
