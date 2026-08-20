# GGUF Quantization Notes

This note records the command boundary for later real-model execution. The current CPU-only baseline is the dependency-free `quantization/toy_quantizer.py` plus native GGUF dequant helpers in `native_core/gguf.cpp`.

## Reproducible Local Probe

```bash
python3 benchmarks/bench_quantization.py
./build/inferlab_quantization_probe --output results/raw/quantization_probe.json
./build/inferlab_quantization_sweep --output results/raw/quantization_sweep.json
./build/inferlab_quantization_channel_compare --output results/raw/quantization_channel_compare.json
```

## Evidence To Add On A Model Machine

Record model id, calibration set, group size, bits, activation dtype, memory footprint, prompt/eval tokens per second, and a fixed-prompt output diff against the FP16 baseline. Do not treat a tiny prompt set as a full accuracy conclusion.
