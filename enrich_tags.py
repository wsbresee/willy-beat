#!/usr/bin/env python3
"""enrich_tags.py — Expand the genres: line in every .beat preset with
semantically related tags so the in-plugin vector search has more hooks
to match against.

For each existing tag we add:
  - PARENTS: broader/family tags (Trap → Hip-Hop, Rap)
  - ERAS:    decades / period markers (Disco → 70s)
  - MOODS:   energy / feel descriptors (Metal → Aggressive, Heavy)
  - SCENES:  use-case / context (Techno → Club, Dance)

Idempotent: re-running won't duplicate existing tags. Run from the repo
root; targets ~/Library/Application Support/WillyBeat/Presets/ by default
but accepts --dir to point elsewhere (useful when running CI on the
bundled Presets/ copy).

Usage:
    .venv/bin/python enrich_tags.py
    .venv/bin/python enrich_tags.py --dir Presets        # in-repo copy
    .venv/bin/python enrich_tags.py --dry-run            # preview only
"""

import argparse
import re
from pathlib import Path

# ──────────────────────────────────────────────────────────────────────────
# Tag-expansion table. Each key is a tag that may appear in an existing
# .beat file's `genres:` line; values are extra tags to merge in. Keys are
# matched case-sensitively against the existing entries (which are already
# canonicalised in the curated bulk_generate list).

