# Developer Guidelines & Contribution Standards

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
  <img src="https://img.shields.io/badge/Python-3.10%2B-blue.svg" alt="Python Version">
  <img src="https://img.shields.io/badge/Build-CMake%20%2F%20Make-brightgreen.svg" alt="Build System">
  <img src="https://img.shields.io/badge/License-Apache%202.0-orange.svg" alt="License">
  <a href="https://khwarzma.bareeed.com/ar/" target="_blank"><img src="https://img.shields.io/badge/Maintained%20by-Khwarzma-red.svg" alt="Khwarzma"></a>
</p>

Thank you for considering contributing to **`kh-comp`**. This document defines the architectural rules, coding standards, memory constraints, and testing protocols required for all contributions.

---

## 1. Architectural Guardrails & Strict Rules

All code contributions modifying `src/` or `include/` must strictly comply with the following architectural invariants:

### A. Zero-Allocation Hot Paths
* **No Runtime Dynamic Memory Allocation:** Functions in hot paths (`compress_*`, `decompress_*`, bit-stream ops) MUST NOT invoke `malloc`, `free`, `new`, `delete`, or operations causing `std::vector` reallocations[cite: 1, 4].
* **Pre-Allocated Buffers:** All temporary states, frequency models, and delta buffers must be sized during system initialization or passed via pre-allocated spans (`std::span<uint8_t>`)[cite: 1, 4].

### B. C++23 Modern Idioms & Exception Isolation
* **Standard Enforcement:** Strict ISO C++23 standard execution[cite: 1]. Legacy C++ constructs (pre-C++20) or raw pointer ownership management are forbidden[cite: 1, 4].
* **No Runtime Exceptions in Core:** The core engine (`src/core/`) must avoid throwing runtime C++ exceptions[cite: 1]. Use `std::expected<T, CompressionError>` for deterministic error propagation[cite: 1, 4].
* **Const & Constexpr Correctness:** Mark functions `noexcept`, `const`, and `constexpr` wherever mathematically and logically applicable[cite: 1].

### C. Dependency Isolation & Hardware Boundaries
* **Zero External Third-Party Core Dependencies:** The core compression engine in `src/core/` must depend solely on the C++23 standard library[cite: 1].
* **Pybind11 Isolation:** Python binding logic must remain strictly confined to `src/bindings/pybind_wrapper.cpp`[cite: 1].
* **Resource Target Guarantee:** All algorithmic code must run efficiently on dual-core CPU architectures (2 vCPU / 8 GB RAM profiles) without requiring GPU dependencies[cite: 1].

---

## 2. Naming & Coding Conventions

Adhere strictly to the naming schema defined below:

| Entity Type | Convention | Example |
| :--- | :--- | :--- |
| **Classes & Structs** | `PascalCase` | `TextCompressor`, `CompressionReport`[cite: 4] |
| **Functions & Methods** | `snake_case` | `compress_image()`, `get_last_report()`[cite: 4] |
| **Member Variables** | `m_snake_case` | `m_frequency_table`, `m_ring_buffer`[cite: 4] |
| **Constants & Enums** | `kPascalCase` or `UPPER_SNAKE` | `kMaxBufferCap`, `INVALID_DATA`[cite: 4] |
| **Namespaces** | `snake_case` | `khcomp::core`, `khcomp::bindings` |

### Formatting Standard
* **Indent:** 4 spaces (No hard tabs).
* **Line Length:** 100 characters maximum.
* **Header Guard:** Use `#pragma once` across all C++ headers inside `include/khcomp/`.

---

## 3. Local Development, Build & Testing Workflow

### A. Setting Up Development Environment

```bash
# Clone the repository recursively
git clone [https://github.com/khwarzma/kh-comp.git](https://github.com/khwarzma/kh-comp.git)
cd kh-comp

# Create virtual environment and install in editable mode with debug symbols
python3 -m venv venv
source venv/bin/activate
pip install -e .[dev]

```

### B. Building Native C++ with Sanitizers (ASAN / UBSAN)

Always test debug builds with AddressSanitizer and UndefinedBehaviorSanitizer enabled:

```bash
# Configure debug build with sanitizers enabled
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-debug -j$(nproc)

# Run C++ GoogleTest / CTest suite
ctest --test-dir build-debug --output-on-failure

```

### C. Running Python Test Suites

Verify Python bindings and buffer protocol integrations:

```bash
# Run pytest for integration and type safety checks
pytest tests/python/ -v

```

---

## 4. Pull Request & Commit Message Policy

### Commit Message Format

Commits must follow the Conventional Commits specification:

```
<type>(<scope>): <short description>

[optional body]

```

* **Types:** `feat`, `fix`, `perf`, `refactor`, `docs`, `test`, `build`.
* **Scopes:** `core`, `bindings`, `text`, `image`, `audio`, `video`, `ci`.
* **Example:** `perf(core): optimize cache-line aligned bitstream store loop`

### Pull Request Checklist

Before submitting a Pull Request, confirm:

1. All C++ unit tests and Python integration tests pass cleanly.


2. Zero memory leaks detected by AddressSanitizer (ASAN).
3. If modifying performance-critical code in `src/core/`, performance benchmarks in `BENCHMARKS.md` must be re-run and documented.


4. Type stubs in `khcomp/_native.pyi` are updated to match any C-extension changes.
