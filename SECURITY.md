# Security Policy

## Supported Versions

| Version | Supported          |
| ------- | ------------------ |
| 1.1.x   | :white_check_mark: |
| < 1.0   | :x:                |

## Reporting a Vulnerability

emhash is a header-only hash table library. Most security concerns relate to:

- **Hash collision attacks**: emhash7/8 do not limit probe sequence length by default, making them potentially vulnerable to hash flooding DoS when accepting untrusted input. For mitigation options, see [docs/usage_notes.md](docs/usage_notes.md). The `emilib` variants (e.g., `emilib2ss`) provide `EMH_SAFE_PSL` compile-time options to cap probe lengths. `EMH_HIGH_LOAD` is a separate feature for high load factor support, not a flood protection mechanism.
- **Memory safety**: Use AddressSanitizer (`-fsanitize=address`) and UndefinedBehaviorSanitizer (`-fsanitize=undefined`) to detect issues.

To report a security vulnerability:

1. **Do not** open a public GitHub issue.
2. Email the maintainer at `bailuzhou AT 163.com` with details.
3. Include: emhash version, compiler/OS, minimal reproducible example, and potential impact.

You can expect a response within 7 days. If the vulnerability is confirmed, a fix will be released as soon as possible.
