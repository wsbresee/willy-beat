#!/usr/bin/env python3
"""
beat2beat.py — Transcribe a drum audio file to a WillyBeat .beat preset.

Analyzes onset timing and spectral content to classify each hit as kick,
snare, hi-hat, tom, or cymbal, quantizes to a 16-step grid, and writes a
.beat file directly into ~/Library/Application Support/WillyBeat/Presets/
so it shows up in the plugin immediately.

When the input spans multiple bars, hits are majority-voted across bars so
incidental variations don't bleed into the core pattern.

Dependencies:
    pip install librosa soundfile

Usage examples:
    python beat2beat.py loop.wav
    python beat2beat.py loop.wav --name "My Funk Groove" --genre Funk
    python beat2beat.py full_song.wav --skip 8 --bars 4 --genre Rock
    python beat2beat.py loop.wav --bpm 95 --print
"""

import argparse
import re
import sys
from pathlib import Path
from typing import Optional

import numpy as np

try:
    import librosa
except ImportError:
    sys.exit("Missing dependency — run:  pip install librosa soundfile")


# ─── WillyBeat constants ──────────────────────────────────────────────────────

PRESETS_DIR = (
    Path.home() / "Library" / "Application Support" / "WillyBeat" / "Presets"
)

TRACK_KEYS = [
    "kick", "snare", "hihat_c", "hihat_o",
    "ride",  "crash",
    "tom_h", "tom_m", "tom_l", "rim",
]
NUM_TRACKS = len(TRACK_KEYS)
MAX_STEPS  = 16

PAT_TYPES = ["Regular", "Variance", "Sm. Fill", "Big Fill"]

# v2 file format constants. PPQN = 96; a 4/4 bar of 16ths is 16 steps × 24 ticks.
PPQN = 96
TICKS_PER_STEP = (PPQN * 4) // MAX_STEPS   # = 24


# ─── Spectral helpers ─────────────────────────────────────────────────────────

def _band_energy(S_lin: np.ndarray, freqs: np.ndarray,
                 fmin: float, fmax: float) -> float:
    mask = (freqs >= fmin) & (freqs <= fmax)
    return float(S_lin[mask].sum()) if mask.any() else 0.0


def classify_onset(y_frame: np.ndarray, sr: int, duration_sec: float) -> str:
    """
    Return a TRACK_KEYS string for the dominant instrument.

    Uses spectral band energy ratios and spectral centroid on the first ~50ms
    of the onset; `duration_sec` distinguishes open from closed hi-hats.
    """
    n_fft = 1024
    if len(y_frame) < n_fft:
        y_frame = np.pad(y_frame, (0, n_fft - len(y_frame)))

    S     = np.abs(librosa.stft(y_frame, n_fft=n_fft, center=False))
    S_lin = S.mean(axis=1)
    freqs = librosa.fft_frequencies(sr=sr, n_fft=n_fft)

    e_sub  = _band_energy(S_lin, freqs,    20,   120)   # kick sub
    e_low  = _band_energy(S_lin, freqs,   120,   350)   # kick / tom body
    e_mid  = _band_energy(S_lin, freqs,   350,  2500)   # snare / tom
    e_pres = _band_energy(S_lin, freqs,  2500,  7000)   # snare presence / crash
    e_air  = _band_energy(S_lin, freqs,  7000, 20000)   # hi-hat sizzle

    total = e_sub + e_low + e_mid + e_pres + e_air + 1e-12

    r_low  = (e_sub + e_low) / total
    r_mid  = e_mid            / total
    r_pres = e_pres           / total
    r_air  = e_air            / total

    centroid = float(np.sum(freqs * S_lin) / (np.sum(S_lin) + 1e-12))

    # ── Decision tree ──────────────────────────────────────────────────
    # High-frequency → cymbal family
    if r_air > 0.40:
        if r_pres + r_air > 0.75 and duration_sec < 0.14:
            return "hihat_c"
        if duration_sec > 0.22:
            return "hihat_o"
        if centroid > 9000:
            return "crash"
        return "ride"

    # Strong presence band with some air → crash or ride
    if r_pres > 0.35 and r_air > 0.20:
        return "crash"

    # Kick: strong low-band energy ratio is the most reliable indicator.
    # Centroid is deliberately NOT used here — high-frequency bleed (especially
    # from Demucs output) pulls the centroid well above 1 kHz even when 80 %+
    # of energy sits below 350 Hz.
    if r_low > 0.40:
        return "kick"

    # Mid-dominant → snare or rim
    if r_mid > 0.35:
        if duration_sec < 0.07 or centroid > 2000:
            return "rim"
        return "snare"

    # Low-mid with lower centroid → toms
    if r_low > 0.20:
        if centroid < 200:
            return "tom_l"
        if centroid < 400:
            return "tom_m"
        return "tom_h"

    return "snare"   # fallback


