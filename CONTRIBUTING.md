# Contributing to emhash

Thank you for your interest in contributing to emhash! This document provides guidelines for contributions.

## Quick Start

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes
4. Run tests: `cd tests && make quick`
5. Submit a pull request

## Build and Test

```bash
# Quick verification (~20 seconds)
cd tests && make quick

# Full verification
cd tests && make all

# With specific compiler
make quick CXX=clang++

# CMake build
cd tests && cmake -B build && cmake --build build
```

## Commit Messages

Follow [Conventional Commits](https://www.conventionalcommits.org/):

```
<type>(<scope>): <subject>

<body>
```

**Types**: `feat`, `fix`, `perf`, `docs`, `test`, `refactor`, `build`, `ci`, `chore`

**Scopes**: `hash_table5`, `hash_table6`, `hash_table7`, `hash_table8`, `hash_set`, `emilib`, `bench`, `test`, `docs`

Examples:
```
feat(hash_table8): add heterogeneous lookup support
fix(hash_table5): resolve iterator invalidation after rehash
perf(hash_table7): optimize probe sequence for cache locality
docs: update README with new benchmark results
test: add coverage for extreme load factor
```

## Code Style

- C++17 standard
- Use `.clang-format` configuration in the project root
- Run `clang-format -i <file>` before committing
- Header files use `#pragma once`
- Template parameters use `CamelCase`
- Member functions use `snake_case`

## Pull Request Checklist

- [ ] Code compiles without warnings on gcc and clang
- [ ] Existing tests pass (`make quick`)
- [ ] New functionality includes tests
- [ ] Performance-sensitive changes include benchmarks
- [ ] No regression in existing benchmarks
- [ ] Commit messages follow conventional commits format
- [ ] Public API is documented

## Reporting Bugs

When filing a bug report, please include:

1. **emhash version** (e.g., hash_table5, hash_table8)
2. **Compiler and version** (e.g., gcc 13.2, clang 18)
3. **Operating system** (e.g., Ubuntu 22.04, macOS 14)
4. **Minimal reproducible example**
5. **Expected vs actual behavior**

## Performance Contributions

When submitting performance improvements:

1. Include benchmark numbers before and after
2. Use the existing benchmark suite (`tests/bench/`)
3. Test with multiple compilers
4. Report ops/sec, not just relative improvement

## License

By contributing, you agree that your contributions will be licensed under the MIT License.
