# Technical Architecture & System Engineering

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
  <img src="https://img.shields.io/badge/Python-3.10%2B-blue.svg" alt="Python Version">
  <img src="https://img.shields.io/badge/Build-CMake%20%2F%20Make-brightgreen.svg" alt="Build System">
  <img src="https://img.shields.io/badge/License-Apache%202.0-orange.svg" alt="License">
  <a href="https://khwarzma.bareeed.com/ar/" target="_blank"><img src="https://img.shields.io/badge/Maintained%20by-Khwarzma-red.svg" alt="Khwarzma"></a>
</p>

This document outlines the internal mechanics, mathematical principles, memory layouts, and architectural guarantees of `kh-comp`.

---

## 1. System Pipeline Overview

`kh-comp` operates on a zero-copy, modular Facade architecture orchestrated by `khcomp::Engine`. Raw byte streams pass through entropy pre-flight checks before being dispatched to specialized processing units. All output bit sequences converge into cache-aligned hardware buffers.



```
                     [ Input Stream / std::span<const uint8_t> ]
                                         │
                                         ▼
                           ┌───────────────────────────┐
                           │   Entropy Pre-Flight      │
                           │   (Magic Bytes / Sh. Ent) │
                           └─────────────┬─────────────┘
                                         │
 ┌──────────────────┬────────────────────┼────────────────────┬──────────────────┐
 ▼                  ▼                    ▼                    ▼                  ▼


┌─────────┐        ┌─────────┐          ┌─────────┐          ┌─────────┐        ┌─────────┐
│  Text   │        │ Dictionary │       │  Image  │          │  Audio  │        │  Video  │
│ Engine  │        │ Engine     │       │ Engine  │          │ Engine  │        │ Engine  │
└────┬────┘        └────┬────┘          └────┬────┘          └────┬────┘        └────┬────┘
│                  │                    │                    │                  │
└──────────────────┴────────────────────┼────────────────────┴──────────────────┘
│
▼
┌───────────────────────┐
│   BitStream Writer    │
│ (alignas(64) Buffer)  │
└───────────────────────┘

```

---

## 2. Core Processing Modules

### A. Text & Structured Dictionary Engine (`khcomp::core::TextCompressor`)
* **Algorithm:** Adaptive Order-$N$ Context Probability Model ($N \in [1, 4]$) paired with a 32-bit precision Arithmetic Entropy Encoder.
* **Byte-Level Context Adaptation:** Specifically tuned for UTF-8 byte sequences. Multi-byte sequences common in Arabic and Semitic languages are tracked at byte-granularity rather than character-symbol trees, preventing state-explosion while maximizing symbol interval probability estimation.
* **Scaling & Renormalization:** Maintains dynamic frequency tables within bounded range $[0, 2^{16}-1]$. Renormalization shifts matching MSBs directly to the hardware bit-buffer in $\mathcal{O}(1)$ time without runtime memory allocation.

### B. Visual Frame Engine (`khcomp::core::ImageCompressor`)
* **Algorithm:** Spatial Redundancy Delta-Quantization + Dynamic Bit-Packing.
* **Mechanics:** Accepts raw pixel arrays (`RGB`, `RGBA`, `HSV`). Calculates horizontal and vertical spatial differences ($\Delta_{x, y} = P_{x, y} - P_{x-1, y}$) across adjacent channels.
* **Zero-GPU Footprint:** Applies SIMD-assisted (AVX2 / ARM NEON) variable-length bit packing to raw delta outputs. Eliminates heavy GPU transform pipelines, delivering sub-millisecond compression latencies.

### C. Audio Stream Engine (`khcomp::core::AudioCompressor`)
* **Algorithm:** Sub-band Differential PCM Quantization.
* **Mechanics:** Process raw 16-bit PCM or 32-bit Float audio streams. Calculates differential delta vectors across continuous time-domain samples while adaptively discarding unutilized low-amplitude bit depths.
* **Throughput Optimization:** Keeps sample processing bound to L1 cache lines, making it suitable for low-latency live audio serialization.

### D. Video Stream Ring-Buffer Engine (`khcomp::core::VideoRingBuffer`)
* **Algorithm:** Intra-Frame Ring-Buffered Compressor with In-RAM Cyclic Bounds.
* **Mechanics:** Manages a contiguous pre-allocated circular RAM region (default max limit: 50 MB, configurable via constructor).
* **Disk I/O Bypass:** Incoming video frames are encoded and decoded directly inside the circular buffer. Oldest frames are overwritten predictably upon memory cycle completion, guaranteeing system RAM consumption never exceeds the strict 50 MB threshold regardless of video duration or resolution.

---

## 3. BitStream Writer & Memory Management

### Cache-Line Alignment (`alignas(64)`)
The bit-stream writer operates on linear arrays aligned to CPU cache-line boundaries (64 bytes). This prevents false sharing in multi-core context evaluation and maximizes throughput on CPU store buffers.


```

```
   Cache Line (64 Bytes / 512 Bits)

```

├───────────────────────────────────────────────┤
[ Byte 0 ][ Byte 1 ][ Byte 2 ] ... [ Byte 63 ]
▲
└─ alignas(64) Pointer

```

### Strict Zero-Allocation Guarantee
* **Hot Paths:** Functions inside `compress_*` and `decompress_*` perform **zero** runtime calls to `malloc`, `free`, `new`, or dynamic vector resizing.
* **Pre-Sized State Buffers:** All frequency tables, delta buffers, and bitstreams are pre-allocated during `khcomp::Engine` initialization.
* **Memory Abstraction:** Data slices pass across library boundaries exclusively via `std::span<const uint8_t>` and `std::span<uint8_t>`.

---

## 4. Python Buffer Protocol & C-Extension Architecture

To prevent memory copying overhead between Python and C++ runtime layers:
1. Python objects (`bytes`, `bytearray`, `memoryview`) are exposed through Pybind11 via raw pointer pointers and sizes without array duplication.
2. C++ functions write outputs directly into memory allocated by Python buffer constructs.
3. Type stubs (`_native.pyi`) and `py.typed` markers enforce full static type safety for IDEs and type checkers like `mypy`.

---

## 5. Fault Tolerance & Progress Reporting

`kh-comp` eliminates runtime exceptions in core hot paths. Operations return `std::expected<T, CompressionError>` to handle bad data, stream corruption, or unsupported formats safely.

### Metric Reporting Structure (`CompressionReport`)
```cpp
struct CompressionReport {
    bool is_completed;
    size_t original_size;
    size_t compressed_size;
    double compression_ratio; // Percentage (%)
    double throughput_mbps;   // MB/s
    double latency_ms;        // Processing time in ms
    std::string status_message;
};

```

This report object is updated atomically after execution, permitting callers to query exact system performance and progress without thread lockouts or memory allocations.