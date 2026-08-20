// Shared-memory tiled GEMM skeleton used as the optimized comparison point.

#ifndef INFERLAB_TILE_SIZE
#define INFERLAB_TILE_SIZE 16
#endif

extern "C" __global__ void inferlab_gemm_tiled_f32(
    const float* a, const float* b, float* c, int m, int n, int k) {
  __shared__ float tile_a[INFERLAB_TILE_SIZE][INFERLAB_TILE_SIZE];
  __shared__ float tile_b[INFERLAB_TILE_SIZE][INFERLAB_TILE_SIZE];

  int row = blockIdx.y * INFERLAB_TILE_SIZE + threadIdx.y;
  int col = blockIdx.x * INFERLAB_TILE_SIZE + threadIdx.x;
  float acc = 0.0f;

  for (int base = 0; base < k; base += INFERLAB_TILE_SIZE) {
    int a_col = base + threadIdx.x;
    int b_row = base + threadIdx.y;
    tile_a[threadIdx.y][threadIdx.x] = (row < m && a_col < k) ? a[row * k + a_col] : 0.0f;
    tile_b[threadIdx.y][threadIdx.x] = (b_row < k && col < n) ? b[b_row * n + col] : 0.0f;
    __syncthreads();
    for (int i = 0; i < INFERLAB_TILE_SIZE; ++i) {
      acc += tile_a[threadIdx.y][i] * tile_b[i][threadIdx.x];
    }
    __syncthreads();
  }

  if (row < m && col < n) c[row * n + col] = acc;
}
