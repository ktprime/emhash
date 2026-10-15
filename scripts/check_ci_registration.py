#!/usr/bin/env python3
"""CI registration consistency check.

Every test source under tests/unit and tests/memory must be both compiled and
run in all four comprehensive CI jobs (build, asan, coverage, windows-msvc).
Tests added to tests/ but not registered in .github/workflows/ci.yml are the
most common CI gap (it happened twice); this turns that into a hard gate
instead of a review catch.

Stress/attack tests are platform/scale-gated deliberately; they are only
required to be referenced by the windows-msvc build targets (the most
complete list). Jobs with deliberately partial lists (compat, tsan, msan,
i386, ...) are not checked.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CI = ROOT / ".github" / "workflows" / "ci.yml"
TESTS = ROOT / "tests"

STRICT_JOBS = ["build", "asan", "coverage", "windows-msvc"]
STRICT_CATS = ("unit", "memory")
WEAK_CATS = ("stress", "attack")  # must be referenced by windows-msvc only

SRC_RE = re.compile(r"\b(?:unit|memory|stress|attack|debug|fuzz)/(test_[\w]+)")
RUN_RE = re.compile(r"\./(test_[\w]+?)(?:_asan|_cov)?\b")
TARGET_LINE_RE = re.compile(r"--target\s+(.+)$", re.M)


def job_sections(text):
    """Split ci.yml into {job_id: section} on 2-space-indented job keys."""
    sections, current, buf = {}, None, []
    for line in text.splitlines():
        m = re.match(r"^  ([a-zA-Z0-9_-]+):\s*$", line)
        if m:
            if current:
                sections[current] = "\n".join(buf)
            current, buf = m.group(1), []
        elif current is not None:
            buf.append(line)
    if current:
        sections[current] = "\n".join(buf)
    return sections


def references(section):
    """Return (compiled, ran) test-name sets referenced by a job section."""
    flat = section.replace("\\\n", " ")  # join bash line continuations
    compiled = set(SRC_RE.findall(flat))
    for m in TARGET_LINE_RE.finditer(flat):
        compiled.update(re.findall(r"\b(test_[\w]+)", m.group(1)))
    ran = set(RUN_RE.findall(flat))
    for m in re.finditer(r"for t in ([^;]+);", flat):
        ran.update(re.findall(r"\b(test_[\w]+)", m.group(1)))
    return compiled, ran


def main():
    strict = {cat: sorted(p.stem for p in (TESTS / cat).glob("test_*.cpp")) for cat in STRICT_CATS}
    weak = [p.stem for cat in WEAK_CATS for p in sorted((TESTS / cat).glob("test_*.cpp"))]

    sections = job_sections(CI.read_text())
    errors = []

    for job in STRICT_JOBS:
        if job not in sections:
            errors.append(f"job '{job}' not found in ci.yml")
            continue
        compiled, ran = references(sections[job])
        for cat, names in strict.items():
            for name in names:
                if name not in compiled:
                    errors.append(f"{job}: {cat}/{name}.cpp is never compiled")
                elif name not in ran:
                    errors.append(f"{job}: {cat}/{name}.cpp is compiled but never run")

    if "windows-msvc" in sections:
        win_compiled, win_ran = references(sections["windows-msvc"])
        for name in weak:
            if name not in win_compiled and name not in win_ran:
                errors.append(f"windows-msvc: stress/attack test '{name}' is not referenced")

    if errors:
        print("CI registration check FAILED:")
        for e in errors:
            print("  -", e)
        print(f"\nFix: register the tests in .github/workflows/ci.yml "
              f"(build loop + run list, asan, coverage, windows-msvc targets).")
        return 1

    total = sum(len(v) for v in strict.values()) + len(weak)
    print(f"CI registration check OK: {total} tests "
          f"({', '.join(f'{len(v)} {c}' for c, v in strict.items())}, {len(weak)} stress/attack) "
          f"registered in {len(STRICT_JOBS)} strict jobs.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
