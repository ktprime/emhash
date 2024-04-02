# ahash-cxx

A variant of [Ahash](https://github.com/tkaitchuck/aHash/) written in C++.

## What are the differences from Ahash?

- Almost the same for most cases.
- Use `VAES` for large input size on `x86-64` targets
- Use `SVE2-AES` for large input size on `aarch64` targets
- Changed `aarch64` behavior to give more robust hash values.

## SMHasher?

**Yes, all tests are passed and the MomentChi2 result is `GREAT`.**

## Performance?

Can be much faster with `VAES` targets. On AMD 7773X, the throughput boosts from `~45000 MB/s` (w/o `VAES`) to `~95000 MB/s` (w/ `VAES`).

## Notice

The hash value is not stable across different platforms and may also vary with different versions. DO NOT USE this hash function as checksums for on-disk files. 
`AHash` family are designed for in-memory hash tables. 

## What is missing?

- `SVE2-AES` can not be tested properly. There is no accessible machine with `sve2-aes` for me and for now.