EXPANSIONS: dict[str, list[str]] = {
    # ── Hip-Hop family ────────────────────────────────────────────────────
    "Hip-Hop":     ["Rap", "Urban", "Beats"],
    "Trap":        ["Hip-Hop", "Rap", "808", "Modern", "Hard", "2010s"],
    "Boom Bap":    ["Hip-Hop", "Rap", "90s", "Underground", "Sample-based", "Old School"],
    "Drill":       ["Hip-Hop", "Rap", "Trap", "Dark", "Aggressive", "Modern", "UK"],
    "Cloud Rap":   ["Hip-Hop", "Rap", "Trap", "Atmospheric", "Mellow", "Dreamy"],
    "Phonk":       ["Hip-Hop", "Memphis", "Dark", "Underground", "Lofi"],
    "Drift Phonk": ["Phonk", "Hip-Hop", "Dark", "Aggressive", "Driving"],
    "Memphis":     ["Hip-Hop", "Phonk", "Southern", "Dark"],
    "Memphis Rap": ["Memphis", "Hip-Hop", "Southern", "Dark"],
    "Memphis Crunk": ["Memphis", "Hip-Hop", "Crunk", "Southern"],
    "Crunk":       ["Hip-Hop", "Southern", "Party", "Aggressive", "2000s"],
    "G-Funk":      ["Hip-Hop", "West Coast", "Funk", "90s", "Laid-back"],
    "West Coast":  ["Hip-Hop", "California"],
    "East Coast":  ["Hip-Hop", "New York", "Boom Bap"],
    "Conscious Hip-Hop": ["Hip-Hop", "Rap", "Lyrical"],
    "Trap Soul":   ["Trap", "R&B", "Hip-Hop", "Smooth", "Modern"],
    "Lofi Hip-Hop": ["Lofi", "Hip-Hop", "Chill", "Mellow", "Study"],
    "Emo Rap":     ["Hip-Hop", "Trap", "Emo", "Sad", "Modern"],
    "Trap Metal":  ["Trap", "Metal", "Aggressive", "Hard"],
    "Mumble Rap":  ["Hip-Hop", "Trap", "Modern"],
    "Old School":  ["Classic", "Hip-Hop", "70s", "80s"],
    "Rap":         ["Hip-Hop", "Vocal", "Urban"],
    "Hyperpop":    ["Pop", "Experimental", "Modern", "Electronic", "Glitchy"],

    # ── Rock family ───────────────────────────────────────────────────────
    "Rock":         ["Guitar", "Band"],
    "Classic Rock": ["Rock", "70s", "Retro", "Guitar"],
    "Hard Rock":    ["Rock", "Heavy", "Aggressive", "70s", "80s"],
    "Heavy Metal":  ["Metal", "Rock", "Heavy", "Aggressive", "Dark"],
    "Metal":        ["Rock", "Heavy", "Aggressive", "Dark", "Guitar"],
    "Death Metal":  ["Metal", "Extreme", "Dark", "Aggressive", "Brutal"],
    "Thrash":       ["Metal", "Aggressive", "Fast", "80s", "Heavy"],
    "Doom":         ["Metal", "Slow", "Dark", "Heavy"],
    "Stoner":       ["Metal", "Rock", "Slow", "Psychedelic"],
    "Prog Metal":   ["Metal", "Progressive", "Technical", "Complex"],
    "Groove Metal": ["Metal", "Groove", "90s", "Heavy"],
    "Prog Rock":    ["Rock", "Progressive", "Complex", "70s"],
    "Soft Rock":    ["Rock", "Mellow", "70s", "Smooth"],
    "Arena Rock":   ["Rock", "Anthemic", "80s", "Stadium"],
    "Heartland Rock": ["Rock", "Americana", "80s"],
    "Folk Rock":    ["Rock", "Folk", "Acoustic", "70s"],
    "Blues Rock":   ["Rock", "Blues", "Guitar"],
    "Garage Rock":  ["Rock", "Raw", "Indie", "Lofi"],
    "Indie Rock":   ["Rock", "Indie", "Alternative", "Underground"],
    "Punk Rock":    ["Punk", "Rock", "Fast", "Raw", "Aggressive"],
    "Pop Rock":     ["Rock", "Pop", "Catchy", "Mainstream"],
    "Surf Rock":    ["Rock", "60s", "Beach", "Retro"],
    "Grunge":       ["Rock", "Alternative", "90s", "Raw", "Heavy"],
    "Alternative":  ["Rock", "Indie", "90s", "Underground"],
    "Post-Rock":    ["Rock", "Atmospheric", "Cinematic", "Instrumental"],
    "Post-Punk":    ["Punk", "Rock", "New Wave", "Dark", "80s"],
    "Post-Hardcore": ["Hardcore", "Punk", "Heavy", "Aggressive"],
    "Math Rock":    ["Rock", "Progressive", "Complex", "Odd Time"],
    "Punk":         ["Rock", "Fast", "Raw", "Aggressive", "DIY"],
    # "Hardcore" is polysemous (punk and electronic gabber/hardstyle); union
    # both senses' tags so search hits patterns tagged with either meaning.
    "Hardcore":     ["Punk", "Aggressive", "Fast", "Heavy", "Electronic", "Dance"],
    "Skate Punk":   ["Punk", "Fast", "90s"],
    "Pop Punk":     ["Punk", "Pop", "Fast", "Catchy", "2000s"],
    "Emo":          ["Rock", "Punk", "Sad", "Emotional"],
    "Shoegaze":     ["Rock", "Atmospheric", "Dreamy", "Wall-of-sound", "90s"],
    "NWOBHM":       ["Metal", "Heavy Metal", "Rock", "80s", "Britain"],

    # ── Pop family ────────────────────────────────────────────────────────
    "Pop":          ["Mainstream", "Catchy", "Vocal"],
    "Dance Pop":    ["Pop", "Dance", "Mainstream", "Energetic", "Club"],
    "Electropop":   ["Pop", "Electronic", "Synth", "Modern"],
    "Synth-Pop":    ["Pop", "Electronic", "Synth", "80s", "Retro"],
    "Indie Pop":    ["Pop", "Indie", "Alternative"],
    "Bedroom Pop":  ["Pop", "Indie", "Lofi", "Intimate", "Modern"],
    "Alt Pop":      ["Pop", "Indie", "Alternative", "Modern"],
    "K-Pop":        ["Pop", "Korean", "Modern", "Dance"],
    "City Pop":     ["Pop", "80s", "Japanese", "Smooth", "Retro"],
    "Boy Band":     ["Pop", "Vocal", "Mainstream"],
    "Tropical House": ["Pop", "House", "Electronic", "Summer", "Chill"],
    "Dream Pop":    ["Pop", "Atmospheric", "Dreamy", "Mellow"],

    # ── Electronic family ─────────────────────────────────────────────────
    "Electronic":   ["Synth", "Digital", "Beats"],
    "House":        ["Electronic", "Dance", "Four on the Floor", "Club"],
    "Tech House":   ["House", "Techno", "Electronic", "Dance", "Club", "Underground"],
    "Acid House":   ["House", "Electronic", "Acid", "303", "Dance", "Club"],
    "Disco House":  ["House", "Disco", "Electronic", "Dance", "Funky"],
    "Deep House":   ["House", "Electronic", "Smooth", "Club", "Mellow"],
    "Progressive House": ["House", "Electronic", "Dance", "Anthemic"],
    "Techno":       ["Electronic", "Dance", "Club", "Underground", "Driving"],
    "Detroit Techno": ["Techno", "Electronic", "Dance", "Underground"],
    "Hardstyle":    ["Electronic", "Dance", "Aggressive", "Hard", "Fast"],
    "Gabber":       ["Electronic", "Hardcore", "Dance", "Fast", "Aggressive"],
    "EBM":          ["Electronic", "Industrial", "Dark", "Driving"],
    "Industrial":   ["Electronic", "Dark", "Mechanical", "Aggressive"],
    "Trance":       ["Electronic", "Dance", "Anthemic", "Uplifting", "Euphoric"],
    "Hands Up":     ["Trance", "Electronic", "Dance", "Energetic"],
    "Big Beat":     ["Electronic", "Breakbeat", "Dance", "90s"],
    "Breakbeat":    ["Electronic", "Breaks", "Dance"],
    "EDM":          ["Electronic", "Dance", "Mainstream", "Festival"],
    "Dubstep":      ["Electronic", "Bass", "Heavy", "Wobble", "2010s"],
    "Brostep":      ["Dubstep", "Electronic", "Bass", "Aggressive"],
    "DnB":          ["Electronic", "Drum and Bass", "Fast", "Breakbeat"],
    "Drum and Bass": ["DnB", "Electronic", "Fast", "Breakbeat"],
    "Liquid DnB":   ["DnB", "Electronic", "Smooth", "Mellow", "Atmospheric"],
    "Jungle":       ["DnB", "Electronic", "Fast", "Breakbeat", "90s"],
    "Footwork":     ["Electronic", "Chicago", "Fast", "Underground"],
    "Future Bass":  ["Electronic", "EDM", "Bass", "Modern"],
    "Glitch":       ["Electronic", "Experimental", "IDM"],
    "IDM":          ["Electronic", "Experimental", "Intelligent"],
    "Ambient":      ["Electronic", "Atmospheric", "Mellow", "Chill"],
    "Synthwave":    ["Electronic", "Retro", "80s", "Synth", "Cinematic"],
    "Vaporwave":    ["Electronic", "Retro", "80s", "Chill", "Dreamy"],
    "Chillwave":    ["Electronic", "Chill", "Mellow", "Dreamy", "Retro"],
    "UK Garage":    ["Electronic", "Garage", "UK", "Dance", "2-Step"],
    "2-Step":       ["UK Garage", "Electronic", "Dance"],
    "Speed Garage": ["UK Garage", "Electronic", "Dance", "Fast"],
    "Tech Step":    ["DnB", "Electronic", "Dark"],
    "Acid Techno":  ["Techno", "Electronic", "Acid", "303", "Dance"],
    "Minimal Techno": ["Techno", "Electronic", "Minimal", "Club"],
    "Garage":       ["Electronic", "Dance", "UK"],

    # ── Dance / disco / funk / soul ───────────────────────────────────────
    "Disco":        ["Dance", "70s", "Funk", "Retro", "Groovy"],
    "Funk":         ["Groove", "Soul", "Bass", "Rhythm"],
    "P-Funk":       ["Funk", "70s", "Psychedelic", "Groove"],
    "Soul":         ["R&B", "Vocal", "Smooth", "Groove"],
    "Motown":       ["Soul", "60s", "Pop", "Classic"],
    "Memphis Soul": ["Soul", "Southern", "Classic"],
    "Neo-Soul":     ["Soul", "R&B", "Smooth", "Modern", "Jazz"],
    "Soul Blues":   ["Soul", "Blues", "Vocal"],
    "Gospel":       ["Soul", "R&B", "Spiritual", "Vocal"],
    "Blue-Eyed Soul": ["Soul", "Pop", "Vocal"],
    "R&B":          ["Soul", "Smooth", "Vocal", "Urban"],
    "New Wave":     ["Rock", "Pop", "80s", "Synth"],
    "Boogie":       ["Funk", "Disco", "80s", "Groove"],

    # ── Latin ─────────────────────────────────────────────────────────────
    "Latin":        ["World", "Rhythm"],
    "Salsa":        ["Latin", "Cuban", "Dance", "Rhythm"],
    "Bossa Nova":   ["Latin", "Brazilian", "Smooth", "Mellow", "Jazz"],
    "Samba":        ["Latin", "Brazilian", "Carnival", "Rhythm", "Energetic"],
    "Brazilian":    ["Latin", "World", "Rhythm"],
    "Cumbia":       ["Latin", "Colombian", "Dance"],
    "Merengue":     ["Latin", "Dominican", "Dance", "Fast"],
    "Mambo":        ["Latin", "Cuban", "Dance", "Big Band"],
    "Latin Jazz":   ["Latin", "Jazz", "Rhythm"],
    "Reggaeton":    ["Latin", "Dembow", "Dance", "Urban", "Modern"],
    "Reggae":       ["Caribbean", "Jamaican", "Laid-back", "World"],
    "Roots Reggae": ["Reggae", "Jamaican", "Classic"],
    "Dub":          ["Reggae", "Atmospheric", "Bass", "Echo"],
    "Dancehall":    ["Reggae", "Caribbean", "Dance"],
    "Calypso":      ["Caribbean", "World", "Tropical"],
    "Latin Pop":    ["Latin", "Pop", "Mainstream"],
    "Bachata":      ["Latin", "Dominican", "Romantic"],

    # ── Jazz ──────────────────────────────────────────────────────────────
    "Jazz":         ["Improvised", "Swing", "Sophisticated"],
    "Bebop":        ["Jazz", "Fast", "Improvised", "Classic", "40s", "50s"],
    "Swing":        ["Jazz", "30s", "40s", "Big Band", "Classic"],
    "Fusion":       ["Jazz", "Rock", "Electric", "Complex", "70s"],
    "Big Band":     ["Jazz", "Swing", "Classic"],
    "Smooth Jazz":  ["Jazz", "Smooth", "Mellow", "Easy"],
    "Hard Bop":     ["Jazz", "Bebop", "50s", "60s"],

    # ── Blues / Country / Folk ────────────────────────────────────────────
    "Blues":        ["Guitar", "Roots", "Soul"],
    "Country":      ["Roots", "Americana", "Acoustic"],
    "Train Beat":   ["Country", "Driving", "Classic"],
    "Bluegrass":    ["Folk", "Country", "Acoustic", "Roots"],
    "Folk":         ["Acoustic", "Roots", "Traditional"],
    "Klezmer":      ["Folk", "Jewish", "Eastern European", "Traditional"],
    "Americana":    ["Country", "Folk", "Roots"],

    # ── World / regional ──────────────────────────────────────────────────
    "Afrobeat":     ["African", "World", "Funky", "Rhythm", "70s"],
    "Afrobeats":    ["African", "World", "Modern", "Dance", "Pop"],
    "Amapiano":     ["African", "House", "South African", "Modern"],
    "New Orleans":  ["Funk", "Soul", "Southern", "Second Line"],
    "Southern":     ["American", "Roots"],

    # ── Genre tags that are mostly era / scene markers ────────────────────
    "Lofi":         ["Chill", "Mellow", "Underground", "Bedroom", "Study"],
    "Lo-fi":        ["Lofi", "Chill", "Mellow", "Underground"],
    "Trip-Hop":     ["Hip-Hop", "Electronic", "Atmospheric", "Mellow", "90s", "Bristol"],
    "Indie":        ["Alternative", "Underground", "Independent"],
    "Experimental": ["Avant-garde", "Underground", "Modern"],
    "Classic":      ["Iconic", "Influential", "Canon"],

    # ── Decade-only tags get reinforced if present ────────────────────────
    "60s":  ["Retro", "Classic", "Vintage"],
    "70s":  ["Retro", "Vintage"],
    "80s":  ["Retro", "Vintage"],
    "90s":  ["Retro"],
    "2000s": ["Y2K"],
    "2010s": ["Modern"],
    "2020s": ["Modern", "Current"],
}


