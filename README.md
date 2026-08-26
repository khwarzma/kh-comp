### kh-comp

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
  <img src="https://img.shields.io/badge/Python-3.10%2B-blue.svg" alt="Python Version">
  <img src="https://img.shields.io/badge/Build-CMake%20%2F%20Make-brightgreen.svg" alt="Build System">
  <img src="https://img.shields.io/badge/License-Apache%202.0-orange.svg" alt="License">
  <a href="https://khwarzma.bareeed.com/ar/" target="_blank"><img src="https://img.shields.io/badge/Maintained%20by-Khwarzma-red.svg" alt="Khwarzma"></a>
</p>

**kh-comp** is a deterministic, ultra-lightweight multimodal streaming and compression engine written in modern **C++23** with zero-copy **Python** bindings. 

Engineered primarily as the high-throughput compression layer for the **Khwarzma** system ecosystem, `kh-comp` provides specialized streaming encoders for **Text, Audio, Visual Frames, and Video Streams**. It is specifically optimized for resource-constrained production hosts (e.g., **2 vCPUs, 8 GB RAM, Zero-GPU reliance**), delivering high density, minimal CPU latency, and strict heap-allocation isolation.

---

## 🚀 Key Features

* **Multimodal Compression Core:** Specialized engines for Text/Dictionaries, Images (Raw RGB/RGBA), Audio PCM, and Real-Time Video Ring-Buffers.
* **Strict Zero-Allocation Hot-Paths:** Critical compression and decompression loops execute without dynamic heap allocation (`malloc`/`new`), preventing GC latency spikes and memory fragmentation.
* **Zero-Copy Python Buffer Protocol:** Interoperates seamlessly with Python (`bytes`, `bytearray`, `memoryview`) via C++23 `std::span`, avoiding redundant `memcpy` overhead.
* **Ring-Buffered Video Streaming:** Fixed-size memory window (default 50MB) for processing frame sequences with zero disk I/O bottlenecks.
* **Entropy Pre-Flight & Fault Tolerance:** Detects pre-compressed or high-entropy streams (e.g., `.zip`, `.mp4`) to avoid compression degradation, returning structured progress and metrics reports safely via `std::expected`.

---

## 🛠 Architectural Overview


```

```
                    ┌───────────────────────────────────────────┐
                    │              khcomp::Engine               │
                    └─────────────────────┬─────────────────────┘
                                          │
 ┌──────────────────┬─────────────────────┼─────────────────────┬──────────────────┐
 ▼                  ▼                     ▼                     ▼                  ▼

```

┌─────────┐        ┌─────────┐           ┌─────────┐           ┌─────────┐        ┌─────────┐
│  Text   │        │ Dictionary │        │  Image  │           │  Audio  │        │  Video  │
│ Engine  │        │ Engine     │        │ Engine  │           │ Engine  │        │ Engine  │
└────┬────┘        └────┬────┘           └────┬────┘           └────┬────┘        └────┬────┘
     │                  │                     │                     │                  │
     └──────────────────┴─────────────────────┼─────────────────────┴──────────────────┘
                                              │
                                              ▼
                                    ┌───────────────────────┐
                                    │   BitStream Writer    │
                                    │ (alignas(64) Buffer)  │
                                    └───────────────────────┘

```

---

## 📦 Installation & Build

### Prerequisites
* **C++ Compiler:** GCC 13+ or Clang 16+ (C++23 support required)
* **Build System:** CMake 3.22+
* **Python:** Python 3.10+ (optional, for Python bindings)

### Python Package Installation
```bash
pip install khcomp

```

### Native C++ Build

```bash
git clone [https://github.com/khwarzma/kh-comp.git](https://github.com/khwarzma/kh-comp.git)
cd kh-comp
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

```

---

## 💻 Quickstart Examples

### Python API

```python
import khcomp

# Initialize engine with custom Video Ring Buffer (default 50MB)
engine = khcomp.Engine(max_ring_buffer_mb=50)

# 1. Text & Structured Data Compression (Order-N Context Model)
arabic_text = "نص تجريبي لاختبار كفاءة الضغط في محرك خوارزمة"
compressed_text = engine.compress_text(arabic_text)
decompressed_text = engine.decompress_text(compressed_text)

# 2. Raw Image Compression (Delta Encoding + Bit Packing)
with open("raw_frame.rgb", "rb") as f:
    raw_rgb_data = f.read()

compressed_img = engine.compress_image(raw_rgb_data, width=1920, height=1080)
decompressed_img = engine.decompress_image(compressed_img)

# 3. Audio Stream Compression (16-bit PCM Sub-band Delta)
compressed_audio = engine.compress_audio(pcm_bytes, sample_rate=44100)
report = engine.get_last_report()

print(f"Ratio: {report.compression_ratio:.2f}% | Speed: {report.throughput_mbps:.1f} MB/s")

```

### C++23 API

```cpp
#include <khcomp/comp_engine.hpp>
#include <iostream>
#include <string_view>

int main() {
    khcomp::Engine engine;
    std::string_view payload = "Khwarzma Multimodal High-Performance Core";

    auto result = engine.compress_text(payload);
    if (result.has_value()) {
        auto compressed = result.value();
        std::cout << "Original: " << payload.size() << " bytes\n";
        std::cout << "Compressed: " << compressed.size() << " bytes\n";
    }

    return 0;
}

```

---

## 📚 Documentation Index

* [ARCHITECTURE.md](ARCHITECTURE.md) — Deep dive into multimodal engines, BitStream alignment, and memory layouts.
* [BENCHMARKS.md](https://www.google.com/search?q=BENCHMARKS.md) — Performance metrics for Text, Image, Audio, and Video streaming.
* [CONTRIBUTING.md](https://www.google.com/search?q=CONTRIBUTING.md) — C++23 code standards, testing policies, and submission procedures.
* [AI_INSTRUCTIONS.md](https://www.google.com/search?q=AI_INSTRUCTIONS.md) — Directives and architectural boundaries for AI code assistants.