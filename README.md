# WillyBeat

A MIDI drum-pattern generator built with JUCE. Type vibe words ("Aggressive Trap", "Smoky Trip-Hop", "Bossa"), hit **Generate**, edit on a flexible grid (any time signature, any subdivision, triplets included), drag the resulting MIDI clip into your DAW. Ships as a VST3 and an Audio Unit instrument plugin: it produces MIDI for routing to other samplers and ships its own simple synthesized drum kit for in-plugin preview (off by default — toggle the **Audio** button in the title bar).

> **macOS / Apple Silicon.** Semantic tag search uses the system `NaturalLanguage` framework (macOS 11+). Windows / Intel builds need a different embedder.

## Build & install

```bash
git clone https://github.com/wsbresee/willy-beat.git
cd willy-beat
cmake -B build
cmake --build build --target WillyBeat_VST3 WillyBeat_AU
```

`COPY_PLUGIN_AFTER_BUILD` copies the artefacts into the system plugin folders automatically:

- VST3 → `~/Library/Audio/Plug-Ins/VST3/WillyBeat.vst3`
- AU → `~/Library/Audio/Plug-Ins/Components/WillyBeat.component`

The plugin reads its preset library from `~/Library/Application Support/WillyBeat/Presets/`. Install the bundled library with:

```bash
./install_presets.sh           # copy any missing presets, skip existing
./install_presets.sh --force   # overwrite all
```

New patterns and edits save back to that folder.

---

## Cubase (VST3)

### Install

1. Build with `cmake --build build --target WillyBeat_VST3`.
2. Open Cubase. **Studio → VST Plug-in Manager**, refresh.
3. Confirm `WillyBeat` shows up under **Instruments**.

### Set up a track

Two routes:

- **Use WillyBeat's built-in drum sounds**: **Project → Add Track → Instrument**, pick **WillyBeat**, click the title-bar **Audio** toggle to unmute the internal kit, hit play. No external sampler needed.
- **Route MIDI to a real sampler**: Add WillyBeat on its own instrument track (audio muted). Add a Groove Agent SE track. In Cubase's MIDI input routing, set Groove Agent's MIDI input to WillyBeat's MIDI output.

---

## Logic Pro (AU)

### Install

1. Build with `cmake --build build --target WillyBeat_AU`.
2. Quit Logic. Launch it again — Logic auto-validates new components on startup.
3. If validation seems stuck, run `auval -a | grep -i willybeat` from Terminal to confirm the AU is registered, then `killall -9 AudioComponentRegistrar` and reopen Logic.

### Set up a track

- **Built-in sounds**: **Track → New Software Instrument Track**, pick **AU Instruments → WillyBresee → WillyBeat**. Click the title-bar **Audio** toggle to unmute the internal kit.
- **Route MIDI**: Load WillyBeat on its own instrument track. Add a second track with Drum Kit Designer / Drum Machine Designer. Route via IAC bus or **External MIDI**.

---

## Workflow

1. **Type tags** in the top-left chip bar. Vibe words work as well as genres: `Aggressive Trap`, `Coltrane-style swing` (no actual artist names but mood words), `Smoky Lounge`, `Driving 5/4 Math Rock`. Press Enter — semantic search via Apple's `NLEmbedding` matches what you typed to the closest tags in the library. Each press auto-fires **Generate**.
2. **Pick a shape**: above the grid, three combos let you set **Time Sig** (4/4, 3/4, 2/4, 5/4, 6/8, 7/8, 12/8), **Bars** (1, 2, 4, 8), and **Grid** (8th, 8th triplet, 16th, 16th triplet, 32nd). Changing time-sig or bars adjusts the pattern length; changing Grid only changes the view.
3. **Edit cells** on the grid. Left-click empty → max-velocity hit. Left-click filled → clear. Drag vertically → tune velocity (up = louder). Scroll → finer velocity steps. Right-click → clear. Hovering shows the current velocity as a fading number on the cell.
4. **Tweak macros**: Duration, Dynamics, Slop, Swing, Density.
5. **Drag the Drag-to-DAW strip** onto a MIDI track to commit. The exported clip uses the current shape + fills.