def _next_onset_gap(onset_idx: int, all_onsets: np.ndarray,
                    total_samples: int, sr: int) -> float:
    """Duration in seconds from this onset to the next (capped at 0.5 s)."""
    if onset_idx + 1 < len(all_onsets):
        gap_samples = int(all_onsets[onset_idx + 1]) - int(all_onsets[onset_idx])
    else:
        gap_samples = total_samples - int(all_onsets[onset_idx])
    return min(gap_samples / sr, 0.5)


# ─── Velocity helpers ─────────────────────────────────────────────────────────

def _rms(y: np.ndarray) -> float:
    return float(np.sqrt(np.mean(y ** 2)) + 1e-12)


def _rms_to_vel(rms: float, rms_min: float, rms_max: float) -> int:
    if rms_max <= rms_min:
        return 80
    norm = (rms - rms_min) / (rms_max - rms_min)
    return int(np.clip(norm * 127, 1, 127))


def vel_to_char(vel: int) -> str:
    if vel == 0:    return '.'
    if vel <= 30:   return 'g'
    if vel <= 65:   return 's'
    if vel <= 90:   return 'm'
    if vel <= 110:  return 'h'
    return 'a'


# ─── Core transcription ───────────────────────────────────────────────────────

def transcribe(
    audio_path: str,
    bpm_override: Optional[float] = None,
    bars: Optional[int] = None,
    skip_bars: int = 0,
    threshold: float = 0.5,
) -> dict:
    """
    Analyse an audio file and return a dict:
        tempo (float), bars_analyzed (int),
        velocities  list[NUM_TRACKS × MAX_STEPS  int 0-127],
        pattern     list[NUM_TRACKS  16-char str],
        hit_count   int
    """
    y, sr = librosa.load(audio_path, sr=None, mono=True)

    # ── BPM detection ──────────────────────────────────────────────────
    if bpm_override:
        tempo = float(bpm_override)
    else:
        # Run twice: once unbiased, once biased toward typical music (60-160).
        # Pick the result whose half-tempo or double-tempo is closer to 90 BPM
        # (the centre of most popular music).  This cures the common 2x error.
        raw1, beats1 = librosa.beat.beat_track(y=y, sr=sr, units='samples',
                                               tightness=100)
        raw2, beats2 = librosa.beat.beat_track(y=y, sr=sr, units='samples',
                                               tightness=100, start_bpm=90)
        t1 = float(np.atleast_1d(raw1)[0])
        t2 = float(np.atleast_1d(raw2)[0])

        # Normalise both into 60-180 by halving/doubling
        def _normalise(t: float) -> float:
            while t < 60:  t *= 2
            while t > 180: t /= 2
            return t

        t1n, t2n = _normalise(t1), _normalise(t2)
        # Prefer whichever normalised value is closer to 100 BPM
        tempo = t1n if abs(t1n - 100) <= abs(t2n - 100) else t2n

    step_samples = (60.0 / tempo) * sr / 4.0    # one 16th note in samples
    bar_samples  = step_samples * MAX_STEPS

    total_bars   = max(1, int(len(y) / bar_samples))
    start_bar    = min(skip_bars, total_bars - 1)

    if bars is None:
        bars_to_use = min(total_bars - start_bar, 8)
    else:
        bars_to_use = min(bars, total_bars - start_bar)
    bars_to_use = max(1, bars_to_use)

    # ── Analysis window ────────────────────────────────────────────────
    start_samp = int(start_bar * bar_samples)
    end_samp   = min(int((start_bar + bars_to_use) * bar_samples), len(y))
    y_analysis = y[start_samp:end_samp]

    hop_length = 512
    min_wait   = max(1, int(step_samples / hop_length * 0.35))  # ~35% of a 16th note

    onset_frames = librosa.onset.onset_detect(
        y=y_analysis, sr=sr,
        hop_length=hop_length,
        backtrack=True,
        delta=0.04,
        wait=min_wait,
        units='frames',
    )
    # Convert to absolute sample positions
    onset_abs = (librosa.frames_to_samples(onset_frames, hop_length=hop_length)
                 + start_samp)

    # ── Classify and bin each onset ────────────────────────────────────
    classify_window = int(sr * 0.05)   # 50 ms for spectral fingerprint
    rms_window      = int(sr * 0.03)   # 30 ms for velocity (attack only)

    hit_data = []   # (bar_idx, step_idx, track, rms)

    for i, onset in enumerate(onset_abs):
        onset = int(onset)
        rel   = onset - start_samp
        bar_i = int(rel / bar_samples)
        offs  = rel - bar_i * bar_samples
        step  = int(round(offs / step_samples)) % MAX_STEPS

        y_frame = y[onset: onset + classify_window]
        if len(y_frame) < 32:
            continue

        dur   = _next_onset_gap(i, onset_abs, len(y), sr)
        track = classify_onset(y_frame, sr, dur)

        y_vel = y[onset: onset + rms_window]
        rms   = _rms(y_vel)

        hit_data.append((bar_i, step, track, rms))

    if not hit_data:
        # Return an empty (silence) pattern
        velocities = [[0] * MAX_STEPS for _ in range(NUM_TRACKS)]
        pattern    = ['.' * MAX_STEPS] * NUM_TRACKS
        return dict(tempo=tempo, bars_analyzed=bars_to_use,
                    velocities=velocities, pattern=pattern, hit_count=0)

    # ── Per-bar grids then majority vote ──────────────────────────────
    # per_bar[b][track_idx][step] = max rms seen
    per_bar = [[{} for _ in range(NUM_TRACKS)] for _ in range(bars_to_use)]

    track_idx = {k: i for i, k in enumerate(TRACK_KEYS)}
    for (bar_i, step, track, rms) in hit_data:
        if 0 <= bar_i < bars_to_use:
            ti = track_idx[track]
            per_bar[bar_i][ti][step] = max(per_bar[bar_i][ti].get(step, 0.0), rms)

    # Global RMS range for velocity scaling
    all_rms = [r for (_, _, _, r) in hit_data]
    rms_min, rms_max = min(all_rms), max(all_rms)

    min_votes = max(1, round(bars_to_use * threshold))

    velocities = [[0] * MAX_STEPS for _ in range(NUM_TRACKS)]
    for ti in range(NUM_TRACKS):
        for step in range(MAX_STEPS):
            vals = [per_bar[b][ti][step]
                    for b in range(bars_to_use)
                    if step in per_bar[b][ti]]
            if len(vals) >= min_votes:
                median_rms = float(np.median(vals))
                velocities[ti][step] = _rms_to_vel(median_rms, rms_min, rms_max)

    pattern = [
        ''.join(vel_to_char(velocities[ti][s]) for s in range(MAX_STEPS))
        for ti in range(NUM_TRACKS)
    ]

    return dict(
        tempo=tempo,
        bars_analyzed=bars_to_use,
        velocities=velocities,
        pattern=pattern,
        hit_count=len(hit_data),
    )


