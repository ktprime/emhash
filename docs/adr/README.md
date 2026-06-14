# Architecture Decision Records (ADR)

This directory contains the architectural decision records for the emhash project.
Each ADR documents an important design decision, its context, and its consequences.

## Index

| Number | Title | Status | Date |
|--------|-------|--------|------|
| [001](001-use-open-addressing.md) | Use open addressing instead of chaining | Accepted | 2019 |
| [002](002-split-index-for-emhash8.md) | emhash8: separate index + dense pairs | Accepted | 2020 |
| [003](003-no-tombstones-emhash7.md) | emhash7: no-tombstone linked-bucket design | Accepted | 2020 |
| [004](004-header-only.md) | Header-only library design | Accepted | 2019 |

## Conventions

- ADR files are named `NNN-short-title.md` (zero-padded sequence number)
- Status values: `Proposed` / `Accepted` / `Deprecated` / `Superseded by [NNN]`
- Once accepted, ADRs are immutable; changes require a new ADR that supersedes the old one
- ADR format follows the lightweight template at <https://adr.github.io/>
