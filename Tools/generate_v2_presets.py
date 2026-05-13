#!/usr/bin/env python3
"""Generate the v2 starter pattern bank.

Writes ~100 .beat files (v2 format) into the directory passed as argv[1].
Patterns cover the foundational rhythms in modern popular music plus a
handful of odd-meter examples to showcase v2's any-time-sig support.

Run: python3 Tools/generate_v2_presets.py <output-dir>
"""

from __future__ import annotations

import os
import sys
from dataclasses import dataclass, field
from typing import Dict, List, Tuple

PPQN = 96

# Track index → file key, must match kTrackFileKeys[] in DrumPattern.h
TRACKS = [
    "kick", "snare", "hihat_c", "hihat_o",
    "ride", "crash", "tom_h", "tom_m",
    "tom_l", "rim",
]
NUM_TRACKS = len(TRACKS)

# Velocity legend used by the ASCII row parser. Mirrors the v1 builtins.
VEL = {
    "g": 25,
    "s": 55,
    "m": 80,
    "h": 100,
    "a": 120,
}

# Grid sub keys, must match kGridSubFileKeys[]
# 8th=48t, 8tr=32t, 16th=24t, 16tr=16t, 32nd=12t
GRID_TICKS = {"8th": 48, "8tr": 32, "16th": 24, "16tr": 16, "32nd": 12}


@dataclass
class Pattern:
    name: str
    genres: List[str]
    timesig: Tuple[int, int] = (4, 4)
    bars: int = 1
    grid: str = "16th"
    type_: str = "Regular"   # "Regular" | "Variance" | "Sm. Fill" | "Big Fill"
    hits: Dict[str, List[Tuple[int, int]]] = field(default_factory=dict)

    @property
    def total_ticks(self) -> int:
        num, den = self.timesig
        return self.bars * num * 4 * PPQN // den


def hits_from_rows(rows: Dict[str, str], grid: str = "16th") -> Dict[str, List[Tuple[int, int]]]:
    """Convert an ASCII-row dict into per-track tick=velocity hit lists.

    Row chars: '.' = off, others lookup in VEL.
    Each character occupies one grid cell whose width is GRID_TICKS[grid].
    """
    cell_ticks = GRID_TICKS[grid]
    out: Dict[str, List[Tuple[int, int]]] = {t: [] for t in TRACKS}
    for track, row in rows.items():
        if track not in TRACKS:
            raise ValueError(f"unknown track {track!r}")
        for i, ch in enumerate(row):
            if ch == "." or ch == " ":
                continue
            if ch not in VEL:
                raise ValueError(f"bad velocity char {ch!r} in {track} row")
            out[track].append((i * cell_ticks, VEL[ch]))
    return out


