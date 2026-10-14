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
| [005](005-disable-prefetch-by-default.md) | Disable hardware prefetch by default in emhash5/7/8 | Accepted | 2026-07-19 |
| [006](006-bitmask-front-layout-emhash6-7.md) | Bitmask front layout for emhash6/7 | Accepted | 2026-07-19 |
| [007](007-emihmap4-swiss-table-experiment.md) | emihmap4 Swiss table experiment — findings and status | Experimental | 2026-07-19 |

## Conventions

- ADR files are named `NNN-short-title.md` (zero-padded sequence number)
- Status values: `Proposed` / `Accepted` / `Experimental` / `Deprecated` / `Superseded by [NNN]`
- Once accepted, ADRs are immutable; changes require a new ADR that supersedes the old one
- ADR format follows the lightweight template at <https://adr.github.io/>
