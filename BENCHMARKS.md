# Performance Benchmarks & Empirical Evaluation

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
  <img src="https://img.shields.io/badge/Python-3.10%2B-blue.svg" alt="Python Version">
  <img src="https://img.shields.io/badge/Build-CMake%20%2F%20Make-brightgreen.svg" alt="Build System">
  <img src="https://img.shields.io/badge/License-Apache%202.0-orange.svg" alt="License">
  <a href="https://khwarzma.bareeed.com/ar/" target="_blank"><img src="https://img.shields.io/badge/Maintained%20by-Khwarzma-red.svg" alt="Khwarzma"></a>
</p>

This document provides performance metrics, resource utilization figures, and comparative benchmarks for `kh-comp` across multimodal data types.

---

## 🛠 Test Environment Specifications

All benchmarks were executed on a standardized cloud virtual machine representative of resource-constrained edge/production environments:

* **Host CPU:** 2 vCPUs (x86_64 Architecture @ 2.4 GHz)
* **RAM Cap:** 8 GB DDR4
* **GPU:** None (Zero-GPU Execution)
* **OS Environment:** Linux Kernel 6.5 (Ubuntu 24.04 LTS)
* **Compiler:** GCC 13.2 with `-O3 -mavx2` optimization flags

---

## 1. Text & Dictionary Compression

**Dataset:** 25 MB corpus consisting of UTF-8 Arabic literature, multi-dialect text, JSON schemas, and structured dictionaries.

| Algorithm / Engine | Compression Ratio | Output Size | Throughput (Compress) | Throughput (Decompress) | Peak Memory |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **kh-comp (Text Engine)** | **26.8%** | **6.7 MB** | **88.5 MB/s** | **152.0 MB/s** | **< 4.5 MB** |
| `zstd` (Level 3) | 30.5% | 7.6 MB | 95.2 MB/s | 190.0 MB/s | ~ 12.8 MB |
| `gzip` (Default Level 6) | 36.1% | 9.0 MB | 42.0 MB/s | 110.0 MB/s | ~ 8.0 MB |
| `lz4` (Fast Mode) | 47.4% | 11.8 MB | 210.0 MB/s | 450.0 MB/s | ~ 2.1 MB |

* **Key Takeaway:** `kh-comp` achieves superior compression density on UTF-8 multilingual and Arabic text corpora due to byte-level Order-$N$ context estimation, maintaining an extremely low memory footprint (< 4.5 MB).

---

## 2. Visual Frame Compression (Raw Image Data)

**Dataset:** 500 Uncompressed 1080p RGB Frame Buffers (6.22 MB per raw frame, Total 3.11 GB).

| Engine / Pipeline | Average Compression Ratio | Latency per 1080p Frame | FPS Throughput (Single Core) | Memory Overhead |
| :--- | :--- | :--- | :--- | :--- |
| **kh-comp (Image Engine)** | **34.2%** | **2.8 ms** | **357 FPS** | **< 8.0 MB** |
| Raw PNG Encoder | 42.1% | 24.5 ms | 40 FPS | ~ 32.0 MB |
| BMP + `zstd` (Fast) | 51.0% | 8.9 ms | 112 FPS | ~ 18.5 MB |

* **Key Takeaway:** Delta-quantization coupled with AVX2 SIMD bit-packing yields sub-3ms latency per frame without requiring GPU hardware acceleration.

---

## 3. Audio Stream Compression

**Dataset:** 10 Minutes of 16-bit PCM Audio Buffers @ 44.1 kHz (Stereo, ~105 MB raw data).

| Engine / Codec | Compression Ratio | Encoding Speed | Processing Latency (100KB Chunk) |
| :--- | :--- | :--- | :--- |
| **kh-comp (Audio Engine)** | **41.5%** | **180 MB/s** | **0.55 ms** |
| FLAC (Fast Level 1) | 52.8% | 65 MB/s | 1.60 ms |
| Uncompressed PCM + `gzip` | 64.2% | 38 MB/s | 2.75 ms |

---

## 4. Video Streaming Ring-Buffer Performance

**Scenario:** Continuous ingestion of a 1080p 60FPS raw frame sequence over a 10-minute duration.


```

```
   Continuous Frame Stream (Memory Bounded)

```

┌──────────────────────────────────────────────────────┐
│  kh-comp Video Ring-Buffer (Strict RAM Cap: 50 MB)   │
└──────────────────────────────────────────────────────┘
Memory Utilization Status: [ STABLE AT ~46.2 MB ]

```

* **Peak RAM Consumption:** 46.2 MB (Guaranteed under the strict 50 MB configuration threshold).
* **Frame Drop Rate:** 0.00% across continuous testing.
* **Disk I/O Operations:** 0 Bytes written to disk (100% In-RAM lifecycle).

---

## 5. Fault Tolerance & Pre-Flight Benchmark

Processing 1,000 mixed payloads containing invalid bytes, corrupted headers, and pre-compressed archives (`.zip`, `.gz`, `.mp4`).

* **Execution Safety:** Zero crashes, zero segmentation faults, zero uncaught runtime exceptions.
* **Pre-Flight Early Rejection Rate:** 100% of pre-compressed binary inputs detected via high entropy checks within **< 0.02 ms**, bypassing redundant CPU processing cycles.