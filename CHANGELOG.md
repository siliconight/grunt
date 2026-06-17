# Changelog

All notable changes to grunt are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/); versions follow
[Semantic Versioning](https://semver.org/).

## [0.21.10] - 2026-06-16

### Fixed (GUI window/taskbar icon now shows the grunt logo)
- The GUI had icon-loading code but the orc logo never appeared, for two
  reasons: the release zip never shipped the `assets/` folder (so
  grunt_icon64.png wasn't present), and the icon was loaded from a bare
  CWD-relative path instead of exe-relative. Now `assets/` is packaged in the
  Windows zip and the icon resolves exe-relative (same as model/registry paths),
  so the window and taskbar show the grunt logo regardless of launch directory.

## [0.21.9] - 2026-06-16

### Fixed (THE "Unable to find voice" root cause — Windows cmd.exe quoting)
- grunt's `shell_quote()` wrapped piper arguments in POSIX single quotes, but
  `std::system()` on Windows runs through **cmd.exe, which does not treat single
  quotes as quoting** — it passes them through literally. So piper received
  `-m 'en_US-ljspeech-high'` (quotes and all) and `--data-dir '<path>'`, couldn't
  match a voice literally named with the quotes, and failed with "Unable to find
  voice" — even though piper, the model, the `.onnx.json`, and the exact same
  command (run from a real shell, which strips quotes) all worked. This is why
  every prior fix (CLI args, data-dir, auto-detect) was individually correct yet
  the bake still failed: the final command was right but got mangled by cmd.exe.
  `shell_quote()` now uses double quotes on Windows (honored by cmd.exe) and
  POSIX single quotes elsewhere. fetch-voice's downloader was unaffected (it uses
  `powershell -Command`, which does honor single quotes).

## [0.21.8] - 2026-06-16

### Added (three vetted public-domain voices — more character range)
- **Cori** (UK English female, ~24h LibriVox, from scratch), **Kristin** (US
  English female, ~11.5h LibriVox, from scratch), **John** (US English male,
  ~12.5h LibriVox — note: finetuned from Kristin, clean chain documented). All
  from Bryce Beattie (same trusted creator as LJ/Norman), public-domain
  LibriVox audio, creator-dedicated public domain. All have direct download URLs
  so `fetch-voice` handles them. Gives the character system +2 female and +1
  male base timbres beyond pitch-shifting two voices.

### Excluded (documented in registry `_excluded_models`, NOT shippable)
- **Jenny (Dioco)** and **Clean 100** are deliberately excluded: their training
  datasets are CC-BY / CC-BY-4.0 (attribution required), NOT public domain. The
  creator's friendly grant cannot override the underlying dataset's CC-BY
  obligation. They're recorded with reasons so they aren't re-added by mistake.
- ManyVoice (clean, but multi-speaker — needs speaker addressing) and "Bryce"
  (clean license, but a named person's cloned voice — publicity-rights
  consideration) are noted and deferred.

### Safety
- All claims are good-faith reads, not legal advice. Defense in depth: excluded
  voices aren't in the live registry (can't be referenced by id), and the
  ShipGate still blocks any clip at bake time whose provenance isn't
  commercial-use + redistributable.

## [0.21.7] - 2026-06-16

### Fixed (no more "'piper' is not recognized" — auto-detection everywhere)
- grunt previously defaulted to invoking a bare `piper` command, which usually
  doesn't exist (upstream ships only a Python package now). So `grunt generate`
  and the GUI's Generate button failed with "'piper' is not recognized" unless
  the user manually set `GRUNT_PIPER_CMD` (only setup.bat did). Now a shared
  `detect_piper_cmd()` auto-probes `piper` -> `python -m piper` -> `py -m piper`
  and uses the first that runs — used by the generator, `doctor`, AND the GUI.
  `GRUNT_PIPER_CMD` still works as an override. Result: with `pip install
  piper-tts`, plain `grunt generate` and the GUI just work, no env var, no
  setup.bat required. When no piper is found at all, the error now says exactly
  how to fix it (`pip install piper-tts`) instead of a cryptic shell error.

## [0.21.6] - 2026-06-16

### Fixed (piper "Unable to find voice" when run by setup.bat / GUI)
- grunt told piper `--data-dir .` (the process working directory), but
  fetch-voice downloads the model **next to the binary**. When grunt ran from a
  different working dir (as setup.bat and the GUI do), piper looked in the wrong
  folder and failed with "Unable to find voice" — even though the model was
  present. (A manual run worked only if you'd cd'd into the model's folder
  first.) The generator now resolves the model exe-relative (same as
  fetch-voice) and passes that absolute directory as `--data-dir`, so it works
  regardless of where grunt is invoked from.

## [0.21.5] - 2026-06-16

