# Changelog

All notable changes to grunt are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/); versions follow
[Semantic Versioning](https://semver.org/).

## [0.15.0] - 2026-06-16

### Added (grunt doctor — foolproof first-bank setup)
- `grunt doctor`: checks the generate path and, for anything missing, prints the
  exact fix. Verifies the voice-model registry, whether `piper` is on PATH,
  whether a registered model file is downloaded, and that grunt can create a
  bank directory. Exit code = number of failed checks (0 = ready); on success
  it prints the exact first-bank commands.
- `grunt doctor --live`: additionally generates one test word and runs it
  through the ship gate, so you know the whole chain works before investing in a
  real bank.
- SETUP.md's generate walkthrough now leads with `doctor`, then a copy-paste
  "make your first talking bank" sequence (generate → synth through a character
  → point the GUI at it).
- CI smoke check: `doctor` runs and finds the registry.

### Why
- The generate path has the most moving parts (external Piper binary, a
  downloaded model, file paths, the gate) and the least verification, since it
  can't run in CI/dev. `doctor` turns an opaque first attempt into a checklist,
  making the one run that actually produces talking output likely to succeed.

## [0.14.0] - 2026-06-16

### Changed (GUI: character dropdown + export fix)
- The GUI now leads with a **Character dropdown** (the eight presets from
  data/characters.json) instead of a typed voice-bank path. Selecting a
  character applies its full recipe (pitch / FX / emotion / formant / sub /
  rasp) via the same Engine::Options the CLI uses — so the GUI and CLI match.
- The voice-bank folder field moved under an **"advanced"** toggle (most users
  never need it); emotion/style overrides live there too, for the "(none)"
  character.
- The bundled bank auto-loads on first frame, so a new user sees a working
  state without hunting for the Load button.

### Fixed
- **Export failure** (`cannot open out/line.ogg` in the v0.12 screenshot): the
  GUI now creates the export path's parent directory before writing, and the
  default export path is a plain `line.ogg`. Status shows the absolute path on
  success.

### Note
- GUI can't be built/run in the dev sandbox; this was type-checked against the
  real engine headers (clean) and is CI-built for the Windows package. The
  window/interaction is validated on Windows.

## [0.13.0] - 2026-06-16

### Changed (intelligibility — words, not just sounds)
- **Word-first planning.** `plan_phonemic` now requests a whole-word unit first
  (keyed by the lowercased word), with a fallback chain of syllable → phonemes →
  grunt. When a bank has the word, you hear the word; otherwise it degrades
  gracefully. This is the fix for "sounds like sounds, not words": the engine
  was correctly planning, but no bank had intelligible units to select, so
  everything fell to grunts.
- **`generate` bakes WORD units**, keyed by the spoken text (was: syllable units
  keyed by the CSV id, which the planner never matched). A bank generated from a
  `key,text` CSV now contains real word clips the planner selects directly.
- `coverage` label updated to "word/syllable match"; reports now reflect
  word-level hits. Verified: a generated word bank takes "my goodness" from 100%
  grunt fallback to 100% matched.
- `examples/barks.csv` reworked to word-oriented rows.

### Note
- Full multi-syllable assembly of an *absent* word (stitching a word the bank
  lacks from its syllable parts) is deferred to the Phase 3 Viterbi selector;
  today the intelligibility path is word units, which suits grunt's fixed bark
  vocabulary. Real spoken output still requires generating a bank with Piper on
  a machine that has it — the stub proves the selection chain only.

## [0.12.0] - 2026-06-16

### Added (download-and-run path toward a double-click GUI)
- Executable-relative resource resolution (`ResourcePath`): `data/` and
  `voices/` are now found relative to the binary (then CWD, then repo/install
  layouts), so grunt works when launched from any working directory — the
  prerequisite for a double-clicked app. Verified by running from a foreign CWD
  with resources beside the exe.
- Release CI now builds and publishes a **Windows GUI package**
  (`grunt-vX.Y.Z-windows.zip`): `grunt_gui.exe` + CLI + runtime DLLs + bundled
  `data/` and demo bank, plus a "READ ME FIRST" launch note. A non-builder can
  download, unzip, and double-click `grunt_gui.exe`. (Deps vendored in CI;
  GLFW/libvorbis via vcpkg.)
- `SETUP.md` leads with the click-and-go Windows path, honest about the
  one-time unsigned-app SmartScreen notice (code signing needs a paid cert).

### Fixed
- quickstart summary no longer prints a doubled `.//path`.

### Note
- The GUI itself can't be built or run in the dev sandbox (no display, deps not
  present), so the packaging workflow and path resolution are built
  correct-by-construction and CI-built; the "window opens and plays sound"
  confirmation happens on a real Windows machine.

## [0.11.0] - 2026-06-16

### Added (Phase 2 — phoneme-backed syllable rendering)
- `SyllablePlanner::plan_phonemic`: syllabifies each word from its ARPAbet
  phonemes (split at vowel nuclei) instead of the Phase 0 spelling splitter.
  The Engine now uses this path, loading a phoneme dictionary if present
  (`data/cmudict.dict` / `data/sample.dict`), else rule-based G2P.
- Scored fallback chain per unit: syllable key → constituent phonemes → grunt.
  `UnitSelector` walks the chain and picks the first matching tier, with a
  tier-cost so closer-to-intended units win. `UnitDatabase::match_key` does
  case-insensitive exact-key matching (empty key = the grunt tier).
- `coverage` command: runs a script/line through the planner against a bank and
  reports syllable-level / phoneme-level / grunt-fallback rates plus the top
  missing syllable units — so you can see where a bank is thin before shipping.

