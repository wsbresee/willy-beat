#!/usr/bin/env python3
"""migrate_v1_to_v2.py — One-shot migration that converts every v1 .beat
preset into the v2 file format AND scrubs all references to specific
artists, albums, and song titles.

Pipeline per file:
  1. Parse the v1 grid + genres line.
  2. Convert the 16-step grid to tick-based hits at PPQN=96.
  3. Call `claude -p` with the pattern's rhythmic profile and existing
     tags. Claude returns a new descriptive name (no proper nouns) and
     a cleaned tag list restricted to a controlled vocabulary of
     genre/mood/feel words.
  4. Emit a v2 .beat file with timesig=4/4, bars=1.

Idempotent via .migration_v2.json log keyed on the source filename.

Usage:
    .venv/bin/python migrate_v1_to_v2.py
    .venv/bin/python migrate_v1_to_v2.py --limit 5      # smoke test
    .venv/bin/python migrate_v1_to_v2.py --workers 6
"""

import argparse
import concurrent.futures
import json
import re
import subprocess
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
SRC_DIR = ROOT / "Presets"
DST_DIR = ROOT / "Presets_v2"
LOG_FILE = ROOT / ".migration_v2.json"

PPQN = 96                       # ticks per quarter note
STEPS_PER_BAR_V1 = 16           # v1 is fixed 4/4 16ths
TICKS_PER_STEP_V1 = (PPQN * 4) // STEPS_PER_BAR_V1   # = 24

# Velocity letter codes used in v1 files.
VEL = {"g": 25, "s": 55, "m": 80, "h": 100, "a": 120}

# Track order in v1/v2 files.
TRACKS = ["kick", "snare", "hihat_c", "hihat_o",
          "ride", "crash", "tom_h", "tom_m", "tom_l", "rim"]

PROMPT_TEMPLATE = """\
You are renaming and re-tagging a drum pattern in WillyBeat (a drum machine).

Critically: this pattern must NOT reference any specific artist, song, \
album, label, producer, drummer, group, or person. Pure descriptions of \
the rhythmic feel and musical context only. Names like "Bohemian \
Rhapsody" or "Coltrane Swing" must become something like "Theatrical \
Rock Epic" or "Modal Jazz Swing".

Current name: {name}
Current type: {ptype}
Current tags: {tags}
Rhythmic profile (16th-note grid, 1 bar, 4/4):
{grid}

Velocity codes: g=ghost s=soft m=medium h=hard a=accent . =off

Return TWO lines and nothing else:

NAME: <a short descriptive name, 2-5 words, no proper nouns, Title Case>
TAGS: <comma-separated tags, 8-15 of them, no proper nouns>

Tag rules - allowed dimensions:
- Genre / subgenre (Rock, Hip-Hop, Boom Bap, Trap, House, Techno, \
Bossa Nova, Reggae, Dub, Funk, Soul, Blues, Country, Folk, Jazz, Bebop, \
Indie Pop, Synth-Pop, Bedroom Pop, etc.)
- Era / decade (60s, 70s, 80s, 90s, 2000s, 2010s, 2020s)
- Mood / feel (Aggressive, Mellow, Driving, Laid-back, Dark, Bright, \
Smooth, Bouncy, Heavy, Light, Sad, Uplifting, Dreamy, Sparse, Busy, \
Tight, Loose)
- Energy / tempo (Fast, Slow, Mid-tempo, Energetic, Chill)
- Scene / use-case (Club, Underground, Mainstream, Festival, Studio, \
Party, Workout, Lounge)
- Regional cues that aren't proper nouns (West Coast, East Coast, \
Southern, UK, Bristol, Detroit, Atlanta, Memphis, Chicago, New Orleans, \
LA, NYC, Latin, Brazilian, Caribbean, African, Japanese, Korean)
- Instrumentation feel (808, Sample-based, Acoustic, Synth, Electronic, \
Drum Machine, Live Drums)

Strict denies:
- Artist names, band names, group names, producer names, drummer names
- Song titles, album titles, EP names
- Record label names

Output ONLY the two lines, no preamble, no markdown.
"""


GENRE_RE = re.compile(r"^genres:\s*(.*)$", re.MULTILINE)
NAME_RE = re.compile(r"^name:\s*(.*)$", re.MULTILINE)
TYPE_RE = re.compile(r"^type:\s*(.*)$", re.MULTILINE)
TRACK_RE = re.compile(r"^([a-z_]+)\s+([.gsmha]+)\s*$", re.MULTILINE)


def parse_v1(text: str) -> dict:
    out = {
        "name": "",
        "genres": [],
        "type": "Regular",
        "grid": {t: "." * STEPS_PER_BAR_V1 for t in TRACKS},
    }
    if m := NAME_RE.search(text):
        out["name"] = m.group(1).strip()
    if m := TYPE_RE.search(text):
        out["type"] = m.group(1).strip()
    if m := GENRE_RE.search(text):
        out["genres"] = [t.strip() for t in m.group(1).split(",") if t.strip()]
    for m in TRACK_RE.finditer(text):
        track, row = m.group(1), m.group(2)
        if track in out["grid"] and len(row) == STEPS_PER_BAR_V1:
            out["grid"][track] = row
    return out


def grid_to_hits(grid: dict) -> dict[str, list[tuple[int, int]]]:
    out = {}
    for track, row in grid.items():
        hits = []
        for step, ch in enumerate(row):
            if vel := VEL.get(ch):
                hits.append((step * TICKS_PER_STEP_V1, vel))
        out[track] = hits
    return out


def render_grid_for_prompt(grid: dict) -> str:
    return "\n".join(f"  {t:<8} {grid[t]}" for t in TRACKS)