### Added (phrase tier — limited-domain longer units)
- The planner now prefers whole **baked phrases** over word-by-word assembly.
  Walking the input left to right, it matches the LONGEST run of tokens the bank
  has as a single phrase unit (keyed by the lowercased space-joined words), so a
  covered phrase becomes ONE unit with zero internal joins — the highest-quality
  path for a fixed-vocabulary bark tool. Falls back to the existing
  word -> syllable -> phoneme -> grunt chain for anything not covered.
- Applies the core finding of limited-domain concatenative TTS (longer units =
  fewer joins = fewer artifacts) without any new dependency: it's pure planner
  logic over banks you already generate. New `UnitType::Phrase`; Engine synth
  uses the bank-aware planner. Verified: "open the gate" (baked) = 1 unit;
  "open the gate now" = 2 (phrase + word); uncovered text = unchanged per-word.

### Note
- Bake phrases by giving `generate` a CSV row whose text is multi-word (e.g.
  `holdline,hold the line`) — the unit is keyed by the spoken text, which the
  phrase tier matches. A future refinement could re-expand a missing phrase into
  its individual word slots; today a missing phrase degrades to the first word's
  chain on that slot.

## [0.21.4] - 2026-06-16

### Changed (licensing provenance hardened)
- The LJ Speech voice's public-domain claim is now traced to its **primary
  source** (keithito.com/LJ-Speech-Dataset) rather than just the Piper model
  card. Registry gains `dataset_source_url` and a structured
  `dataset_provenance` attestation (texts 1884-1964 + 2016-17 LibriVox audio,
  public domain, no use restrictions). Strengthens the zero-licensing-exposure
  backbone the ship gate enforces. Still a good-faith read, not legal advice.

## [0.21.3] - 2026-06-16

### Changed
- Renamed the character preset `ganger` -> `gangster` (correct spelling). Use
  `--character gangster` / "Gangster (gruff PS1 tough-guy)" in the GUI.

## [0.21.2] - 2026-06-16

### Added
- New character preset **gangster** ("Gangster (gruff PS1 tough-guy)"): Norman base
  voice pitched -5 semitones (low & menacing), formant shifted -0.15 (wider
  chest), rasp on (chain-smoker gravel), light sub-octave layer (menace weight),
  +2 dB, through the clean PS1 chain (bit-crushed, no radio). For a tough-guy /
  enforcer read.
- Note: grunt supplies the gruff/low/gravel texture, not a regional accent — the
  phonemes are standard English from the Norman voice. rasp/formant/sub DSP are
  still ear-unverified; treat the recipe as a tunable starting point.

## [0.21.1] - 2026-06-16

### Fixed (modern piper rejected the model: "Unable to find voice")
- After the v0.21.0 migration, piper installed and ran but generation failed
  with `ValueError: Unable to find voice: 'en_US-ljspeech-high.onnx'`. The
  modern piper1-gpl CLI takes a voice *name* resolved from a data dir, not a raw
  `.onnx` path, and reads text as a positional arg — a different convention from
  the old exe. grunt was still passing the old `--model <path> --output_file`
  with text piped on stdin.
- The generator now invokes the modern form:
  `python -m piper -m <name> --data-dir <dir> -f <out> -- <text>`, splitting the
  registry's model_file into its directory (`--data-dir`) and base name with
  `.onnx` stripped (`-m`). fetch-voice already downloads the matching
  `<name>.onnx` + `<name>.onnx.json` pair into that dir.

## [0.21.0] - 2026-06-16