### Notes
- Determinism preserved; all existing paths (synth, characters, efforts,
  quickstart) still render. Banks key syllable units by ARPAbet string (e.g.
  "G EY T"); a generate-side path that emits real syllable units is the
  remaining Phase 2 work (today most banks are grunt-only, which `coverage`
  now makes visible).

## [0.10.0] - 2026-06-16

### Added (frictionless first run)
- `grunt quickstart`: zero-config first-run command. Renders five demo clips
  (plain line, two characters, two efforts including a death scream) from the
  bundled voice bank into `./grunt_quickstart/` — no Piper, no model downloads,
  no setup. First sound in one command. Deterministic.
- `grunt --version` / `-V`.
- Friendlier no-args landing: leads with "new here? run: grunt quickstart".
- `examples/barks.csv`: sample units CSV so `generate` has something to point at.
- `SETUP.md`: explicit, copy-paste setup — fastest-path quickstart, build, and a
  step-by-step generate-path walkthrough (install Piper, exactly which voice
  files to download and where, generate, verify). Turns the previously-implicit
  scariest step (Piper + model) into a checklist.
- CI smoke check: `quickstart` must produce >=5 clips from the bundled bank, so
  the first-run path can't silently break.

### Why
- The first-time experience had ~7 steps and three external dependencies before
  any sound — at odds with grunt's accessibility mission. The zero-config path
  already worked (bundled bank + synth) but wasn't surfaced; quickstart makes it
  the obvious first move.

## [0.9.0] - 2026-06-16

### Added (male base voice — roster now spans two timbres)
- Second license-cleared base voice in the registry: `piper-en_US-norman` — a
  US English male voice, trained from scratch on ~15.5h of public-domain
  LibriVox recordings, by the same creator (Bryce Beattie) as the LJ Speech
  voice. Public domain, commercial + redistributable. Provenance is the
  creator's personal-site attestation rather than an institutional model card —
  credible (same trusted source) and recorded as such in the registry. Not legal
  advice; confirm before commercial release. Alternatives noted: John
  (finetuned from Kristin), Bryce (creator's own voice).
- Male-coded characters (grunt, deep_big, orc, demon, yelling_man, robot) now
  derive from Norman instead of being pitched-down female; woman_raspy and
  yelling_woman stay on LJ Speech. The eight presets now span two real timbres
  rather than one.
- `base_voice` drives the `generate` path (which voice synthesizes a character's
  bank). Download `norman.onnx` + `.onnx.json` from brycebeattie.com/files/tts.

## [0.8.1] - 2026-06-16

### Changed
- ROADMAP: added a "Quality refinements" section capturing two designed-but-
  deferred upgrades — a biquad formant filter bank (F1/F2/F3, vocal-tract-based,
  refining the current resample formant shift) and principled per-emotion
  prosody profiles (data-driven). Both marked as polish, explicitly not queued
  ahead of breadth work or ahead of verifying the current DSP by ear. Plus a
  positioning note that the synthetic-voice ethics stance is already embodied,
  not new work.

## [0.8.0] - 2026-06-16

### Added (PSOLA — clean repitch/retime, completes the Phase 3 DSP thread)
- `psola_timestretch`: phase-coherent, pitch-synchronous time-stretching —
  changes duration while preserving pitch by repeating/skipping grains at the
  detected pitch period. This is the robust core operation.
- `psola`: repitch+retime. Pitch-shift is done as resample (moves pitch) then
  PSOLA time-stretch to restore the target duration — the standard robust
  combination that avoids the per-grain phase-alignment failure mode of naive
  TD-PSOLA pitch-shifting. Pairs with the separate formant stage.
- `estimate_period`: normalized-autocorrelation pitch detection (70–400 Hz)
  with a confidence threshold; returns 0 on unvoiced/aperiodic audio.
- Renderer now uses PSOLA for pitch/duration, with automatic fallback to the
  resample+refit path when a clip isn't reliably periodic — never worse than
  before, better when the audio is voiced.
- Tests: period detection accuracy, aperiodic rejection (fallback path),
  time-stretch length, pitch-shift period change, determinism preserved.

### Note
- Real DSP, best judged by ear; the sandbox verifies the math invariants and
  that all eight characters still render deterministically. The pitch-shift
  combination (resample+PSOLA) was chosen deliberately over pure TD-PSOLA
  pitch-shift after the latter produced incoherent output in testing — a case
  where "tests pass" caught a subtly wrong algorithm.

## [0.7.0] - 2026-06-16

### Added (Phase 3 character DSP — formant / sub-octave / rasp)
- Formant shifting (`voc::dsp::formant_shift`): moves the spectral envelope
  independently of pitch via resample-and-restore, so lowering a voice sounds
  like a bigger throat rather than a slowed tape. Decoupled from the pitch
  resample.
- Sub-octave layering (`add_sub_octave`): mixes in an octave-down copy for
  chest/size.
- Rasp (`apply_rasp`): bounded soft-clip + odd-harmonic grit for raspy and
  monstrous timbres.
- `ProsodyUnit` and `Engine::Options` carry `formant_shift` / `sub_layer` /
  `rasp`, wired from character presets through to the renderer.
- All four DSP-dependent characters (Deep Big, Woman/raspy, Orc, Demon) are now
  fully realized and marked `ready: true` — no more approximation notes.
- DSP invariant tests: formant/sub preserve length (pitch intact), rasp stays
  bounded in [-1,1], zero-amount is a no-op, empty input is safe.

### Note
- This is real DSP and best judged by ear; the sandbox verifies the math
  invariants and that all characters render. Confirm the timbres on Windows.
- PSOLA (clean repitch/retime across a small unit set) is the remaining Phase 3
  item.

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
