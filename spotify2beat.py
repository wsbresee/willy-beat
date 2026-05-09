#!/usr/bin/env python3
"""
spotify2beat.py — Search Spotify for a song and transcribe its drums to a
WillyBeat .beat preset, with no manual audio download needed.

How it works:
  1. Searches Spotify for your query → shows top results to pick from
  2. Pulls BPM and artist genres from the Spotify API (no guessing)
  3. Downloads the audio from YouTube via yt-dlp
  4. Optionally separates the drum stem with Demucs (--stems, much better results)
  5. Transcribes using beat2beat.py logic and writes the preset

The raw Spotify genre tags (e.g. "funk", "boom bap", "electropop") are written
directly to the .beat file as genre tags — no fixed 6-genre mapping.

Setup (one time):
  1. Create a free Spotify app at https://developer.spotify.com/dashboard
  2. Add these to your shell (e.g. ~/.zshrc):
       export SPOTIFY_CLIENT_ID="your_client_id"
       export SPOTIFY_CLIENT_SECRET="your_client_secret"

Dependencies:
  pip install spotipy yt-dlp  (librosa soundfile already in requirements.txt)
  brew install ffmpeg

Optional (highly recommended for full-mix tracks):
  pip install demucs           runs in ~1 min, makes a huge accuracy difference

Usage:
  python spotify2beat.py "funky drummer james brown"
  python spotify2beat.py "billie jean" --stems
  python spotify2beat.py "money pink floyd" --auto --stems
  python spotify2beat.py "four on the floor" --bars 2
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Optional

# ── Dependency checks ─────────────────────────────────────────────────────────

def _require(pkg: str, install: str):
    try:
        __import__(pkg)
    except ImportError:
        sys.exit(f"Missing dependency — run:  pip install {install}")

_require("spotipy",  "spotipy")
_require("yt_dlp",   "yt-dlp")

import spotipy
from spotipy.oauth2 import SpotifyClientCredentials
import yt_dlp

# ── Import transcription helpers from beat2beat ───────────────────────────────

sys.path.insert(0, str(Path(__file__).parent))
from beat2beat import transcribe, format_beat, print_preview, PRESETS_DIR

def build_genre_tags(spotify_genres: list) -> list:
    """Return Spotify genre strings as WillyBeat tags.

    Each Spotify genre is title-cased and included directly.  Up to 5 tags
    are kept so the pattern isn't cluttered; the most-specific (longest) tags
    are preferred since they carry more information.
    """
    if not spotify_genres:
        return ["Unknown"]
    # Title-case each tag, deduplicate, sort longest-first, keep up to 5
    tags = list({g.title() for g in spotify_genres if g.strip()})
    tags.sort(key=len, reverse=True)
    return tags[:5]


# ── Spotify helpers ───────────────────────────────────────────────────────────

def _make_spotify() -> spotipy.Spotify:
    client_id     = os.environ.get("SPOTIFY_CLIENT_ID")
    client_secret = os.environ.get("SPOTIFY_CLIENT_SECRET")
    if not client_id or not client_secret:
        sys.exit(
            "\nSpotify credentials not set.\n\n"
            "  1. Create a free app at https://developer.spotify.com/dashboard\n"
            "  2. Copy the Client ID and Client Secret, then add to ~/.zshrc:\n"
            "       export SPOTIFY_CLIENT_ID=\"your_id\"\n"
            "       export SPOTIFY_CLIENT_SECRET=\"your_secret\"\n"
            "  3. Open a new shell (or: source ~/.zshrc)\n"
        )
    return spotipy.Spotify(
        auth_manager=SpotifyClientCredentials(
            client_id=client_id,
            client_secret=client_secret,
        )
    )


def search_tracks(sp: spotipy.Spotify, query: str, n: int = 5) -> list:
    """Return up to n track dicts with keys: id, name, artist, genres, duration_ms."""
    results = sp.search(q=query, type="track", limit=n)
    tracks  = []
    for item in results["tracks"]["items"]:
        artist_obj = item["artists"][0]
        # Genres live on the artist, not the track — fetch them
        try:
            artist_data = sp.artist(artist_obj["id"])
            genres = artist_data.get("genres", [])
        except Exception:
            genres = []

        tracks.append({
            "id":          item["id"],
            "name":        item["name"],
            "artist":      artist_obj["name"],
            "genres":      genres,
            "duration_ms": item["duration_ms"],
            "bpm":         None,   # filled in below if available
            "skip_secs":   0.0,
        })
    return tracks


def enrich_with_audio_features(sp: spotipy.Spotify, track: dict) -> dict:
    """
    Try to add BPM and intro-skip from Spotify's audio analysis.
    Spotify restricted these endpoints in late 2024, so we gracefully skip
    if the call returns 403.
    """
    try:
        feat = sp.audio_features(track["id"])
        if feat and feat[0]:
            track["bpm"] = feat[0].get("tempo")
    except Exception:
        pass

    try:
        analysis = sp.audio_analysis(track["id"])
        sections = analysis.get("sections", [])
        if sections and track["bpm"]:
            # Skip intro: find first section whose loudness is within 4 dB of
            # the track average and lasts at least 5 s
            avg_loud = analysis["track"]["loudness"]
            for section in sections:
                if (section["loudness"] > avg_loud - 4
                        and section["duration"] > 5.0
                        and section["start"] > 1.0):
                    track["skip_secs"] = section["start"]
                    break
    except Exception:
        pass

    return track


# ── YouTube download ──────────────────────────────────────────────────────────

def download_audio(artist: str, title: str, out_dir: str) -> Optional[str]:
    """Download the best audio match for '{artist} {title}' from YouTube."""
    search_query = f"ytsearch1:{artist} {title} official audio"

    outtmpl = str(Path(out_dir) / "%(title)s.%(ext)s")
    ydl_opts = {
        "format":        "bestaudio/best",
        "outtmpl":       outtmpl,
        "quiet":         True,
        "no_warnings":   True,
        "postprocessors": [{
            "key":            "FFmpegExtractAudio",
            "preferredcodec": "wav",
        }],
    }

    info = None
    with yt_dlp.YoutubeDL(ydl_opts) as ydl:
        info = ydl.extract_info(search_query, download=True)
        if info and "entries" in info:
            info = info["entries"][0]

    if not info:
        return None

    # yt-dlp stores the pre-postprocessor filename; the extension is now .wav
    raw_path  = Path(ydl_opts["outtmpl"] % info if "%" in ydl_opts["outtmpl"]
                     else ydl.prepare_filename(info))
    wav_path  = raw_path.with_suffix(".wav")
    if wav_path.exists():
        return str(wav_path)

    # Fallback: any wav in the directory
    wavs = sorted(Path(out_dir).glob("*.wav"))
    return str(wavs[0]) if wavs else None


# ── Demucs drum separation ────────────────────────────────────────────────────

def separate_drums(audio_path: str, out_dir: str) -> Optional[str]:
    """
    Run Demucs to isolate the drum stem.  Returns path to drums.wav, or None
    if Demucs isn't installed or fails.
    """
    try:
        result = subprocess.run(
            [sys.executable, "-m", "demucs",
             "--two-stems=drums", "-o", out_dir, audio_path],
            capture_output=True, text=True,
        )
        if result.returncode != 0:
            print(f"  Demucs error: {result.stderr[:300]}")
            return None
    except FileNotFoundError:
        print("  Demucs not installed — run:  pip install demucs")
        return None

    # Demucs writes to {out_dir}/{model_name}/{track_stem}/drums.wav
    stem = Path(audio_path).stem
    for candidate in sorted(Path(out_dir).glob(f"*/{stem}/drums.wav")):
        return str(candidate)

    return None


# ── CLI ───────────────────────────────────────────────────────────────────────

def _safe_name(s: str) -> str:
    return re.sub(r'[/\\:*?"<>|]', "_", s).strip() or "pattern"


def main():
    ap = argparse.ArgumentParser(
        description="Search Spotify for a song and transcribe its drums to a WillyBeat preset.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
examples:
  python spotify2beat.py "funky drummer james brown"
  python spotify2beat.py "billie jean" --stems
  python spotify2beat.py "money pink floyd" --auto --stems
  python spotify2beat.py "amen break" --genre "Electronic, Breakbeat"
        """,
    )
    ap.add_argument("query",               help="Song search query (title, artist, etc.)")
    ap.add_argument("--auto",   action="store_true",
                    help="Auto-select the first Spotify result without prompting")
    ap.add_argument("--stems",  action="store_true",
                    help="Run Demucs to isolate the drum stem first (slower, more accurate)")
    ap.add_argument("--genre",  default=None,
                    help="Override genre tags (comma-separated, e.g. 'Funk, Soul')")
    ap.add_argument("--bars",   type=int, default=4,
                    help="Bars to analyse from the main section  (default: 4)")
    ap.add_argument("--threshold", type=float, default=0.5, metavar="0-1",
                    help="Hit vote threshold across bars  (default: 0.5)")
    ap.add_argument("--output", default=None,
                    help="Output directory  (default: WillyBeat presets folder)")
    ap.add_argument("--print",  action="store_true",
                    help="Print .beat content to stdout instead of writing a file")
    args = ap.parse_args()

    if not shutil.which("ffmpeg"):
        sys.exit("ffmpeg not found — run:  brew install ffmpeg")

    # ── Spotify search ────────────────────────────────────────────────────────
    sp = _make_spotify()

    print(f'\nSearching Spotify for "{args.query}" …')
    tracks = search_tracks(sp, args.query)
    if not tracks:
        sys.exit("No results found — try a different query.")

    # Enrich the first handful with audio features (may fail on restricted apps)
    for t in tracks[:5]:
        enrich_with_audio_features(sp, t)

    # ── Track selection ───────────────────────────────────────────────────────
    if args.auto or len(tracks) == 1:
        chosen = tracks[0]
    else:
        print()
        for i, t in enumerate(tracks, 1):
            bpm_str  = f"  {t['bpm']:.0f} BPM" if t["bpm"] else ""
            genres   = ", ".join(t["genres"][:2]) if t["genres"] else "unknown genre"
            duration = f"{t['duration_ms'] // 60000}:{(t['duration_ms'] // 1000 % 60):02d}"
            print(f"  {i}. {t['name']} — {t['artist']}"
                  f"  [{genres}]{bpm_str}  {duration}")

        print()
        raw = input(f"Select track [1–{len(tracks)}] or Enter for #1: ").strip()
        idx = (int(raw) - 1) if raw.isdigit() and 1 <= int(raw) <= len(tracks) else 0
        chosen = tracks[idx]

    # ── Infer settings ────────────────────────────────────────────────────────
    if args.genre:
        genre_tags = [g.strip() for g in args.genre.split(",") if g.strip()]
    else:
        genre_tags = build_genre_tags(chosen["genres"])

    bpm = chosen["bpm"]    # None if API restricted

    step_secs = (60.0 / bpm / 4) if bpm else None
    skip_bars = int(chosen["skip_secs"] / (step_secs * 16)) if step_secs else 0

    print(f"\n  Track:  {chosen['name']} — {chosen['artist']}")
    if bpm:
        print(f"  BPM:    {bpm:.1f}  (from Spotify)")
    else:
        print("  BPM:    auto-detect  (Spotify audio features unavailable)")
    print(f"  Tags:   {', '.join(genre_tags)}")
    print(f"  Skip:   {skip_bars} intro bars")
    print(f"  Bars:   {args.bars}")

    # ── Download ──────────────────────────────────────────────────────────────
    tmp = tempfile.mkdtemp(prefix="willybeat_")
    try:
        print(f"\nDownloading audio from YouTube …")
        audio_path = download_audio(chosen["artist"], chosen["name"], tmp)
        if not audio_path:
            sys.exit("Download failed — check your internet connection.")
        print(f"  ✓ {Path(audio_path).name}")

        # ── Optional stem separation ──────────────────────────────────────────
        if args.stems:
            print("Separating drum stem with Demucs … (this takes ~1 min)")
            drum_path = separate_drums(audio_path, tmp)
            if drum_path:
                print(f"  ✓ drum stem isolated")
                audio_path = drum_path
            else:
                print("  ✗ stem separation failed — transcribing full mix")

        # ── Transcribe ────────────────────────────────────────────────────────
        print("Transcribing drums …")
        result = transcribe(
            audio_path,
            bpm_override=bpm,
            bars=args.bars,
            skip_bars=skip_bars,
            threshold=args.threshold,
        )

        print(f"  ✓ {result['tempo']:.1f} BPM  ·  "
              f"{result['bars_analyzed']} bars  ·  "
              f"{result['hit_count']} onsets\n")

        print_preview(result)

        name      = chosen["name"]
        beat_text = format_beat(name, genre_tags, "Regular", result["pattern"])

        if args.print:
            print(beat_text)
            return

        out_dir = Path(args.output) if args.output else PRESETS_DIR
        out_dir.mkdir(parents=True, exist_ok=True)
        out_path = out_dir / (_safe_name(name) + ".beat")
        out_path.write_text(beat_text)
        print(f"Written → {out_path}")
        print("Reload WillyBeat (or restart Cubase) to see the new preset.\n")

    finally:
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
