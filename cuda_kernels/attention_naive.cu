// Naive single-head scaled dot-product attention for correctness/profiling study.
// Layout: q [query, dim], k/v [key, dim], output [query, dim].

extern "C" __global__ void inferlab_attention_naive_f32(
    const float* q, const float* k, const float* v, float* output,
    int query_tokens, int key_tokens, int head_dim, float scale) {
  int query = blockIdx.x;
  int dim = threadIdx.x;
  if (query >= query_tokens || dim >= head_dim) return;

  // Educational skeleton: compute unnormalized weighted value using exp(score).
  // The CPU reference in native_core/attention.cpp is the correctness oracle.
  float numerator = 0.0f;
  float denominator = 0.0f;
  for (int key = 0; key < key_tokens; ++key) {
    float score = 0.0f;
    for (int d = 0; d < head_dim; ++d) {
      score += q[query * head_dim + d] * k[key * head_dim + d];
    }
    float weight = expf(score * scale);
    numerator += weight * v[key * head_dim + dim];
    denominator += weight;
  }
  output[query * head_dim + dim] = numerator / denominator;
}
