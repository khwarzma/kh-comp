# Directives & Architectural Constraints for Engineers & AI Agents (.cursorrules)

<p align="center">
  <img src="https://img.shields.io/badge/C%2B%2B-23-blue.svg" alt="C++23">
  <img src="https://img.shields.io/badge/Python-3.10%2B-blue.svg" alt="Python Version">
  <img src="https://img.shields.io/badge/Build-CMake%20%2F%20Make-brightgreen.svg" alt="Build System">
  <img src="https://img.shields.io/badge/License-Apache%202.0-orange.svg" alt="License">
  <a href="https://khwarzma.bareeed.com/ar/" target="_blank"><img src="https://img.shields.io/badge/Maintained%20by-Khwarzma-red.svg" alt="Khwarzma"></a>
</p>

This file defines strict architectural boundaries, memory safety rules, and code generation standards for both **Human Engineers** and **AI Code Generation Agents** (Cursor, Claude, Copilot, ChatGPT) interacting with the `kh-comp` repository[cite: 1].

---

## 1. Fundamental System Directives

1. **Strict ISO C++23 Standard:** Write modern C++23 exclusively[cite: 1]. Do not generate legacy constructs (pre-C++20), raw `malloc`/`free`, or manual pointer management[cite: 1, 4].
2. **Strict Zero-Allocation in Hot Paths:**
   * Critical compression/decompression loops in `src/core/` MUST NOT allocate dynamic heap memory (`new`, `malloc`, `std::vector::push_back` reallocations)[cite: 1, 4].
   * All state tables, context probability maps, and bit buffers must be pre-sized during setup or passed via `std::span`[cite: 1, 4].
3. **Dependency Isolation:**
   * Core logic in `src/core/` and `include/khcomp/` must remain 100% independent of third-party libraries[cite: 1].
   * Pybind11 interactions must be strictly restricted to `src/bindings/pybind_wrapper.cpp`[cite: 1].
4. **Exception-Free Safety & Const Correctness:**
   * Mark critical hot-path functions `noexcept`, `const`, and `constexpr` wherever logically permissible[cite: 1].
   * Use `std::expected<T, CompressionError>` for deterministic error propagation instead of runtime exceptions[cite: 1, 4].
5. **Hardware Boundary Target:** Maintain code paths optimized for dual-core CPU architectures (2 vCPU / 8 GB RAM profiles) without GPU dependencies[cite: 1].

---

## 2. Code Generation Boundaries & File Map

AI Agents and Engineers MUST strictly respect permitted file modifications:


```

kh-comp/
├── include/khcomp/          ──► Public C++23 header signatures and zero-copy types
│   ├── bit_stream.hpp       ──► Cache-line aligned bit writer (alignas(64))
│   ├── comp_engine.hpp      ──► Unified Facade Engine interface
│   ├── context_model.hpp    ──► Order-N frequency estimation models
│   └── utils.hpp            ──► Zero-allocation helper functions
├── src/core/                ──► High-performance C++23 implementations
├── src/bindings/            ──► Pybind11 zero-copy wrapper implementation
├── khcomp/                  ──► Python package layer (_native.pyi, py.typed)
└── tests/                   ──► Unit and integration test suites

```

---

## 3. Strict Code Patterns & Anti-Patterns

### ✅ Permitted Patterns (Do This)
* Use `std::span<const uint8_t>` for immutable memory input views[cite: 1, 4].
* Use `std::span<uint8_t>` for mutable target buffer outputs[cite: 1, 4].
* Align physical bitstream storage to 64-byte boundaries (`alignas(64)`)[cite: 2].
* Use `std::expected` to handle input errors or invalid formats gracefully[cite: 1, 4].

### ❌ Forbidden Anti-Patterns (NEVER Do This)
* **DO NOT** use `new`, `delete`, `malloc()`, or `free()` inside core loops[cite: 1, 4].
* **DO NOT** throw C++ runtime exceptions (`throw std::runtime_error`) in `src/core/`[cite: 1].
* **DO NOT** perform implicit memory copies (`memcpy` or assignment) when passing Python buffers[cite: 2].
* **DO NOT** add third-party `#include` headers inside core algorithms[cite: 1].

---

## 4. Verification & Self-Check Protocol

Before committing or outputting code blocks, AI agents and engineers MUST execute this validation checklist:

1. **Allocation Check:** Does this implementation execute without invoking heap allocation during streaming?
2. **Type Safety:** Are Python type stubs in `khcomp/_native.pyi` synchronized with any binding signature changes?
3. **Memory Safety:** Does the code pass AddressSanitizer (ASAN) without memory leaks or unaligned access?
4. **Interface Consistency:** Does the C++ facade method map directly to `khcomp.Engine` in Python?