# ─── .beat file writer ────────────────────────────────────────────────────────

def format_beat(name: str, genres: str | list, pat_type: str,
                velocities: list) -> str:
    """Emit a v2 .beat file from a 10×16 velocity grid (each cell 0-127).

    genres may be a comma-separated string or a list of tag strings.
    The transcription always produces a single 4/4 bar of 16ths, so timesig,
    bars, and grid are fixed.
    """
    if isinstance(genres, list):
        genres_str = ", ".join(g.strip() for g in genres if g.strip())
    else:
        genres_str = genres

    active = sum(1 for row in velocities for v in row if v > 0)
    density = active / (NUM_TRACKS * MAX_STEPS)

    lines = [
        "# WillyBeat Preset v2",
        "version: 2",
        f"name:    {name}",
        f"genres:  {genres_str}",
        f"type:    {pat_type}",
        "timesig: 4/4",
        "bars:    1",
        "grid:    16th",
        f"density: {density:.4f}",
        "",
        '# Hits per track as "tick=velocity" pairs. PPQN = 96.',
    ]
    for ti, key in enumerate(TRACK_KEYS):
        pairs = [f"{step * TICKS_PER_STEP}={velocities[ti][step]}"
                 for step in range(MAX_STEPS) if velocities[ti][step] > 0]
        prefix = key.ljust(8)
        lines.append(f"{prefix} {', '.join(pairs)}" if pairs else prefix)
    return '\n'.join(lines) + '\n'


