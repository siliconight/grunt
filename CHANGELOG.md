# Changelog

All notable changes to grunt are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/); versions follow
[Semantic Versioning](https://semver.org/).

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