### Changed (migrate to modern Python Piper — the old exe crashes)
- ROOT CAUSE of both the local "piper.exe has stopped working" crash AND the
  empty starter bank: the pinned Piper `2023.11.14-2` is a known-crashy build
  that dies on launch with a ucrtbase.dll error on current Windows (rhasspy/piper
  issues #274, #681). It crashed on the user's machine and silently failed the
  CI bake — so the shipped zip had no starter bank. The old rhasspy/piper repo
  is abandoned (latest tag is still that broken build); development moved to
  OHF-Voice/piper1-gpl, distributed as a Python package.
- grunt now uses **modern Piper via `pip install piper-tts`**, invoked as
  `python -m piper`. The Piper command is configurable through a new
  `GRUNT_PIPER_CMD` env var, so the same generator works with the modern Python
  CLI, a standalone `piper.exe`, or a bundled binary — grunt shells out, never
  links (engine is GPLv3; fine as a build-time tool).
- `grunt doctor`, the GUI's Piper detection, `setup.bat`, and the release CI now
  all probe for `piper` / `python -m piper` / `py -m piper` and set
  `GRUNT_PIPER_CMD` to whatever they find. `setup.bat` installs piper via pip
  (errors clearly if Python 3.9+ is missing) instead of downloading the crashy
  exe. SETUP.md updated.

### Note
- The AVG "suspicious file" popup the user saw is a separate, harmless
  unsigned-binary reputation scan — not the cause of the crash, and not fixed
  here (code signing is still deferred, needs a paid cert).

## [0.20.1] - 2026-06-16

### Fixed (release workflow broke during packaging)
- The Windows package step ran in bash, but a line I added tried to inline a
  PowerShell command containing `$p`/`` `r`n `` to convert setup.bat to CRLF.
  Bash expanded `$p`/`$pkg` and interpreted the backticks as command
  substitution, mangling the command and failing the job (the binaries built
  fine; only packaging broke). Removed it entirely — `.gitattributes`
  (`*.bat eol=crlf`) already guarantees setup.bat is checked out with CRLF, so
  the plain `cp` preserves it. Verified git applies `eol: crlf` to setup.bat.
- Hardened the starter-bank bake step: it's now `continue-on-error` with
  `set +e` and guarded downloads, so a Piper/model hiccup can never abort the
  release — the package just ships the demo bank instead.

### Note
- The failed run was the v0.19.0 tag (immutable). This fix ships in v0.20.1;
  push that tag for a clean Windows package build.

## [0.20.0] - 2026-06-16

### Added (easier intelligible VO: fetch-voice + a bundled talking bank)
- `grunt fetch-voice --model <id>`: downloads a registered voice model's files
  to where grunt looks for them, removing the manual "find the .onnx on a
  website" step. Models with a verified direct URL (LJ Speech) download
  automatically; models without one (Norman) print exact manual instructions
  rather than guessing a URL. Registry gains verified `download_url` fields.
- **Bundled "starter" word bank**: the release CI now downloads Piper + the
  public-domain LJ voice and pre-bakes a curated bark vocabulary
  (`examples/starter_barks.csv` — hello/halt/intruder/reloading/cover/medic/...)
  into a real, gate-clean word bank that ships in the Windows package. So a user
  hears **real English words with zero setup** — pick "starter" in the bank
  dropdown, choose a character, Play. (CI verifies it's gate-clean and has word
  units before shipping; if Piper/model fetch fails, the package ships the demo
  bank only rather than a broken bank.)
- The GUI bank dropdown defaults to "starter" when present, so first launch is
  words, not grunts.

### Why
- The remaining friction for intelligible VO was entirely the external Piper +
  model dependency. `fetch-voice` removes the model hunt; the pre-baked starter
  bank sidesteps Piper for first contact entirely — the audio is synthesized
  once on CI (which has Piper) and shipped, so the user consumes ready speech.

### Note — provenance
- The starter bank is LJ-Speech-generated audio (public domain), baked by CI,
  gate-clean. As always: good-faith license read, not legal advice; the ship
  gate enforces commercial + redistributable before any clip is baked.

## [0.19.0] - 2026-06-16

### Added (GUI: bank dropdown + in-app voice generation)
- **Voice bank dropdown** replaces the typed bank-path field. The GUI scans
  `voices/` for banks and lists them, labeling each `(words)` or
  `(grunts only)` so you can see at a glance which can actually speak. It
  defaults to a word-capable bank when one exists, and loads on selection.
- **Generate voices** section (collapsing): finds Piper automatically (PATH,
  then a bundled `.\piper\` next to the exe), lets you name a new bank and pick
  a voice model, and bakes a word bank from `examples/barks.csv` **in-app** —
  then auto-selects and loads it. No more dependence on the external
  `setup.bat`; you see success or the exact error right in the window.
- Generation logic extracted into a shared `generate_bank()` (BankGen) used by
  both the CLI `generate` command and the GUI, so there's one code path.

### Why
- The bank field exposed the wrong concept (file paths) and, more importantly,
  the only bank present was the grunt-only demo — so there were no word-voices
  to pick. Bringing generation into the GUI closes that gap directly and makes
  the grunt-only-vs-words distinction visible instead of a silent surprise.

### Note
- The Generate button needs Piper present (auto-detected) and a downloaded voice
  model; if Piper isn't found the section explains how to get it. GUI behavior
  is validated on Windows (can't run it in dev); the shared generate path and
  bank discovery are tested here.

## [0.18.3] - 2026-06-16

### Fixed (the actual cause of setup.bat closing instantly)
- `setup.bat` had **Unix LF line endings**, not Windows CRLF. cmd.exe cannot
  parse an LF-only `.bat` reliably — it bails before printing anything, so the
  window opens and closes with no output. This was the real cause behind the
  repeated "closed again, no success message"; the earlier content fixes
  (parens, caret) were valid but not the reason it died.
- Converted `setup.bat` to CRLF. Added `.gitattributes` (`*.bat text eol=crlf`)
  so git always checks it out with CRLF, and a CI step that re-normalizes the
  batch file to CRLF when building the Windows package — so this can't regress
  when the zip is assembled on Linux. Verified CRLF survives into the tarball.

## [0.18.2] - 2026-06-16

### Fixed (setup.bat parse error — closed instantly with no output)
- A literal `(.json)` in an echo *inside* an `if (...)` block prematurely
  closed the block. cmd.exe parses the whole file before running, so this was a
  fatal parse error: the window opened and closed instantly, before any step or
  pause — exactly the "closed again, no success message" symptom.
- Removed parentheses from all echoed text (cmd treats them as block
  delimiters), and collapsed a fragile caret line-continuation in an echo.
- Verified the if/else block structure is balanced. The window now reaches its
  pause and shows SUCCESS or the failed `[X]` step.

## [0.18.1] - 2026-06-16

### Fixed (setup.bat usability)
- The window no longer closes before you can read it — both the success and
  failure paths now `pause` and wait for a keypress.
- Every step reports `[ok]` / `[X]`, and the script ends with an unmistakable
  **SUCCESS** or **SETUP DID NOT FINISH** banner (the failed step is the one
  marked `[X]`).
- Added verification that catches silent failures the closed window used to
  hide: confirms `piper.exe` actually exists after extraction (re-detects both
  release layouts), that the model + its `.json` downloaded, that `generate`
  wrote a `units.json`, and that the output clip was written.
- Closing message explains the easy-to-miss gotcha: the bundled demo bank is
  grunt-only by design, so to hear words you must point the GUI's "advanced"
  bank field at the generated `voices\my_guards`.

## [0.18.0] - 2026-06-16

### Added (Phase 2 — syllable-unit generation)
- `generate --unit-type syllable` bakes one clip per row as a **syllable** unit
  keyed by the CSV key. Supply ARPAbet keys (e.g. `G EY T`,`gate`) and the
  planner's syllable fallback tier matches them, so a bank can assemble words it
  has no whole-word clip for — now worth doing because the Viterbi selector
  (v0.17.0) sequences those parts by join cost. Default stays `--unit-type word`.

### Note — honest scope
- This synthesizes each syllable directly (one short Piper utterance per
  syllable). It does NOT slice a whole spoken utterance into syllables via
  forced alignment — Piper emits no phoneme timing, and bolting on an aligner
  (e.g. MFA) is a real dependency not taken. Word units remain the crispest path
  for grunt's fixed bark vocabulary; syllable units are the flexibility option.

## [0.17.0] - 2026-06-16

### Changed (Phase 3 — Viterbi unit selection replaces greedy)
- The unit selector is now a **Viterbi lattice search**: instead of picking the
  locally-cheapest unit per slot independently, it finds the globally min-cost
  path through all slots, balancing per-unit **target cost** (fallback tier,
  grunt bias, emotion match) against per-boundary **join cost**. This is the
  standard concatenative unit-selection formulation and the biggest quality
  lever for how stitched output flows.
- **Join cost** (`sel::join_cost`) penalizes pitch discontinuity (~0.15/semitone
  via the units' `pitch_center_hz`), energy discontinuity, and immediate
  back-to-back repetition of the same clip — so the chosen sequence flows rather
  than lurching between mismatched units.
- Deterministic per seed (verified); one unit per slot as before; character and
  effort paths unchanged. Tests cover the join-cost behavior (pitch/energy/
  repetition ordering).

### Note
- Windowed anti-repetition (avoid reusing a clip within N slots, not just
  adjacent) is deferred: carrying that history in the Viterbi DP state explodes
  the lattice, and the immediate-repetition penalty covers the most jarring
  case. Revisit if repetition is audible on a real bank.
- Diphone units also deferred — they need a generate-side path that emits
  diphone metadata first, or they'd be dead code.

## [0.16.0] - 2026-06-16

### Added (one-click Windows talking setup)
- `setup.bat`: a Windows script that automates the whole generate path —
  downloads the Piper engine into `.\piper\`, downloads a public-domain voice
  model (LJ Speech), puts piper on PATH for the session, runs
  `grunt doctor --live`, generates a talking bank from `examples\barks.csv`, and
  plays a real spoken word. Re-runnable (skips existing downloads); documents how
  to swap in the male Norman voice.
- `setup.bat` and `examples/` are now bundled in the Windows release package;
  the "READ ME FIRST" note points at it for real spoken words. SETUP.md gains a
  one-click section.

### Fixed
- The Windows package previously omitted `examples/`, which `setup.bat` and the
  docs reference — now included.

### Note
- The script uses verifiable URLs (Piper's GitHub release; LJ Speech on the
  rhasspy/piper-voices HuggingFace repo). The male Norman voice stays a
  documented manual swap because its host's exact file URLs aren't
  machine-verifiable here. Can't run Windows/network in dev, so the script is
  logic-checked and ships for on-machine validation.

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
