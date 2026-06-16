# Changelog

All notable changes to grunt are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/); versions follow
[Semantic Versioning](https://semver.org/).

## [0.6.0] - 2026-06-16

### Added (efforts & onomatopoeia — non-lexical vocalizations)
- `--effort <id>` on `synth`: renders a named vocalization from
  `data/efforts.json` (pain_hit, pain_death, exertion, fear, gasp, laugh, yell,
  alert). Each maps to an ARPAbet pattern + intensity + emotion. Unknown id
  lists the menu with descriptions.
- `--onomatopoeia "aaargh"` on `synth`: voices a literal spelling as a sound
  rather than pronouncing it as a word. Repeated letters raise intensity and
  length (`"aaaargh"` > `"argh"`).
- Both compose with `--character`: the same effort through yelling_man / orc /
  robot gives a human scream / monster roar / robot glitch for free.
- `Engine::synth_vocalization`: renders from a `PhonemeSeq`, bypassing text
  normalization + syllable planning; intensity scales gain (up to +6 dB) and
  duration (up to +60%). Reuses the existing prosody/selection/render/FX path,
  so vocalizations work whether or not the bank has dedicated effort units
  (falls back to grunts otherwise).
- `Vocalization` module (`EffortLibrary`, `effort_to_phonemes`,
  `onomatopoeia_to_phonemes`) + tests.

### Why
- Closes a real gap: the voice-bank structure (TDD §8) always listed efforts,
  but nothing produced or selected them. "A guard yelling when shot" is exactly
  this category — squarely the "everything else" the north star targets.

## [0.5.0] - 2026-06-16

### Added (character presets — "type text, pick a character")
- `CharacterPreset` + `CharacterLibrary` loading `data/characters.json`.
- `--character <name>` on `synth`: applies a preset recipe (base voice + pitch
  + FX + emotion bias + gain). Explicit `--style`/`--emotion` still override.
  An unknown name lists all characters; a not-yet-DSP-ready character renders a
  reasonable approximation with an honest note.
- `Engine::Options` layers per-character pitch/gain onto every unit.
- The four "achievable now" characters work end-to-end: Grunt, Robot, Yelling
  Woman, Yelling Man (Wilhelm).

### Verified
- Registry now contains a real license-cleared base voice: `piper-en_US-ljspeech`
  (rhasspy/piper-voices, MIT model repo; trained from scratch on the
  public-domain LJ Speech dataset — clean transitive chain). Female-based
  presets point at it. The remaining placeholder is kept only as a template for
  adding a male/CC0 base. Not legal advice — confirm before commercial release.

## [0.4.2] - 2026-06-16

### Added
- Character presets: `data/characters.json` defines named, out-of-the-box
  voices for TTS mode (Grunt, Deep Big, Woman/raspy, Orc, Robot, Demon, Yelling
  Woman, Yelling Man/Wilhelm). A preset is a recipe over the pipeline (base
  voice + pitch + FX + prosody, plus planned formant/sub/rasp), so most layer
  over a few shared base voices. ROADMAP gains a Character Presets section with
  an honest split: four are achievable with existing FX/pitch (Grunt, Robot,
  Yelling Woman, Yelling Man); four await Phase 3 renderer DSP (Deep Big, Woman
  raspy, Orc, Demon). Loader/`--character` flag/GUI dropdown are planned.

## [0.4.1] - 2026-06-16

### Changed
- Repositioned around grunt's actual purpose: an accessibility tool that lets
  devs with no studio/mic/VO budget type text and get legally-yours, game-ready
  Vorbis OGG voice clips. README intro now leads with that promise and the
  "fill the blanks, don't chase realism" philosophy; `generate` is framed as the
  headline path.
- ROADMAP gains a "North star" section — four checks every future feature is
  measured against (lowers the barrier? stays airtight? serves decent-and-
  characterful over realistic? stays offline/at-rest/Godot-friendly?).

## [0.4.0] - 2026-06-16

### Added (generative voice banks — no recording required)
- `generate` command: builds a voice bank's units from a license-cleared open
  TTS generator. Reads a `key,text` units CSV, synthesizes each unit, and writes
  `units.json` with provenance auto-stamped from the model's license.
- `VoiceModelRegistry` (`data/voice_models.json`): an allowlist of license-
  cleared generator models (CC0 / permissive). `generate` only uses models from
  it, turning per-model license diligence into data the ship gate enforces.
- Generator backends: `piper` (shells out to the Piper binary — a build-time
  tool grunt never links) and `stub` (deterministic test tones, always
  gate-blocked as synth-derived so they can't leak into a real bank).
- Provenance for generated clips records generator + model + license; clips
  from a commercial+redistributable model pass the gate, others are blocked.
- Tests for registry load, model lookup, and shippable-vs-blocked provenance.

### Notes
- The real Piper invocation is wired but verified on a machine with Piper
  installed; the offline test path uses the stub. MBROLA was evaluated and
  rejected — its voices are non-commercial, so they can never ship.

## [0.3.0] - 2026-06-16

### Added (TDD Phase 1 — Phoneme Debug Build)
- `PhonemeMapper`: converts words to ARPAbet phonemes via a CMUdict-style
  dictionary lookup with a rule-based grapheme→phoneme fallback for
  out-of-dictionary words (digraphs, soft c/g, silent final e, etc.).
- `phonemes` command now emits real ARPAbet (replacing the Phase 0 stub),
  with `--dict <file>` to load a CMUdict-format dictionary and an unknown-word
  report listing which words used the rule fallback.
- `PhonemeSeq` / `WordPhonemes` types carry per-word phonemes + resolution
  source (dictionary vs. rule fallback).
- `data/sample.dict`: tiny CMUdict-format sample for trying the dictionary path.
- Tests for dictionary load, fallback, digraph handling, and dict-overrides-fallback.

## [0.2.4] - 2026-06-16

### Fixed
- Windows test crash (SEGFAULT in CI): tests wrote to a hardcoded `/tmp/` path,
  which doesn't exist on Windows. `write_wav` failed, then `read_wav` ran on a
  nonexistent file and crashed. Tests now use `std::filesystem::temp_directory_path()`.
- `read_wav` hardened against malformed/truncated WAVs: the claimed `data` chunk
  length is now clamped to the bytes actually present (a too-large length would
  read past the buffer and crash), and the chunk scanner guards against
  zero-advance loops and short `fmt ` chunks. Protects the renderer from a bad
  bank clip, not just the tests.
- Added a regression test asserting `read_wav` on a missing file fails cleanly.

## [0.2.3] - 2026-06-16

### Changed
- Workflow restructured to match gool's pattern: `ci.yml` (build + ci jobs,
  every push/PR) and a separate tag-triggered `release.yml`. Workflow display
  name is now "grunt CI" / "grunt Release"; each run is labeled with the version.
- `build` runs a Linux + Windows matrix; `ci` runs the verification suite
  (tests, smoke bake, Vorbis codec assertion, determinism).
- `release.yml` (tag pushes only) verifies, then publishes the source tarball
  to a GitHub Release via `softprops/action-gh-release` (`contents: write`).
- `scripts/package.sh` takes an optional output dir so CI can stage the tarball.

## [0.2.2] - 2026-06-16

### Added
- Logo (`assets/grunt.png`) — README banner and GUI window icon.
- CI jobs are labeled with the version being built (a `version` job resolves the
  tag or describe-sha and feeds it to the build jobs' names + env).

### Fixed
- MSVC build: replaced `M_PI` (undefined on MSVC without `_USE_MATH_DEFINES`)
  with a portable `voc::kPi` constant. Caught by the Windows CI job.
- MSVC `std::fopen` C4996 deprecation warning silenced via
  `_CRT_SECURE_NO_WARNINGS` (set in CMake for MSVC builds).

## [0.2.1] - 2026-06-16

### Added
- `ROADMAP.md`: feature progress tracked against the TDD phases and acceptance
  criteria (what's done in v0.1.0/v0.2.0 vs. remaining work per phase).

## [0.2.0] - 2026-06-16

### Added
- `Engine` class: a single reusable synthesis facade. The CLI and GUI both call
  it, so there is one synthesis path that can't drift. `synth_line` orchestration
  moved out of `main.cpp`.
- `grunt_gui`: optional Dear ImGui desktop front end. Type a line, pick voice /
  emotion / style, **Play** to preview by ear (real-time via miniaudio, no temp
  files), **Export** to save an OGG/Vorbis or WAV clip. Off by default; enable
  with `-DGRUNT_BUILD_GUI=ON` after vendoring deps (see `third_party/README.md`).
- README: explicit "no build-time coupling" section clarifying grunt is a
  standalone at-rest authoring tool, plus a GUI section.

### Changed
- `batch` and `synth` now render through `Engine` instead of the inline helper.

### Notes
- GUI glue is syntax-checked against the Engine/AudioOut API in CI's scope, but
  the full Dear ImGui / GLFW / miniaudio binding is validated only when built
  against the vendored libraries.

## [0.1.0] - 2026-06-16

First scaffold — Phase 0 (grunt-vocalizer mode) of the TDD. A build-time CLI
that turns short game text into named PS1-style vocal clips from owned voice
banks and bakes a reproducible sound bank for Godot/gool to import by name.

### Added
- Full Phase 0 pipeline: `TextNormalizer`, `SyllablePlanner` (grunt-mode
  open-syllable splitter), `ProsodyPlanner` (data-driven emotion contours),
  `UnitDatabase`, `UnitSelector` (target + emotion + repetition cost),
  `AudioRenderer` (zero-crossing align, crossfade, pitch/time, limiter),
  `RetroFxChain` (5 PS1 presets: clean / radio / monster / robot / muffled).
- CLI commands: `synth`, `batch`, `verify`, `phonemes`.
- **OGG/Vorbis output by default** via libvorbisenc (NOT Opus); WAV available
  for debugging and as the no-libvorbis fallback. `--format` / `--quality`
  flags. Output extension forced to match the chosen format.
- `batch`: bakes a folder of named clips + `bank.json` manifest from a CSV.
  The clip name is the contract gool resolves against (`has_sound`).
- Ship gate (`verify`, also run inside `batch`): refuses to package a bank if
  any clip is synth-tool-derived, non-commercial, or an unlicensed sample-pack
  clip. Makes "no third-party licensing exposure" an enforced invariant.
- Deterministic renders via `--seed` (byte-identical output) for reproducible
  asset builds; omit for slight per-render variation.
- Dependency-free `Json` reader and minimal `Wav` read/write.
- CMake build with auto-detection of libvorbis; falls back to WAV-only build
  with a clear warning when libvorbis is absent.
- Unit tests (`grunt_tests`) covering normalizer, syllabifier, prosody, JSON,
  WAV round-trip, format dispatch, and ship-gate logic.
- Demo `heavy_brother` voice bank (synthetic placeholder tones) so the
  pipeline runs out of the box.
- GitHub Actions CI building **with libvorbis** and running tests — closes the
  gap that the OGG encoder path is only stub-compiled locally.

### Notes
- The OGG encoder path is stub-compiled in the scaffold environment (no
  libvorbis dev headers there). CI builds it against the real library; verify
  actual `.ogg` output on a libvorbis-enabled build before relying on it.
- Demo bank clips are placeholders — replace with owned recordings before
  shipping. The ship gate enforces clean provenance regardless.

[0.1.0]: https://github.com/siliconight/grunt/releases/tag/v0.1.0