def density(p: Pattern) -> float:
    """Mirror DrumPattern::recomputeDensity()."""
    active = sum(len(p.hits.get(t, [])) for t in TRACKS)
    sixteenths = max(1, p.total_ticks // 24)
    total = NUM_TRACKS * sixteenths
    return min(1.0, active / total)


def serialize(p: Pattern) -> str:
    lines: List[str] = []
    lines.append("# WillyBeat Preset v2")
    lines.append("version: 2")
    lines.append(f"name:    {p.name}")
    lines.append(f"genres:  {', '.join(p.genres)}")
    lines.append(f"type:    {p.type_}")
    lines.append(f"timesig: {p.timesig[0]}/{p.timesig[1]}")
    lines.append(f"bars:    {p.bars}")
    lines.append(f"grid:    {p.grid}")
    lines.append(f"density: {density(p):.4f}")
    lines.append("")
    lines.append('# Hits per track as "tick=velocity" pairs. PPQN = 96.')
    for t in TRACKS:
        key = t.ljust(8)
        hs = sorted(p.hits.get(t, []), key=lambda x: x[0])
        if hs:
            body = ", ".join(f"{tick}={vel}" for tick, vel in hs)
            lines.append(f"{key} {body}")
        else:
            lines.append(f"{key}")
    return "\n".join(lines) + "\n"


# ─── Pattern definitions ───────────────────────────────────────────────────

PATTERNS: List[Pattern] = []


def add(name, genres, rows=None, grid="16th", timesig=(4, 4), bars=1, type_="Regular", hits=None):
    p = Pattern(name=name, genres=genres, timesig=timesig, bars=bars, grid=grid, type_=type_)
    if rows is not None:
        p.hits = hits_from_rows(rows, grid=grid)
    elif hits is not None:
        p.hits = {t: list(hs) for t, hs in hits.items() if hs}
        # fill missing tracks with empty list so serialise sees them
        for t in TRACKS:
            p.hits.setdefault(t, [])
    PATTERNS.append(p)


# ════════════════════════════════════════════════════════════════════════════
# ROCK / POP — 4/4, 16th grid (16 cells per bar)
# ════════════════════════════════════════════════════════════════════════════

add("Rock Basic",
    ["Rock", "Pop", "Backbeat", "Classic", "Driving", "Live", "Acoustic", "Mainstream"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Rock Drive",
    ["Rock", "Driving", "Backbeat", "Anthem", "Stadium", "Energetic", "Power"],
    rows={
        "kick":    "h.....m.h.......",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Rock Half-Time",
    ["Rock", "Half-Time", "Heavy", "Groove", "Slow", "Stoner", "Doom-Adjacent"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "........a.......",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Rock Eighth Drive",
    ["Rock", "Punk", "Driving", "Fast", "Energetic", "Live"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Pop Backbeat",
    ["Pop", "Backbeat", "Mainstream", "Clean", "Radio", "Smooth"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "....h.......h...",
        "hihat_c": "m...m...m...m...",
    })

add("Stadium Anthem",
    ["Rock", "Anthem", "Stadium", "Big", "Driving", "Power", "Live"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "crash":   "h...............",
    })

add("Train Beat",
    ["Country", "Rock", "Train", "Driving", "Roots", "Americana"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "g.g.m.g.g.g.m.g.",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Punk Driver",
    ["Punk", "Rock", "Fast", "Aggressive", "Driving", "Raw", "Energy"],
    rows={
        "kick":    "h.h.h.h.h.h.h.h.",
        "snare":   "....a.......a...",
        "hihat_c": "h...h...h...h...",
    })

add("Power Ballad",
    ["Rock", "Ballad", "Slow", "Power", "Emotional", "80s"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "....a.......a...",
        "hihat_c": "m...m...m...m...",
        "crash":   "h...............",
    })

# ════════════════════════════════════════════════════════════════════════════
# FUNK / SOUL
# ════════════════════════════════════════════════════════════════════════════

add("Funk Basic",
    ["Funk", "Groove", "Soul", "Syncopated", "Pocket", "Vintage", "Live"],
    rows={
        "kick":    "h.....h..h......",
        "snare":   "....a.......a...",
        "hihat_c": "m.mgm.mgm.mgm.mg",
    })

add("Funk Ghost Notes",
    ["Funk", "Soul", "Ghost Notes", "Pocket", "Groove", "Dynamic", "Live"],
    rows={
        "kick":    "h....h..h.......",
        "snare":   "g..ga.gg.gga.gg.",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Funk Sixteenth Hat",
    ["Funk", "16th", "Busy", "Energetic", "Disco-Adjacent", "Pocket"],
    rows={
        "kick":    "h....h..h....h..",
        "snare":   "....a.......a...",
        "hihat_c": "mmmmmmmmmmmmmmmm",
    })

add("NOLA Second Line",
    ["Funk", "New Orleans", "Parade", "Marching", "Syncopated", "Brass-Band"],
    rows={
        "kick":    "h..h.h..h..h.h..",
        "snare":   "....g.m.....s.m.",
        "hihat_c": "m...m...m...m...",
    })

add("Cold Funk",
    ["Funk", "Cold", "Hard", "Aggressive", "70s", "James-Brown-Adjacent"],
    rows={
        "kick":    "h..h....h.......",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "hihat_o": "................",
    })

add("Funk Pocket Slow",
    ["Funk", "Slow", "Pocket", "Smooth", "R&B", "Laid Back"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "....m.......m...",
        "hihat_c": "m.mgm.mgm.mgm.mg",
    })

# ════════════════════════════════════════════════════════════════════════════
# HIP HOP / R&B
# ════════════════════════════════════════════════════════════════════════════

add("Boom Bap Straight",
    ["Hip Hop", "Boom Bap", "Straight", "Classic", "90s", "East Coast", "Sample-Based"],
    rows={
        "kick":    "h.....h.h.......",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Boom Bap Swung",
    ["Hip Hop", "Boom Bap", "Swung", "Classic", "90s", "Laid Back", "Sample-Based"],
    grid="8tr",
    rows={
        "kick":    "h.....h...h.",
        "snare":   "...a......a.",
        "hihat_c": "m..m..m..m..",
    })

add("Lofi Hip Hop",
    ["Hip Hop", "Lo-Fi", "Chill", "Mellow", "Bedroom", "Study", "Tape"],
    grid="8tr",
    rows={
        "kick":    "h.....h.....",
        "snare":   "...m......m.",
        "hihat_c": "m.gm.gm.gm.g",
    })

add("West Coast G-Funk",
    ["Hip Hop", "West Coast", "G-Funk", "90s", "Laid Back", "Smooth", "Bounce"],
    rows={
        "kick":    "h....h..h.......",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Trap Basic",
    ["Trap", "Hip Hop", "Half-Time", "808", "Atlanta", "Modern", "Club"],
    rows={
        "kick":    "h.......h.h.....",
        "snare":   "........a.......",
        "hihat_c": "mmmmmmmmmmmmmmmm",
    })

add("Trap Double-Time Hats",
    ["Trap", "Hip Hop", "Half-Time", "32nd Hats", "Modern", "Energetic"],
    grid="32nd",
    rows={
        "kick":    "h...............h...h...........",
        "snare":   "................a...............",
        "hihat_c": "m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.",
    })

add("Drill",
    ["Drill", "Trap", "UK", "Hip Hop", "Modern", "Sliding-Bass", "Dark"],
    rows={
        "kick":    "h.....h..h.h....",
        "snare":   "........a.......",
        "hihat_c": "m.m.m.mmm.m.m.mm",
    })

add("R&B Slow Jam",
    ["R&B", "Slow Jam", "Smooth", "Sensual", "90s", "Quiet Storm", "Bedroom"],
    rows={
        "kick":    "h.......h.h.....",
        "snare":   "....a.......a...",
        "hihat_c": "m.mgm.mgm.mgm.mg",
    })

add("Neo-Soul",
    ["Neo-Soul", "R&B", "Laid Back", "Pocket", "Live", "Smooth"],
    grid="8tr",
    rows={
        "kick":    "h.g..h..h...",
        "snare":   "...a......a.",
        "hihat_c": "m.gm.gm.gm.g",
    })

add("Quiet Storm",
    ["R&B", "Slow", "Quiet Storm", "Smooth", "80s", "Mellow"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "....m.......m...",
        "hihat_c": "m...m...m...m...",
    })

# ════════════════════════════════════════════════════════════════════════════
# HOUSE / TECHNO / DANCE
# ════════════════════════════════════════════════════════════════════════════

add("Four on the Floor",
    ["House", "Dance", "Four on the Floor", "Club", "Driving", "Foundation"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....m.......m...",
        "hihat_c": "..m...m...m...m.",
    })

add("Deep House",
    ["House", "Deep House", "Club", "Smooth", "Underground", "Late Night"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....s.......s...",
        "hihat_c": "..m...m...m...m.",
        "hihat_o": "..h...h...h...h.",
    })

add("Tech House",
    ["House", "Tech House", "Club", "Underground", "Driving", "Minimal"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....m.......m...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "hihat_o": "..h...h...h...h.",
    })

add("Minimal Techno",
    ["Techno", "Minimal", "Underground", "Hypnotic", "Club", "Berlin"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "................",
        "hihat_c": "..m...m...m...m.",
        "rim":     "..s.....g.......",
    })

add("Acid Techno",
    ["Techno", "Acid", "303", "Underground", "Driving", "90s", "Rave"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....m.......m...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "hihat_o": "..h...h...h...h.",
    })

add("Trance Anthem",
    ["Trance", "Anthem", "Uplifting", "Club", "Festival", "Euphoric", "Big Room"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....a.......a...",
        "hihat_c": "..m...m...m...m.",
        "hihat_o": "..h...h...h...h.",
    })

add("Hard Techno",
    ["Techno", "Hard", "Driving", "Industrial", "Berghain", "Aggressive"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....m.......m...",
        "hihat_c": "..h...h...h...h.",
    })

add("UK Garage 2-Step",
    ["UK Garage", "2-Step", "Skippy", "Syncopated", "Bass", "Underground", "London"],
    rows={
        "kick":    "h.....h....h.h..",
        "snare":   "....a.....a.....",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "hihat_o": "..h.....h.....h.",
    })

add("Speed Garage",
    ["Garage", "Speed Garage", "UK", "Club", "Driving", "Bass"],
    rows={
        "kick":    "h...h.....h.h...",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "hihat_o": "..h...h...h...h.",
    })

add("Disco Classic",
    ["Disco", "70s", "Four on the Floor", "Dance", "Groove", "Vintage", "Club"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "hihat_o": "..h...h...h...h.",
    })

add("NuDisco",
    ["Disco", "Nu-Disco", "Modern", "Funky", "Dance", "Bouncy"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....m.......m...",
        "hihat_c": "..mgm.mgm.mgm.mg",
        "hihat_o": "..h.....h.......",
    })

# ════════════════════════════════════════════════════════════════════════════
# DRUM AND BASS / BREAKBEAT
# ════════════════════════════════════════════════════════════════════════════

add("DnB Two-Step",
    ["Drum and Bass", "DnB", "Two-Step", "Fast", "Liquid", "UK"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "....a.......a...",
        "hihat_c": "..m...m...m...m.",
        "ride":    "m...m...m...m...",
    })

add("DnB Jungle Chop",
    ["Drum and Bass", "DnB", "Break", "Jungle", "Chopped", "Classic"],
    rows={
        "kick":    "h.....h.h.......",
        "snare":   "....a..a....a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "ride":    "............m.m.",
    })

add("Jungle",
    ["Jungle", "Drum and Bass", "Break", "Fast", "Chopped", "Ragga-Adjacent", "90s"],
    rows={
        "kick":    "h.....h.h.......",
        "snare":   "....a.....a.a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Liquid DnB",
    ["Drum and Bass", "DnB", "Liquid", "Smooth", "Musical", "Soulful"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "ride":    "..m...m...m...m.",
    })

add("Big Beat",
    ["Big Beat", "Breakbeat", "90s", "Electronic", "Rock-Adjacent", "Energetic"],
    rows={
        "kick":    "h..h....h.......",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Classic Breakbeat",
    ["Breakbeat", "Classic", "Sample", "Funk-Break", "Hip Hop", "90s"],
    rows={
        "kick":    "h.....h.h.......",
        "snare":   "....a...g...a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

# ════════════════════════════════════════════════════════════════════════════
# DUBSTEP / BASS MUSIC
# ════════════════════════════════════════════════════════════════════════════

add("Dubstep Half-Time",
    ["Dubstep", "Bass", "Half-Time", "Wobble", "Heavy", "UK", "Club"],
    rows={
        "kick":    "h.......h.h.....",
        "snare":   "........a.......",
        "hihat_c": "..m...m...m...m.",
    })

add("Future Bass",
    ["Future Bass", "Bass", "Modern", "EDM", "Festival", "Trap-Adjacent", "Melodic"],
    rows={
        "kick":    "h....h..h.h.....",
        "snare":   "........a.......",
        "hihat_c": "mmmmmmmmmmmmmmmm",
    })

add("Footwork",
    ["Footwork", "Juke", "Chicago", "Fast", "Triplet", "Modern", "Underground"],
    grid="16tr",
    bars=1,
    rows={
        "kick":    "h....h....h..h..........",
        "snare":   "............m...........",
        "hihat_c": "m..m..m..m..m..m..m..m..",
    })

# ════════════════════════════════════════════════════════════════════════════
# LATIN / AFRO-CARIBBEAN
# ════════════════════════════════════════════════════════════════════════════

add("Bossa Nova",
    ["Bossa Nova", "Brazilian", "Latin", "Jazz", "Smooth", "Acoustic", "Soft"],
    rows={
        "kick":    "h...h..hh..h..h.",
        "snare":   "................",
        "rim":     "..m...m..m..m...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Samba",
    ["Samba", "Brazilian", "Latin", "Carnival", "Fast", "Percussion", "Energetic"],
    rows={
        "kick":    "h.h.h.h.h.h.h.h.",
        "snare":   "..gm..gm..gm..gm",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "rim":     "..g.s.g...g.s.g.",
    })

add("Cha-Cha-Cha",
    ["Cha-Cha-Cha", "Cuban", "Latin", "Dance", "Classic", "Ballroom"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "....m......m.m.m",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "rim":     "..g...g...g...g.",
    })

add("Son Clave 3-2",
    ["Latin", "Son Clave", "Cuban", "Foundation", "Salsa", "Afro-Cuban"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "................",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "rim":     "h..h..h.....h.h.",
    })

add("Son Clave 2-3",
    ["Latin", "Son Clave", "Cuban", "Foundation", "Salsa", "Afro-Cuban"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "................",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "rim":     "....h.h.h..h..h.",
    })

add("Rumba Clave",
    ["Latin", "Rumba Clave", "Cuban", "Afro-Cuban", "Foundation", "Folkloric"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "................",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "rim":     "h..h...h....h.h.",
    })

add("Salsa Cascara",
    ["Salsa", "Latin", "Cascara", "Cuban", "Timbale", "Dance"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "................",
        "ride":    "m.mmm.m.m.mmm.m.",
        "rim":     "h..h..h.....h.h.",
    })

add("Mambo",
    ["Mambo", "Latin", "Cuban", "Big Band", "Dance", "Vintage"],
    rows={
        "kick":    "h.......h.....h.",
        "snare":   "....m.......m.m.",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "rim":     "h..h..h.....h.h.",
    })

add("Reggaeton Dembow",
    ["Reggaeton", "Dembow", "Latin", "Caribbean", "Modern", "Club", "Pop"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "...a..a....a..a.",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Afro-Cuban 6/8",
    ["Afro-Cuban", "6/8", "Latin", "Folkloric", "Bembe", "Percussion"],
    timesig=(6, 8), grid="16th",
    rows={
        "kick":    "h..h..h..h..",
        "snare":   "............",
        "rim":     "h..h.hh.hh.h",
        "hihat_c": "m.m.m.m.m.m.",
    })

# ════════════════════════════════════════════════════════════════════════════
# REGGAE / DUB
# ════════════════════════════════════════════════════════════════════════════

add("One Drop",
    ["Reggae", "One Drop", "Roots", "Jamaican", "Classic", "Smooth"],
    rows={
        "kick":    "........h.......",
        "snare":   "........a.......",
        "hihat_c": "m.m.m.m.m.m.m.m.",
        "rim":     "..m...m...m...m.",
    })

add("Rockers",
    ["Reggae", "Rockers", "Roots", "Jamaican", "Driving"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....a.......a...",
        "hihat_c": "..m...m...m...m.",
    })

add("Steppers",
    ["Reggae", "Steppers", "Roots", "Dub", "Marching", "Heavy"],
    rows={
        "kick":    "h.h.h.h.h.h.h.h.",
        "snare":   "....a.......a...",
        "hihat_c": "..m...m...m...m.",
    })

add("Dub",
    ["Dub", "Reggae", "Spacious", "Heavy", "Echo", "Bass"],
    rows={
        "kick":    "........h.......",
        "snare":   "....a...........",
        "hihat_c": "..m.....m.......",
        "rim":     "..g.......m...m.",
    })

# ════════════════════════════════════════════════════════════════════════════
# COUNTRY / FOLK / BLUES
# ════════════════════════════════════════════════════════════════════════════

add("Country Two-Step",
    ["Country", "Two-Step", "Honky-Tonk", "Dance", "Roots", "Americana"],
    rows={
        "kick":    "h...h...h...h...",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Country Shuffle",
    ["Country", "Shuffle", "Honky-Tonk", "Western", "Roots", "Swung"],
    grid="8tr",
    rows={
        "kick":    "h.....h.....",
        "snare":   "...a......a.",
        "hihat_c": "m.mm.mm.mm.m",
    })

add("Blues Shuffle",
    ["Blues", "Shuffle", "Swung", "Roots", "Bar Band", "12-Bar"],
    grid="8tr",
    rows={
        "kick":    "h.....h.....",
        "snare":   "...a......a.",
        "hihat_c": "m.mm.mm.mm.m",
    })

add("Country Train",
    ["Country", "Train", "Driving", "Cash-Adjacent", "Brushy", "Roots"],
    rows={
        "kick":    "h.h.h.h.h.h.h.h.",
        "snare":   "....a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

# ════════════════════════════════════════════════════════════════════════════
# JAZZ
# ════════════════════════════════════════════════════════════════════════════

add("Jazz Swing",
    ["Jazz", "Swing", "Triplet", "Ride", "Acoustic", "Bebop", "Standard"],
    grid="8tr",
    rows={
        "kick":    "h.....h.....",
        "ride":    "m.mm.mm.mm.m",
        "hihat_c": "...m.....m..",
        "snare":   "...g.....g..",
    })

add("Jazz Brushes",
    ["Jazz", "Brushes", "Soft", "Ballad", "Acoustic", "Smooth", "Slow"],
    grid="8tr",
    rows={
        "kick":    "h.....h.....",
        "snare":   "g.gg.gg.gg.g",
        "ride":    "m...m...m...",
    })

add("Jazz Bossa",
    ["Jazz", "Bossa", "Brazilian", "Latin Jazz", "Smooth", "Acoustic"],
    rows={
        "kick":    "h...h..hh..h..h.",
        "rim":     "..m...m..m..m...",
        "ride":    "m.m.m.m.m.m.m.m.",
    })

add("Jazz Waltz",
    ["Jazz", "Waltz", "3/4", "Triplet", "Acoustic", "Lyrical"],
    timesig=(3, 4), grid="8tr",
    rows={
        "kick":    "h........",
        "snare":   "...g..g..",
        "ride":    "m.mm.mm.m",
    })

add("Mambo Jazz",
    ["Jazz", "Latin Jazz", "Mambo", "Cuban", "Big Band"],
    rows={
        "kick":    "h.......h.......",
        "snare":   "....m.......m.m.",
        "ride":    "m.mmm.m.m.mmm.m.",
        "rim":     "h..h..h.....h.h.",
    })

# ════════════════════════════════════════════════════════════════════════════
# AFRICAN / WORLD
# ════════════════════════════════════════════════════════════════════════════

add("Afrobeat",
    ["Afrobeat", "African", "Fela-Adjacent", "Percussion", "Nigerian", "Polyrhythm"],
    rows={
        "kick":    "h..h.h..h..h.h..",
        "snare":   "....m..m....m..m",
        "hihat_c": "m.mgm.mgm.mgm.mg",
        "rim":     "g.g.g.g.g.g.g.g.",
    })

add("Highlife",
    ["Highlife", "African", "Ghanaian", "Dance", "Joyful", "Polyrhythm"],
    rows={
        "kick":    "h...h.h.h...h.h.",
        "snare":   "....m..m....m..m",
        "hihat_c": "m.m.m.m.m.m.m.m.",
    })

add("Soukous",
    ["Soukous", "African", "Congolese", "Dance", "Joyful", "Guitar-Driven"],
    rows={
        "kick":    "h.h.h.h.h.h.h.h.",
        "snare":   "....m.......m...",
        "hihat_c": "..m...m...m...m.",
    })

add("Bembe 6/8",
    ["African", "Bembe", "6/8", "Folkloric", "Percussion", "Cuban-Adjacent"],
    timesig=(6, 8), grid="16th",
    rows={
        "kick":    "h..h..h..h..",
        "rim":     "h.hh.hh.hh.h",
        "hihat_c": "m.m.m.m.m.m.",
    })

# ════════════════════════════════════════════════════════════════════════════
# METAL
# ════════════════════════════════════════════════════════════════════════════

add("Metal Standard",
    ["Metal", "Heavy", "Aggressive", "Driving", "Rock", "Modern"],
    rows={
        "kick":    "h..hh...h..hh...",
        "snare":   "....a.......a...",
        "hihat_c": "h...h...h...h...",
    })

add("Double Kick Metal",
    ["Metal", "Double Kick", "Heavy", "Fast", "Death", "Thrash"],
    rows={
        "kick":    "h.h.h.h.h.h.h.h.",
        "snare":   "....a.......a...",
        "ride":    "m...m...m...m...",
    })

add("Blast Beat",
    ["Metal", "Blast Beat", "Extreme", "Death", "Black", "Grindcore", "Fast"],
    grid="32nd",
    rows={
        "kick":    "h.h.h.h.h.h.h.h.h.h.h.h.h.h.h.h.",
        "snare":   ".h.h.h.h.h.h.h.h.h.h.h.h.h.h.h.h",
        "hihat_c": "h.h.h.h.h.h.h.h.h.h.h.h.h.h.h.h.",
    })

add("Gallop",
    ["Metal", "Gallop", "Heavy", "Driving", "Maiden-Adjacent", "Thrash"],
    grid="8tr",
    rows={
        "kick":    "h.hhh.hhh.hh",
        "snare":   "...a......a.",
        "ride":    "m...m...m...",
    })

add("Half-Time Metal",
    ["Metal", "Half-Time", "Slow", "Heavy", "Sludge", "Doom"],
    rows={
        "kick":    "h..h.h..h.......",
        "snare":   "........a.......",
        "hihat_c": "h...h...h...h...",
    })

# ════════════════════════════════════════════════════════════════════════════
# 3/4 — WALTZ
# ════════════════════════════════════════════════════════════════════════════

add("Waltz Basic",
    ["Waltz", "3/4", "Classic", "Ballroom", "Acoustic"],
    timesig=(3, 4), grid="16th",
    rows={
        "kick":    "h...........",
        "snare":   "....m...m...",
        "hihat_c": "m.m.m.m.m.m.",
    })

add("Country Waltz",
    ["Country", "Waltz", "3/4", "Roots", "Acoustic", "Slow"],
    timesig=(3, 4), grid="16th",
    rows={
        "kick":    "h...........",
        "snare":   "....m...m...",
        "hihat_c": "m.m.m.m.m.m.",
    })

add("Slow Waltz Ballad",
    ["Waltz", "Ballad", "3/4", "Slow", "Romantic", "Smooth"],
    timesig=(3, 4), grid="16th",
    rows={
        "kick":    "h...........",
        "snare":   "....s.......",
        "ride":    "m.m.m.m.m.m.",
    })

# ════════════════════════════════════════════════════════════════════════════
# 6/8 — COMPOUND
# ════════════════════════════════════════════════════════════════════════════

add("6/8 Slow Rock",
    ["6/8", "Slow Rock", "Compound", "Ballad", "Triplet Feel"],
    timesig=(6, 8), grid="16th",
    rows={
        "kick":    "h...........",
        "snare":   "...........a",
        "hihat_c": "m.m.m.m.m.m.",
    })

add("6/8 Blues",
    ["6/8", "Blues", "Compound", "Slow", "Triplet Feel"],
    timesig=(6, 8), grid="16th",
    rows={
        "kick":    "h.....h.....",
        "snare":   "...a......a.",
        "hihat_c": "m.m.m.m.m.m.",
    })

add("6/8 Soul",
    ["6/8", "Soul", "Ballad", "Compound", "Triplet Feel", "60s"],
    timesig=(6, 8), grid="16th",
    rows={
        "kick":    "h........h..",
        "snare":   "...a......a.",
        "ride":    "m.m.m.m.m.m.",
    })

add("6/8 Marching",
    ["6/8", "March", "Military", "Drumline", "Driving"],
    timesig=(6, 8), grid="16th",
    rows={
        "kick":    "h..h..h..h..",
        "snare":   "...m.....m..",
        "rim":     "...g.g...g.g",
    })

# ════════════════════════════════════════════════════════════════════════════
# 12/8 — SLOW BLUES / DOO-WOP
# ════════════════════════════════════════════════════════════════════════════

add("12/8 Slow Blues",
    ["12/8", "Blues", "Slow", "Compound", "Triplet Feel", "Bar Band"],
    timesig=(12, 8), grid="8th",
    rows={
        "kick":    "h.....h.....",
        "snare":   "...a......a.",
        "ride":    "m.mm.mm.mm.m",
    })

add("12/8 Doo-Wop",
    ["12/8", "Doo-Wop", "50s", "Slow", "Romantic", "Triplet"],
    timesig=(12, 8), grid="8th",
    rows={
        "kick":    "h.....h.....",
        "snare":   "...a......a.",
        "hihat_c": "m.mm.mm.mm.m",
    })

add("12/8 Soul Ballad",
    ["12/8", "Soul", "Ballad", "Slow", "Triplet Feel", "60s", "Motown-Adjacent"],
    timesig=(12, 8), grid="8th",
    rows={
        "kick":    "h........h..",
        "snare":   "...a......a.",
        "ride":    "m.mm.mm.mm.m",
    })

# ════════════════════════════════════════════════════════════════════════════
# 5/4 / 7/8 — ODD METER
# ════════════════════════════════════════════════════════════════════════════

add("Take 5/4",
    ["5/4", "Jazz", "Odd Meter", "Cool", "Modal", "Brubeck-Style"],
    timesig=(5, 4), grid="16th",
    rows={
        "kick":    "h.......h...........",
        "snare":   "....a..........a....",
        "ride":    "m.m.m.m.m.m.m.m.m.m.",
    })

add("5/4 Rock",
    ["5/4", "Rock", "Odd Meter", "Prog", "Driving"],
    timesig=(5, 4), grid="16th",
    rows={
        "kick":    "h.......h...........",
        "snare":   "....a.......a.......",
        "hihat_c": "m.m.m.m.m.m.m.m.m.m.",
    })

add("Balkan 7/8",
    ["7/8", "Balkan", "Odd Meter", "World", "Folk", "Energetic"],
    timesig=(7, 8), grid="16th",
    rows={
        "kick":    "h...h...h.....",
        "snare":   "....m.....m.m.",
        "hihat_c": "m.m.m.m.m.m.m.",
    })

add("Prog 7/8",
    ["7/8", "Prog", "Odd Meter", "Rock", "Technical"],
    timesig=(7, 8), grid="16th",
    rows={
        "kick":    "h.....h.....h.",
        "snare":   "....a.....a...",
        "hihat_c": "m.m.m.m.m.m.m.",
    })

# ════════════════════════════════════════════════════════════════════════════
# FILLS
# ════════════════════════════════════════════════════════════════════════════

add("Basic Fill",
    ["Fill", "Rock", "Standard", "Snare", "Transition"],
    type_="Sm. Fill",
    rows={
        "snare":   "........m.m.m.m.",
        "kick":    "h.......h.......",
        "tom_h":   "............h...",
        "tom_m":   "..............h.",
        "tom_l":   "...............h",
    })

add("Snare Roll Fill",
    ["Fill", "Snare Roll", "Build", "Transition", "Rock"],
    type_="Sm. Fill",
    grid="32nd",
    rows={
        "snare":   "................mmmmhhhhaaaahhhh",
        "kick":    "h...............................",
    })

add("Tom Fill Descending",
    ["Fill", "Tom Roll", "Descending", "Drum Solo", "Transition"],
    type_="Big Fill",
    rows={
        "tom_h":   "h.h.h.h.........",
        "tom_m":   "........h.h.....",
        "tom_l":   "............h.h.",
        "crash":   "...............a",
    })

add("Half-Time Snare Fill",
    ["Fill", "Half-Time", "Snare", "Hip Hop", "Modern"],
    type_="Sm. Fill",
    rows={
        "kick":    "h.......h.......",
        "snare":   "....g.g.a.g.a.gm",
        "hihat_c": "m.m.m.m.m.m.m...",
    })

add("Crash Resolution",
    ["Fill", "Crash", "Resolution", "End", "Climax"],
    type_="Sm. Fill",
    rows={
        "kick":    "h.......h.....h.",
        "snare":   "....m...m.m.m.m.",
        "crash":   "a...............",
    })

add("Triplet Tom Fill",
    ["Fill", "Triplet", "Tom", "Drum Solo", "Heavy"],
    type_="Big Fill",
    grid="8tr",
    rows={
        "tom_h":   "h.hh.h......",
        "tom_m":   "......h.hh.h",
        "kick":    "h...........",
    })

# ════════════════════════════════════════════════════════════════════════════
# 2-BAR EXAMPLES — show off variable bars
# ════════════════════════════════════════════════════════════════════════════

add("Hip Hop 2-Bar Pocket",
    ["Hip Hop", "Boom Bap", "2-Bar", "Pocket", "Variation", "Sampled"],
    bars=2,
    rows={
        "kick":    "h.....h.h...............h.h.h.h.",
        "snare":   "....a.......a.......a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.",
    })

add("Funk 2-Bar Groove",
    ["Funk", "2-Bar", "Groove", "Pocket", "Live", "Vintage"],
    bars=2,
    rows={
        "kick":    "h.....h..h.....h.h.....h....h...",
        "snare":   "....a.g.....a.g.....a.g.g...a.g.",
        "hihat_c": "m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.",
    })

add("Rock 2-Bar Driver",
    ["Rock", "2-Bar", "Driving", "Anthem", "Live"],
    bars=2,
    rows={
        "kick":    "h.......h.......h...h...h...h...",
        "snare":   "....a.......a.......a.......a...",
        "hihat_c": "m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.m.",
    })

add("House 2-Bar Variation",
    ["House", "2-Bar", "Club", "Dance", "Variation"],
    bars=2,
    rows={
        "kick":    "h...h...h...h...h...h...h...h...",
        "snare":   "....m.......m.......m.......m...",
        "hihat_c": "..m...m...m...m...m...m...m...m.",
        "hihat_o": "..h.....h.......h.....h.....h...",
    })


# ─── Output ────────────────────────────────────────────────────────────────

def main():
    if len(sys.argv) < 2:
        print("usage: generate_v2_presets.py <output-dir>", file=sys.stderr)
        sys.exit(2)
    out_dir = sys.argv[1]
    os.makedirs(out_dir, exist_ok=True)

    written = 0
    for p in PATTERNS:
        # Validate hit ticks fit within the pattern.
        for t, hs in p.hits.items():
            for tick, vel in hs:
                if tick < 0 or tick >= p.total_ticks:
                    raise SystemExit(
                        f"{p.name}: track {t} tick {tick} outside [0,{p.total_ticks})"
                    )
                if vel < 1 or vel > 127:
                    raise SystemExit(f"{p.name}: bad velocity {vel}")

        safe = "".join(c if c not in '/\\:*?"<>|' else "_" for c in p.name)
        path = os.path.join(out_dir, safe + ".beat")
        with open(path, "w") as f:
            f.write(serialize(p))
        written += 1

    print(f"wrote {written} patterns to {out_dir}")


if __name__ == "__main__":
    main()