def density_from_hits(hits: dict, total_cells: int) -> float:
    active = sum(len(v) for v in hits.values())
    return active / total_cells if total_cells else 0.0


def call_claude(prompt: str, timeout: int = 60) -> str:
    r = subprocess.run(
        ["claude", "-p", "--output-format", "text"],
        input=prompt,
        text=True,
        capture_output=True,
        timeout=timeout,
    )
    if r.returncode != 0:
        raise RuntimeError(f"claude {r.returncode}: {r.stderr.strip()}")
    return r.stdout.strip()


NAME_OUT_RE = re.compile(r"^NAME:\s*(.+)$", re.MULTILINE)
TAGS_OUT_RE = re.compile(r"^TAGS:\s*(.+)$", re.MULTILINE)


def parse_llm_reply(raw: str) -> tuple[str, list[str]]:
    name = ""
    tags: list[str] = []
    if m := NAME_OUT_RE.search(raw):
        name = m.group(1).strip().strip("\"'")
    if m := TAGS_OUT_RE.search(raw):
        tags_raw = m.group(1).strip().strip("\"'")
        tags = [t.strip() for t in tags_raw.split(",") if t.strip()]
    return name, tags


def emit_v2(meta: dict, hits: dict, dst: Path) -> None:
    lines = [
        "# WillyBeat Preset v2",
        "version: 2",
        f"name:    {meta['name']}",
        f"genres:  {', '.join(meta['genres'])}",
        f"type:    {meta['type']}",
        "timesig: 4/4",
        "bars:    1",
        f"density: {meta['density']:.4f}",
        "",
        "# Hits per track as \"tick=velocity\" pairs. PPQN = 96.",
    ]
    for t in TRACKS:
        pairs = ", ".join(f"{tick}={vel}" for tick, vel in hits[t])
        if pairs:
            lines.append(f"{t:<8} {pairs}")
        else:
            lines.append(f"{t:<8}")
    dst.write_text("\n".join(lines) + "\n", encoding="utf-8")


def safe_filename(name: str) -> str:
    # Mirrors PatternLibrary's existing patternFile naming: keep letters,
    # digits, spaces, dashes; replace anything else with underscore.
    s = "".join(c if c.isalnum() or c in " -" else "_" for c in name)
    return s.strip() or "Untitled"


def process(src: Path, log: dict, lock_print, force: bool) -> dict:
    rec = {"src": src.name, "ok": False, "skipped": False, "error": ""}
    if not force and src.name in log:
        rec = log[src.name].copy()
        rec["skipped"] = True
        return rec

    try:
        v1 = parse_v1(src.read_text(encoding="utf-8"))
        hits = grid_to_hits(v1["grid"])

        prompt = PROMPT_TEMPLATE.format(
            name=v1["name"] or src.stem,
            ptype=v1["type"],
            tags=", ".join(v1["genres"]) or "(none)",
            grid=render_grid_for_prompt(v1["grid"]),
        )
        raw = call_claude(prompt)
        new_name, new_tags = parse_llm_reply(raw)
        if not new_name:
            raise RuntimeError(f"empty NAME from claude: {raw[:200]!r}")
        if not new_tags:
            raise RuntimeError(f"empty TAGS from claude: {raw[:200]!r}")

        meta = {
            "name": new_name,
            "genres": new_tags,
            "type": v1["type"],
            "density": density_from_hits(hits, len(TRACKS) * STEPS_PER_BAR_V1),
        }
        out_path = DST_DIR / f"{safe_filename(new_name)}.beat"
        # Collision: append a numeric suffix.
        i = 2
        while out_path.exists():
            out_path = DST_DIR / f"{safe_filename(new_name)} {i}.beat"
            i += 1

        emit_v2(meta, hits, out_path)
        rec["ok"] = True
        rec["dst"] = out_path.name
        rec["new_name"] = new_name
        rec["new_tags"] = new_tags
        lock_print(f"  ✓ {src.name} → {out_path.name}")
    except Exception as e:
        rec["error"] = str(e)
        lock_print(f"  ✗ {src.name}: {e}")
    return rec


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--workers", type=int, default=6)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--force", action="store_true")
    args = ap.parse_args()

    if not SRC_DIR.is_dir():
        raise SystemExit(f"missing {SRC_DIR}")
    DST_DIR.mkdir(exist_ok=True)

    log: dict = {}
    if LOG_FILE.exists():
        log = json.loads(LOG_FILE.read_text())

    files = sorted(SRC_DIR.glob("*.beat"))
    todo = files if args.force else [f for f in files if f.name not in log]
    if args.limit:
        todo = todo[: args.limit]

    print(f"Migrating {len(todo)} / {len(files)} files (workers={args.workers})")

    plock = threading.Lock()
    def lock_print(msg: str) -> None:
        with plock:
            print(msg, flush=True)

    t0 = time.time()
    done = 0
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as ex:
        futures = {ex.submit(process, f, log, lock_print, args.force): f for f in todo}
        for fut in concurrent.futures.as_completed(futures):
            rec = fut.result()
            log[rec["src"]] = rec
            done += 1
            if done % 20 == 0:
                LOG_FILE.write_text(json.dumps(log, indent=2))
                lock_print(f"  ── {done}/{len(todo)}  ({time.time() - t0:.0f}s)")

    LOG_FILE.write_text(json.dumps(log, indent=2))
    ok = sum(1 for v in log.values() if v.get("ok"))
    fail = sum(1 for v in log.values() if v.get("error"))
    print(f"Done in {time.time() - t0:.0f}s. OK={ok}, failed={fail}, total processed={len(todo)}")


if __name__ == "__main__":
    main()