# A few helpers that look at the existing tag set as a whole rather than
# one tag at a time.
def add_combo_tags(existing: list[str]) -> list[str]:
    """Add tags based on the COMBINATION of existing genres."""
    extras: list[str] = []
    s = set(existing)

    if "Trap" in s and "R&B" in s:
        extras.append("Trap Soul")
    if "Punk" in s and ("Hardcore" in s or "D-Beat" in s):
        extras.append("Aggressive")
    if "Pop" in s and any(x in s for x in ["Disco", "Funk", "Dance Pop"]):
        extras.append("Danceable")
    if any(x in s for x in ["Bedroom Pop", "Lofi", "Lo-fi", "Lofi Hip-Hop"]):
        extras.append("Intimate")
    if "Hip-Hop" in s and "Modern" in s:
        extras.append("Contemporary")
    if "Metal" in s or "Hardcore" in s or "Thrash" in s:
        extras.append("Loud")
    if "Jazz" in s or "Bebop" in s or "Bossa Nova" in s:
        extras.append("Sophisticated")

    return extras


GENRE_RE = re.compile(r"^genres:\s*(.*)$", re.MULTILINE)


def expand(genres: list[str]) -> list[str]:
    """Return a deduped, order-preserving list with expansion applied."""
    seen: dict[str, None] = {}
    for g in genres:
        if g and g not in seen:
            seen[g] = None
    # Expand each original tag.
    for g in list(seen):
        for extra in EXPANSIONS.get(g, []):
            if extra not in seen:
                seen[extra] = None
    # Combo-based extras (consider the union after first-pass expansion).
    for extra in add_combo_tags(list(seen)):
        if extra not in seen:
            seen[extra] = None
    return list(seen)


