# grunt

A build-time CLI that turns short game text into named PS1-style vocal clips
from **owned** voice banks, and bakes a reproducible sound bank that
Godot/gool imports and plays at runtime by name.

This is **Phase 0** (grunt-vocalizer mode): text drives timing and emotion;
output is stylized syllables and grunts stitched from recorded units, run
through a PS1-style FX chain. No neural TTS, no cloud, no runtime synthesis.

See `vocalizer_tdd.md` (the design doc) for the full architecture and roadmap.

## Build

Requires a C++20 compiler. OGG/Vorbis output (the default) needs
**libvorbis + libvorbisenc + libogg**. With CMake:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build        # run unit tests
```

CMake auto-detects libvorbis. If it's missing, grunt still builds but emits
**WAV only** (and says so). To get OGG output:

- Windows: `vcpkg install libvorbis libogg`, then configure with the vcpkg toolchain.
- Debian/Ubuntu: `sudo apt install libvorbis-dev libogg-dev`.
- macOS: `brew install libvorbis libogg`.

Direct build without CMake (WAV-only, no OGG):

```
g++ -std=c++20 -O2 -Iinclude src/*.cpp -o grunt
```

With OGG, add `-DGRUNT_HAVE_VORBIS` and link `-lvorbisenc -lvorbis -logg`.

## Commands

```
grunt verify   --voice voices/heavy_brother
grunt phonemes --text "Open the gate!"
grunt synth    --text "Open the gate!" --voice voices/heavy_brother \
               --emotion urgent --style clean_ps1 --seed 42 --out open_gate.ogg
grunt batch    --csv scripts/sample_lines.csv --voice voices/heavy_brother \
               --out-dir build/vo --seed 1234
```

- `--emotion` : `neutral | urgent | angry`
- `--style`   : `clean_ps1 | radio_ps1 | monster_ps1 | robot_ps1 | muffled_mask`
- `--format`  : `ogg | wav`. **Default: ogg (Vorbis).** The output extension is
  forced to match the format. WAV is for debugging / no-libvorbis builds.
- `--quality` : libvorbis VBR quality, `-0.1`..`1.0` (default `0.4`).
- `--seed`    : fixed seed -> byte-identical output (reproducible asset builds).
  Omit for slight per-render variation.

Note: OGG here means the OGG **container with a Vorbis stream** — the codec
Godot's `AudioStreamOggVorbis` decodes. Not Opus.

## Output: a sound bank gool imports by name

`batch` produces a folder of named clips plus a `bank.json` manifest:

```
build/vo/
  mission01_open_gate.ogg
  goblin_taunt_01.ogg
  ...
  bank.json
```

The **clip name is the contract**. The game references clips by name through
gool (`Gool.has_sound("goblin_taunt_01")` then `create_emitter`). grunt's job
ends at producing named files; which event triggers which clip — and where in
3D space — is gool/Godot wiring. `bank.json` carries build-time metadata
(text, emotion, seed, peak) the runtime doesn't need.

## The ship gate (clean licensing, enforced)

Every clip carries a `provenance` block in `units.json`. `verify` — and
`batch`, which runs it first — refuses to ship a bank if any clip is:

- `synth_tool_derived: true` (eSpeak/MBROLA placeholder), or
- `commercial_use: false`, or
- a `sample_pack` without a recognized commercial/redistributable license.

Prototype freely with placeholder clips; the gate guarantees they can never
reach a baked bank. This makes "no third-party licensing exposure" an
invariant the tool enforces, not a hope.

## Voice bank layout

```
voices/heavy_brother/
  voice.json                 # manifest (id, sample rate, base pitch, style)
  units/
    grunts/  syllables/  words/
  metadata/
    units.json               # per-clip metadata + provenance
```

The included `heavy_brother` bank uses **synthetic placeholder tones** so the
pipeline is runnable out of the box — replace them with real recordings (and
keep `synth_tool_derived: false` only when they are genuinely owned).

## Status / next

Phase 0 is complete and tested: normalizer, grunt-mode syllable planner,
data-driven prosody, unit selection with repetition penalty, concatenative
renderer (zero-crossing align + crossfade + pitch/time + limiter), PS1 FX
presets, batch bank bake, deterministic seeds, and the provenance ship gate.

Next per the TDD: Phase 1 (real CMUdict phoneme mapper + unknown-word
fallback), Phase 2 (syllable DB + coverage reporting), Phase 3 (diphones +
Viterbi unit selection).