# ─── Preview printer ─────────────────────────────────────────────────────────

def print_preview(result: dict):
    beats   = "  1 . . . 2 . . . 3 . . . 4 . . ."
    print(f"\n  {'':9s}{beats}")
    print(f"  {'':9s}" + "-" * len(beats))

    for ti, key in enumerate(TRACK_KEYS):
        row = "  ".join(vel_to_char(result["velocities"][ti][s])
                        for s in range(MAX_STEPS))
        # Highlight non-empty rows
        has_hit = any(result["velocities"][ti][s] for s in range(MAX_STEPS))
        marker  = "●" if has_hit else " "
        print(f"  {key:<9s}{marker} {row}")
    print()


# ─── CLI ──────────────────────────────────────────────────────────────────────

def _safe_name(s: str) -> str:
    return re.sub(r'[/\\:*?"<>|]', '_', s).strip() or "pattern"


def main():
    ap = argparse.ArgumentParser(
        description="Transcribe a drum audio file to a WillyBeat .beat preset.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
examples:
  python beat2beat.py loop.wav
  python beat2beat.py loop.wav --name "Funky Loop" --genre Funk
  python beat2beat.py song.wav --skip 8 --bars 4 --bpm 93
  python beat2beat.py loop.wav --print        # preview without saving
        """,
    )
    ap.add_argument("audio",               help="Input audio file (wav/aiff/mp3/flac/…)")
    ap.add_argument("--name",  default=None,
                    help="Preset name  (default: audio filename)")
    ap.add_argument("--genre", default="Rock",
                    help="Genre tag(s), comma-separated  (default: Rock)")
    ap.add_argument("--type",  default="Regular", choices=PAT_TYPES, dest="pat_type",
                    help="Pattern type (default: Regular)")
    ap.add_argument("--bpm",   type=float, default=None,
                    help="Override auto-detected BPM")
    ap.add_argument("--bars",  type=int,   default=None,
                    help="Bars to analyse  (default: auto, up to 8)")
    ap.add_argument("--skip",  type=int,   default=0,
                    help="Skip N bars at the start  (useful to skip an intro)")
    ap.add_argument("--threshold", type=float, default=0.5, metavar="0-1",
                    help="Fraction of bars a hit must appear in to be kept  (default: 0.5)")
    ap.add_argument("--output", default=None,
                    help="Output directory  (default: WillyBeat presets folder)")
    ap.add_argument("--print",  action="store_true",
                    help="Print .beat content to stdout instead of writing a file")
    args = ap.parse_args()

    if not Path(args.audio).exists():
        sys.exit(f"File not found: {args.audio}")

    name = args.name or Path(args.audio).stem

    print(f"\nLoading  {args.audio}")
    result = transcribe(
        args.audio,
        bpm_override=args.bpm,
        bars=args.bars,
        skip_bars=args.skip,
        threshold=args.threshold,
    )

    print(f"  BPM detected:  {result['tempo']:.1f}"
          + (f"  (overridden to {args.bpm})" if args.bpm else ""))
    print(f"  Bars analysed: {result['bars_analyzed']}")
    print(f"  Onsets found:  {result['hit_count']}")

    print_preview(result)

    beat_text = format_beat(name, args.genre, args.pat_type, result["velocities"])

    if args.print:
        print(beat_text)
        return

    out_dir = Path(args.output) if args.output else PRESETS_DIR
    out_dir.mkdir(parents=True, exist_ok=True)

    out_path = out_dir / (_safe_name(name) + ".beat")
    out_path.write_text(beat_text)
    print(f"Written → {out_path}")
    print("Reload the plugin (or restart Cubase) to see the new preset.\n")


if __name__ == "__main__":
    main()
