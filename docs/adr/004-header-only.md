# ADR 004: Header-Only Library Design

## Status
Accepted (2019)

## Context
Hash table implementations are heavily templated, with most logic in headers anyway.
Users have two common integration patterns:
1. Install a compiled `.so` / `.lib` and link against it
2. Drop a header into the project and `#include` it

## Decision
emhash is distributed as **header-only** files: just copy one `.hpp` into your project.

## Consequences

### Positive
- **Zero-config integration**: no build system, no linker, no ABI concerns
- The compiler can inline aggressively (critical for hash table performance)
- Users can easily customize for their specific key/value types
- Single-file distribution: no version mismatch between headers and library

### Negative
- Longer compile times (template instantiations in every translation unit)
- Cannot ship pre-compiled binaries
- API/ABI changes affect all users on upgrade

### Mitigations
- Forward declarations keep header dependencies minimal
- User can include only the version they need (hash_table5/6/7/8)
- CHANGELOG tracks all breaking changes

### Usage pattern
```bash
# Simplest: copy one header
cp hash_table7.hpp /your/project/

# Or: include path
git clone https://github.com/ktprime/emhash.git
g++ -I/path/to/emhash your_app.cpp
```
