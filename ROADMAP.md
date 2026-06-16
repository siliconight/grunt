# grunt Roadmap

Progress against the TDD (`vocalizer_tdd.md`). This tracks *remaining* work by
phase; shipped detail lives in `CHANGELOG.md`. Status keys:
**[x] done · [~] partial · [ ] not started**

Current version: **v0.4.0**

---

## North star — what every roadmap decision is checked against

grunt's reason to exist: **a dev with no studio, no mic, and no VO budget can
type text and get legally-yours, game-ready voice clips.** Realism is not the
goal — legibility and feel are. We fill the "everything else" (barks, grunts,
reactions, efforts) so real voice acting can be reserved for key story lines.

Before adding or prioritizing a feature, check it against these:

1. **Does it lower the barrier, or raise it?** Anything that requires a studio,
   a mic, paid assets, or audio expertise to benefit from is suspect. The
   generate-from-text path is the main path; recording is the optional one.
2. **Is the output still legally airtight?** Every clip must stay shippable by
   construction (registry + ship gate). A feature that introduces unclear
   rights doesn't ship until the gate can enforce it.
3. **Does it serve "decent and characterful," not "realistic"?** Effort spent
   chasing human-fidelity is usually better spent on legibility, variety, and
   the retro identity that makes synthetic output feel intentional.
4. **Does it stay offline, at-rest, and Godot-friendly?** No cloud, no runtime
   synthesis, output is Vorbis OGG files that import like any other asset.

A feature can be technically impressive and still be wrong for grunt if it
fails these. When in doubt, optimize for the soloist filling blanks in a fun
game — not the studio chasing fidelity.

---

## Phase 0 — Grunt Vocalizer  (TDD §18 Phase 0)  — [x] DONE (v0.1.0)

- [x] Text normalizer — tokenize, punctuation, emotion hint, CAPS emphasis (§6.1)
- [x] Grunt-mode syllable planner (§6.3)
- [x] Data-driven prosody: emotion contours, declination, stress (§6.4)
- [x] Voice manifest + unit metadata loader (§7–9)
- [x] Unit selector: target + emotion + repetition cost (§6.5, greedy)
- [x] Renderer: zero-crossing align, crossfade, pitch/time, limiter (§6.6)
- [x] Retro FX chain: 5 PS1 presets (§6.7)
- [x] WAV writer; OGG/Vorbis writer as default output (§6, production criteria)
- [x] CLI: `synth`, `batch`, `verify`, `phonemes` (§14)
- [x] Ship gate / provenance enforcement (§21–22)
- [x] Deterministic `--seed` renders (§19)
- [x] Unit tests, CMake, CI building with libvorbis (§16)

### Bonus (beyond TDD Phase 0)
- [x] `Engine` facade — one synthesis path for CLI + GUI (v0.2.0)
- [x] `grunt_gui` — type / preview-by-ear / export (v0.2.0)
- [x] **`generate` — the headline path: build banks from a license-cleared open
  TTS model, no recording. Registry + ship gate keep it airtight. (v0.4.0)**
  This is now grunt's primary workflow per the north star; the rest of the
  roadmap exists to make its output sound better, not to replace it.

---

## Phase 1 — Phoneme Debug Build  (TDD §18 Phase 1)  — [~] IN PROGRESS

Goal: convert text into real phoneme sequences; make `phonemes` a true view.

- [x] CMUdict-derived dictionary, shipped as a compact lookup (§6.2)
- [x] Rule-based grapheme→phoneme fallback for unknown words (§6.2)
- [x] Unknown-word report (which words hit the fallback) (§6.2, §14)
- [x] `phonemes` command emits real ARPAbet (replace Phase 0 stub) (§14)
- [ ] Mode C — Direct Phoneme Mode (`--mode phoneme`) for debug (§12)
- [ ] eSpeak NG as optional prototype-only phoneme front end, build-side,
      gated out of shipped banks (§6.2, §17 risk 4)

---

## Phase 2 — Syllable-Based Renderer  (TDD §18 Phase 2)  — [ ]

Goal: short lines sound game-ready from real syllable assets.

- [ ] Syllable database keyed by real syllable units (not grunt-mode guess)
- [ ] Syllable planner backed by phonemes (supersedes Phase 0 splitter)
- [ ] Fallback chain syllable → phoneme → grunt, scored (§6.3, §6.5)
- [ ] `coverage` command: % coverage, missing units, fallback rate (§14)
- [ ] Coverage regression check in CI over a fixed script corpus (§16)
- [ ] Mode B — Semi-Readable Speech (`--mode speech`) (§12)
- [ ] `analyze` command / BankAnalyzer: auto-fill pitch_center_hz + energy (§9, §14)

---

## Phase 3 — Diphone Smoothing  (TDD §18 Phase 3)  — [ ]

Goal: improve intelligibility via diphone units + real unit selection.

- [ ] Diphone unit support + metadata (start/end phone) (§9)
- [ ] Join-cost scoring with spectral/pitch discontinuity (§6.5)
- [ ] Viterbi lattice search (replace greedy selector) (§6.5)
- [ ] Better pitch/duration matching; PSOLA/WSOLA time-stretch (§6.6)
- [ ] Tightened anti-repetition over a window (§6.5)

---

## Phase 4 — Game Pipeline Integration  (TDD §18 Phase 4)  — [~] PARTIAL

Goal: usable by designers; clean handoff to gool.

- [x] Batch CSV → folder of named clips + `bank.json` manifest (§20)
- [x] OGG/Vorbis output (§20)
- [x] Deterministic, reproducible renders (§19)
- [ ] Per-character preset config (voice defaults applied per character)
- [ ] Confirm `bank.json` shape against gool's real importer; match its
      convention rather than the bespoke manifest (§20 open questions)
- [ ] Decide bare-folder auto-import vs. manifest-driven with gool (§20)
- [ ] Wire a real bank into DELCO_DANGEROUS as the test corpus (§23)
- [ ] CSV quoted-comma support (current splitter is naive) (§14)

---

## Cross-cutting / tech debt

- [ ] Replace `LICENSE` placeholder with full Apache-2.0 text (copy from gool)
- [ ] Record real owned voice banks to replace synthetic placeholder tones
- [ ] Real-ABI verify OGG output + GUI on a Windows libvorbis build
- [ ] Record the four MVP archetype voices: hero, heavy, nervous, creature (§10)
- [ ] Listening-test harness: 20 lines/voice, 1–5 scoring (§16)
- [ ] Performance budget checks: bank load / render / batch throughput (§15)

---

## Acceptance criteria status (TDD §15)

Functional: WAV [x] · four voices [~ one demo bank] · ≥3 emotions [x] ·
phoneme debug [~ stub] · coverage report [ ] · grunt fallback [x] ·
≥1 PS1 preset [x].

Audio quality: no clicks/pops [x] · no clipping >-1 dBFS [x] ·
intelligible barks [~] · per-render variation [x] · distinct voices
[~ needs real banks].

Production: offline [x] · CLI [x] · local banks [x] · no cloud [x] ·
OGG default [x] · pipeline-integrable [x].