def process_file(path: Path, dry_run: bool) -> tuple[int, int]:
    text = path.read_text(encoding="utf-8")
    m = GENRE_RE.search(text)
    if not m:
        return 0, 0
    before = [t.strip() for t in m.group(1).split(",") if t.strip()]
    after = expand(before)
    if after == before:
        return len(before), len(after)
    new_line = "genres: " + ", ".join(after)
    new_text = GENRE_RE.sub(new_line, text, count=1)
    if not dry_run:
        path.write_text(new_text, encoding="utf-8")
    return len(before), len(after)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--dir",
        type=Path,
        default=Path.home() / "Library/Application Support/WillyBeat/Presets",
        help="Folder of .beat files to enrich",
    )
    ap.add_argument("--dry-run", action="store_true", help="don't write files")
    args = ap.parse_args()

    if not args.dir.is_dir():
        raise SystemExit(f"Not a directory: {args.dir}")

    files = sorted(args.dir.glob("*.beat"))
    total_before = total_after = 0
    changed = 0
    for f in files:
        b, a = process_file(f, args.dry_run)
        total_before += b
        total_after += a
        if a > b:
            changed += 1

    print(f"Processed {len(files)} files, expanded {changed}.")
    if files:
        print(f"  avg tags/pattern: {total_before / len(files):.2f} → {total_after / len(files):.2f}")
    if args.dry_run:
        print("(dry run — nothing written)")


if __name__ == "__main__":
    main()
