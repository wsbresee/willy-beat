#!/usr/bin/env python3
"""enrich_tags_llm.py — Per-pattern LLM enrichment using the local claude
CLI (`claude -p`). Asks Claude to suggest 6-10 semantic tags that wouldn't
be reachable by the rule-based table in enrich_tags.py: artist, era,
mood/feel, regional scene, cultural moment.

Idempotent via a side log .llm_enriched.json that records which files
have already been processed. Re-running skips done files unless --force.

Usage:
    .venv/bin/python enrich_tags_llm.py --dir Presets
    .venv/bin/python enrich_tags_llm.py --dir Presets --limit 5      # dry-ish smoke test
    .venv/bin/python enrich_tags_llm.py --dir Presets --workers 4    # parallel calls
"""

import argparse
import concurrent.futures
import json
import re
import subprocess
import sys
import time
from pathlib import Path


GENRE_RE = re.compile(r"^genres:\s*(.*)$", re.MULTILINE)
NAME_RE  = re.compile(r"^name:\s*(.*)$",   re.MULTILINE)
TYPE_RE  = re.compile(r"^type:\s*(.*)$",   re.MULTILINE)

PROMPT_TEMPLATE = """\
You are tagging a drum pattern in WillyBeat (a drum machine). Users find \
patterns by typing genre/mood/vibe words; tags drive a semantic vector \
search, so good tags help users discover the pattern.

Pattern name: {name}
Type:         {ptype}
Existing tags: {existing}

Suggest 6 to 10 ADDITIONAL semantic tags that would help users find this \
pattern. Focus on dimensions not already covered:
- Artist or producer style (if recognisable from the name)
- Era / decade / scene
- Mood, feel, energy
- Adjacent subgenres
- Cultural moment, use-case, vibe words people actually type

Rules:
- DO NOT repeat any existing tag.
- Each tag should be 1-3 words. Use canonical capitalisation (e.g. \
"Boom Bap", "80s", "Stadium Rock").
- No quotes, no preamble, no bullets, no trailing punctuation.
- Output a single line: comma-separated tags only.
"""


def parse_field(text: str, regex: re.Pattern) -> str:
    m = regex.search(text)
    return m.group(1).strip() if m else ""


def call_claude(prompt: str, timeout: int = 60) -> str:
    """Run the claude CLI in print mode and return the raw stdout."""
    result = subprocess.run(
        ["claude", "-p", "--output-format", "text"],
        input=prompt,
        text=True,
        capture_output=True,
        timeout=timeout,
    )
    if result.returncode != 0:
        raise RuntimeError(f"claude exited {result.returncode}: {result.stderr.strip()}")
    return result.stdout.strip()


def parse_tags(raw: str) -> list[str]:
    """Take the model's reply and pull out a clean tag list."""
    # Strip code fences if the model added them.
    raw = raw.strip().strip("`")
    # Take the first non-empty line — sometimes the model adds an extra blank.
    for line in raw.splitlines():
        line = line.strip()
        if line:
            raw = line
            break
    parts = [p.strip().strip("\"'.;:") for p in raw.split(",")]
    # Filter junk: empty, too long, surrounded by markdown noise.
    return [p for p in parts if p and len(p) <= 40 and not p.startswith("-")]


def enrich_one(path: Path, force: bool, log: dict, lock_print) -> tuple[str, list[str]]:
    text = path.read_text(encoding="utf-8")
    name = parse_field(text, NAME_RE) or path.stem
    ptype = parse_field(text, TYPE_RE) or "Regular"
    existing_raw = parse_field(text, GENRE_RE)
    existing = [t.strip() for t in existing_raw.split(",") if t.strip()]

    if not force and path.name in log:
        return path.name, []

    prompt = PROMPT_TEMPLATE.format(
        name=name,
        ptype=ptype,
        existing=", ".join(existing) or "(none)",
    )
    try:
        raw = call_claude(prompt)
    except Exception as e:
        lock_print(f"  ✗ {path.name}: {e}")
        return path.name, []

    new_tags = parse_tags(raw)
    # Drop dupes against existing (case-sensitive — keep model casing).
    new_tags = [t for t in new_tags if t not in existing]
    if not new_tags:
        lock_print(f"  · {path.name}: no new tags")
        return path.name, []

    merged = existing + new_tags
    new_line = "genres: " + ", ".join(merged)
    new_text = GENRE_RE.sub(new_line, text, count=1)
    path.write_text(new_text, encoding="utf-8")
    lock_print(f"  ✓ {path.name}: +{len(new_tags)}  ({', '.join(new_tags)})")
    return path.name, new_tags


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dir", type=Path, required=True, help=".beat folder")
    ap.add_argument("--workers", type=int, default=4, help="parallel claude calls")
    ap.add_argument("--limit", type=int, default=0, help="cap at N files (0 = all)")
    ap.add_argument("--force", action="store_true", help="reprocess files in the log")
    ap.add_argument("--log", type=Path, default=Path(".llm_enriched.json"))
    args = ap.parse_args()

    if not args.dir.is_dir():
        raise SystemExit(f"Not a directory: {args.dir}")

    log: dict = {}
    if args.log.exists():
        log = json.loads(args.log.read_text())

    files = sorted(args.dir.glob("*.beat"))
    if not args.force:
        todo = [f for f in files if f.name not in log]
    else:
        todo = list(files)
    if args.limit:
        todo = todo[: args.limit]

    print(f"Enriching {len(todo)} / {len(files)} files via claude -p "
          f"(workers={args.workers}, force={args.force})")

    import threading
    plock = threading.Lock()
    def lock_print(msg: str) -> None:
        with plock:
            print(msg, flush=True)

    t0 = time.time()
    done = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as ex:
        futures = {ex.submit(enrich_one, f, args.force, log, lock_print): f for f in todo}
        for fut in concurrent.futures.as_completed(futures):
            name, new_tags = fut.result()
            log[name] = {"added": new_tags, "ts": int(time.time())}
            done += 1
            if done % 10 == 0:
                args.log.write_text(json.dumps(log, indent=2))
                lock_print(f"  ── progress {done}/{len(todo)} "
                           f"({(done / len(todo)) * 100:.0f}%, "
                           f"{time.time() - t0:.0f}s elapsed)")

    args.log.write_text(json.dumps(log, indent=2))
    print(f"Done. Processed {done} files in {time.time() - t0:.0f}s.")


if __name__ == "__main__":
    main()
