# Nsight Compute Profiling Notes

This directory stores command templates and future Nsight Compute exports. The current CPU-only server records model-side roofline estimates in `results/raw/matrix_ops_profile.json` and `reports/02_cuda_kernel_profile.md`; real `.ncu-rep` artifacts should be added here only when collected on a CUDA machine.

Suggested commands:

```bash
ncu --set full --target-processes all ./build/vector_add_cuda_test
ncu --metrics dram__throughput.avg.pct_of_peak_sustained_elapsed,sm__throughput.avg.pct_of_peak_sustained_elapsed ./build/matrix_ops_cuda_test
```

Interpretation focus: DRAM throughput, SM throughput, achieved occupancy, warp stalls, shared-memory bank conflicts, and whether tiled variants reduce global-memory traffic relative to naive kernels.
