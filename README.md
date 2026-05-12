# WillyBeat

A MIDI drum-pattern generator built with JUCE. Type genre tags, hit
**Generate**, drag the resulting MIDI clip into your DAW. Ships as a VST3 and
an Audio Unit instrument plugin: it produces MIDI for routing to other
samplers and ships its own simple synthesized drum kit for in-plugin
preview (off by default — toggle the "Audio" button in the title bar).

## Build & install

```bash
git clone https://github.com/wsbresee/willy-beat.git
cd willy-beat
cmake -B build
cmake --build build --target WillyBeat_VST3 WillyBeat_AU
```

`COPY_PLUGIN_AFTER_BUILD` copies the artefacts into the system plugin folders
automatically:

- VST3 → `~/Library/Audio/Plug-Ins/VST3/WillyBeat.vst3`
- AU → `~/Library/Audio/Plug-Ins/Components/WillyBeat.component`

The first time the plugin loads it writes its preset library to
`~/Library/Application Support/WillyBeat/Presets/`. New patterns and tag
edits save back to that folder.

---

## Cubase (VST3)

### Install

1. Build with `cmake --build build --target WillyBeat_VST3` (the VST3 lands
   in `~/Library/Audio/Plug-Ins/VST3/`).
2. Open Cubase. Go to **Studio → VST Plug-in Manager** and click the refresh
   icon, or rescan from **Studio → Studio Setup → VST Plug-in Manager**.
3. Confirm `WillyBeat` shows up under **MIDI Effects**.

### Set up a track

WillyBeat is now an instrument plugin. Two routes:

- **Use WillyBeat's built-in drum sounds**: **Project → Add Track → Instrument**,
  pick **WillyBeat**, click the title-bar **Audio** toggle to unmute the
  internal kit, hit play. No external sampler needed.
- **Route MIDI to a real sampler**: Add WillyBeat on its own instrument
  track (audio muted). Add a Groove Agent SE track. In Cubase's MIDI input
  routing, set Groove Agent's MIDI input to WillyBeat's MIDI output.

### Use it

1. Type a genre tag in the top-left chip bar (e.g. `Trap`, `Boom Bap`,
   `House`) and press Enter. Add as many tags as you like — patterns
   matching any of them are in scope.
2. Click **Generate**. A composite pattern is built from the tagged
   sources, the grid populates, and playback starts using it the moment
   transport rolls.
3. Iterate: tweak Gate / Humanize / Swing / Feel / Density, edit cells in
   the grid (left-click cycles velocity, right-click clears).
4. To commit a clip, drag the **Drag to DAW** button onto a MIDI track.
   Cubase drops a MIDI part with the bar count, fills, and seed configured
   below the grid.

If you'd rather route MIDI live, leave WillyBeat as a MIDI insert and the
sampler underneath plays in real time — no drag needed.

---

## Logic Pro (AU)

### Install

1. Build with `cmake --build build --target WillyBeat_AU`.
2. Quit Logic. Launch it again — Logic auto-validates new components on
   startup and the first launch may take a minute.
3. If validation seems stuck, run
   `auval -a | grep -i willybeat` from Terminal to confirm the AU is
   registered, then `killall -9 AudioComponentRegistrar` and reopen Logic.

### Set up a track

WillyBeat is now an AU instrument. Two routes:

- **Use WillyBeat's built-in drum sounds**: **Track → New Software
  Instrument Track**, pick **AU Instruments → WillyBresee → WillyBeat**.
  Click the title-bar **Audio** toggle to unmute the internal kit. Hit
  play.
- **Route MIDI to a real sampler**: Load WillyBeat on its own software
  instrument track. Add a second instrument track with Drum Kit Designer
  / Drum Machine Designer. Use Logic's IAC bus or the **External MIDI**
  routing in the second track's input to receive WillyBeat's MIDI
  output.

### Use it

Same flow as Cubase:

1. Add tags, click **Generate**, listen back as Logic plays.
2. Drag the **Drag to DAW** button into the MIDI region area to drop a MIDI
   region containing the current pattern, expanded to the configured bar
   count and fills.

---

## The interface

- **Top-left chip bar** — filter tags. Patterns matching any selected tag
  are in scope for Generate, density augmentation, and fill matching.
  Semantic similarity is on (e.g. `Rock` matches `Metal`).
- **Pattern #** — step through patterns matching the active filter.
- **Generate** — re-roll a composite pattern from the tagged sources. Each
  click produces a different mix.
- **Drag to DAW** — drag a MIDI file of the current pattern (with fills
  and bar count) onto a track.
- **Audio toggle** (top-right title bar) — turns the internal preview
  drum kit on/off. Default off (you'll hear nothing from WillyBeat
  itself). Turn on to hear simple synthesized drum sounds via WillyBeat's
  audio output as the DAW transport plays.
- **Collapse / expand toggle** (`-` / `+` top-right) — shrinks the editor
  into a mini-pattern view (read-only thumbnail in the lower-left + Edit
  Pattern + Import MIDI buttons). Useful when WillyBeat is parked in a
  track header.
- **Macro knobs** — Gate, Humanize, Swing, Feel, Density.
- **Pattern grid** — left-click an empty cell to place a max-velocity hit
  (120); left-click a filled cell to clear it. Click + drag vertically
  to tune the velocity (up = louder, down = quieter). Scroll over a cell
  with the wheel for finer adjustments. Right-click also clears. Hovering
  any cell shows its current velocity as a fading number on the cell.
- **Name field** — rename the active pattern. Saves on Enter or focus loss.
- **New Pattern / Open Folder** — start a blank pattern with the current
  filter tags, or open the presets folder in Finder.
- **Tags** (per-pattern) — chip bar bound to the active pattern's own
  genre tags. Add or remove chips and the change is written to the
  pattern's `.beat` file immediately.
- **Bars / Fill Start / Fill Mid / Fill End / Seed** — controls the
  drag-to-DAW export. Empty seed = fresh randomness each drag; numeric
  seed = reproducible output. Fill selection rerolls with the seed too,
  so each drag picks a different fill.

## Pattern library

Patterns live as plain-text `.beat` files in
`~/Library/Application Support/WillyBeat/Presets/`. Edit them in any text
editor — the format is self-describing:

```
# WillyBeat Preset
name:    Boom Bap Backbeat
genres:  Hip-Hop, Boom Bap
type:    Regular
density: 0.225

kick    h.......h.......
snare   ....h.......h...
hihat_c m.m.m.m.m.m.m.m.
...
```

Drop new files into the folder; reload the plugin to pick them up.
