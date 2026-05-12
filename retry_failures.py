#!/usr/bin/env python3
"""retry_failures.py — Re-run only the queries that failed during the
previous bulk_generate.py run.

Reads bulk_generate.log, finds every `[N/total]` line followed by a
`✗ failure / timeout / exception` line, then re-runs QUERIES[N-1] from
bulk_generate.py via spotify2beat.py.

Logs to bulk_retry.log so the original run log stays intact.

Usage:
    .venv/bin/python retry_failures.py
    .venv/bin/python retry_failures.py --no-stems   # faster, less accurate
"""

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
PYTHON = ROOT / ".venv/bin/python"

sys.path.insert(0, str(ROOT))
from bulk_generate import QUERIES


def parse_failed_indices(log_path: Path) -> list[int]:
    """Return the QUERIES indices (1-based) for each failed entry in the log.

    Logs use one of two header formats:
      [N/total] <query>...                 # original bulk_generate.log
      [N/total] (orig #M) <query>...       # retry_failures.py output

    For the second form, M is the QUERIES index. For the first, N already is.
    """
    text = log_path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    failed = []
    head_re = re.compile(r"^\[(\d+)/\d+\](?:\s+\(orig\s+#(\d+)\))?")
    fail_re = re.compile(r"^\s*✗\s+(failure|timeout|exception)")

    for i, line in enumerate(lines):
        m = head_re.match(line)
        if not m:
            continue
        idx = int(m.group(2) if m.group(2) else m.group(1))
        for j in range(i + 1, min(i + 8, len(lines))):
            nxt = lines[j]
            if not nxt.strip():
                continue
            if head_re.match(nxt):
                break
            if fail_re.match(nxt):
                failed.append(idx)
                break
            if "✓ success" in nxt:
                break
    return failed


def main():
    ap = argparse.ArgumentParser(description="Retry only failed bulk-generate songs.")
    ap.add_argument("--input",  default="bulk_generate.log",
                    help="Source log to parse for failed entries")
    ap.add_argument("--output", default="bulk_retry.log",
                    help="Where to write this run's log")
    ap.add_argument("--no-stems", action="store_true",
                    help="Skip Demucs stem separation")
    ap.add_argument("--dry-run", action="store_true",
                    help="Print what would be re-run without executing")
    args = ap.parse_args()

    log_in  = ROOT / args.input
    log_out = ROOT / args.output

    if not log_in.exists():
        print(f"Missing {log_in}", file=sys.stderr)
        sys.exit(1)

    indices = parse_failed_indices(log_in)
    print(f"Found {len(indices)} failed entries to retry", flush=True)

    retries = [(idx, QUERIES[idx - 1]) for idx in indices if 1 <= idx <= len(QUERIES)]
    if args.dry_run:
        for idx, (q, tags) in retries:
            print(f"[{idx}] {q}  ({tags})")
        return

    log = open(log_out, "w", encoding="utf-8")
    log.write(f"Starting retry at {time.strftime('%Y-%m-%d %H:%M:%S')} — {len(retries)} queries\n")
    log.flush()

    successes = failures = 0
    start = time.time()
    total = len(retries)

    for n, (idx, (query, tags)) in enumerate(retries, 1):
        elapsed = time.time() - start
        rate = n / max(elapsed, 1.0)
        eta_min = (total - n) / max(rate, 1e-3) / 60.0
        prefix = (f"[{n}/{total}] (orig #{idx}) {query[:50]:50s} | "
                  f"tags: {tags[:40]:40s} | ETA {eta_min:5.0f} min")
        print(prefix, flush=True)
        log.write(f"\n{prefix}\n")
        log.flush()

        cmd = [str(PYTHON), "spotify2beat.py", query, "--auto", "--genre", tags]
        if not args.no_stems:
            cmd.append("--stems")

        try:
            result = subprocess.run(cmd, capture_output=True, text=True,
                                    timeout=420, cwd=ROOT)
            if result.returncode == 0:
                successes += 1
                log.write("  ✓ success\n")
            else:
                failures += 1
                log.write(f"  ✗ failure (rc={result.returncode})\n")
                if result.stderr:
                    log.write(f"    stderr: {result.stderr[:400]}\n")
        except subprocess.TimeoutExpired:
            failures += 1
            log.write("  ✗ timeout (>7 min)\n")
        except Exception as e:
            failures += 1
            log.write(f"  ✗ exception: {e}\n")
        log.flush()

    total_min = (time.time() - start) / 60.0
    summary = f"\nDone in {total_min:.1f} min — {successes} ok, {failures} failed (of {total})\n"
    print(summary, flush=True)
    log.write(summary)
    log.close()


if __name__ == "__main__":
    main()