If you'd rather route MIDI live, leave WillyBeat as a MIDI source and the downstream sampler plays in real time — no drag needed.

## The interface

- **Chip bar (top-left)** — filter / search tags. Press Enter to generate. Semantic search means `Coltrane swing` finds modal-jazz patterns even if no preset literally has that word. Backspace removes the rightmost chip.
- **Pattern #** — step through patterns matching the active filter.
- **Generate** — re-roll a composite from tagged sources. Each click produces a different mix; sources are shape-filtered (4/4 patterns won't mix with 6/8 ones).
- **Drag to DAW** — drag a MIDI file of the current pattern onto a track. Honours the pattern's time sig, bars, and grid; fills are blended in at the configured Start / Mid / End positions.
- **Audio toggle** (top-right) — turns the internal preview drum kit on/off. Default off — when on, simple synthesized drum sounds play via WillyBeat's audio output as the DAW transport rolls.
- **Collapse / expand toggle** (`-` / `+` top-right) — shrinks the editor into a mini view: clickable thumbnail of the pattern on the left, the five macro rotaries in the middle, and the three Fill rotaries on the right. Click the thumbnail to expand back to the full grid.
- **Macro knobs** — Duration, Dynamics, Slop, Swing, Density.
  - **Swing label is clickable** to toggle between **16th**-note swing (default) and **8th**-note swing. The label reads "Swing 16" or "Swing 8" so you can see the current target.
- **Time Sig / Bars / Grid combos** (above the grid) — pattern shape controls. Each pattern remembers its own grid subdivision.
- **Pattern grid** — variable cell count based on shape × grid. Triplet grids are visually shaded in alternating groups so straight vs swung is unmistakable. Bar lines are bold, beat lines lighter. The play cursor follows the current tick within the pattern.
  - **Scrolls horizontally** when the natural width (≥18 px per cell) exceeds the viewport. Track labels stay pinned on the left.
- **Fill row** — Start / Mid / End rotaries pull notes from a matching fill pattern at the start, middle, and end of the exported clip.

## Pattern library

Patterns live as plain-text `.beat` files in `~/Library/Application Support/WillyBeat/Presets/`. The v2 format is self-describing:

```
# WillyBeat Preset v2
version: 2
name:    Boom Bap Pocket
genres:  Hip-Hop, Boom Bap, 90s, Underground, Sample-based
type:    Regular
timesig: 4/4
bars:    1
grid:    16th
density: 0.225

# Hits per track as "tick=velocity" pairs. PPQN = 96.
kick     0=100, 192=100
snare    96=100, 288=100
hihat_c  0=80, 48=80, 96=80, 144=80, 192=80, 240=80, 288=80, 336=80
hihat_o
ride
crash
tom_h
tom_m
tom_l
rim
```

- **PPQN = 96** is the resolution used by all `.beat` files. A 4/4 1-bar pattern is 384 ticks; a quarter is 96 ticks; a 16th is 24 ticks; an 8th triplet is 32 ticks; a 16th triplet is 16 ticks.
- **No proper nouns** in the bundled library. Names and tags describe feel, era, scene, and instrumentation rather than artists, songs, albums, or labels.
- Drop new files into the folder; reload the plugin to pick them up. Build-time, the bundled tag vocabulary is pre-embedded into the plugin binary so search is instant on first open.

## Versioning

- **v1.0.0** ([tag](https://github.com/wsbresee/willy-beat/releases/tag/v1.0.0)): fixed 4/4, 16-step grid; song-titled presets; LLM-enriched tag bank.
- **v2** (this branch): tick-based engine, variable time-sig / bar count / grid subdivision, triplets, scrollable grid, 8th/16th swing target, clean-vocabulary preset bank without artist references.
