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
- [x] **Efforts & onomatopoeia (v0.6.0)** — `--effort <id>` (named, from
  data/efforts.json) and `--onomatopoeia "aaargh"` (literal spelling) on
  `synth`, both composing with `--character`. Closes the long-standing TDD §8
  gap where efforts were listed in the bank structure but never produced.

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

## Phase 2 — Syllable-Based Renderer  (TDD §18 Phase 2)  — [~] PARTIAL

Goal: short lines sound game-ready from real syllable assets.

- [x] Syllable planner backed by phonemes (`plan_phonemic`) — supersedes Phase 0
      splitter in the Engine; syllabifies ARPAbet at vowel nuclei (§6.3)
- [x] Fallback chain syllable → phoneme → grunt, scored by the selector with
      tier cost (§6.3, §6.5)
- [x] `coverage` command: syllable/phoneme/grunt fallback rates + top missing
      units over a script (§14)
- [x] Word-first planning + `generate` bakes word units keyed by the spoken
      word — the intelligibility path; a generated bank renders real words, not
      grunts (v0.13.0)
- [ ] Syllable database keyed by real syllable units (banks key by ARPAbet
      syllable string now; needs a generate-side path that emits those units)
- [ ] Coverage regression check in CI over a fixed script corpus (§16)
- [ ] Mode B — Semi-Readable Speech (`--mode speech`) (§12)
- [ ] `analyze` command / BankAnalyzer: auto-fill pitch_center_hz + energy (§9, §14)

---

## Phase 3 — Diphone Smoothing & Renderer DSP  (TDD §18 Phase 3)  — [~] PARTIAL

Goal: improve intelligibility via diphone units + real unit selection, and
raise renderer quality (formant/pitch/time DSP). The DSP sub-thread (which the
character roster depended on) is complete; diphone/Viterbi selection remains.

- [x] Formant shift / sub-octave / rasp character DSP (v0.7.0, §6.6)
- [x] PSOLA repitch/retime: phase-coherent time-stretch, pitch via
      resample+PSOLA-restore, graceful fallback on non-periodic audio (v0.8.0, §6.6)
- [ ] Diphone unit support + metadata (start/end phone) (§9)
- [ ] Join-cost scoring with spectral/pitch discontinuity (§6.5)
- [ ] Viterbi lattice search (replace greedy selector) (§6.5)
- [ ] Tightened anti-repetition over a window (§6.5)

---

## Character Presets — out-of-the-box voices for TTS mode  — [ ] PLANNED

The headline UX: a user types a line, **picks a character by name**, and gets a
clip in that character's voice. Presets are *recipes over the pipeline* — a
named bundle of (base TTS voice + pitch + FX preset + prosody bias + character
DSP), defined in `data/characters.json` so they're editable without
recompiling. Most characters layer over a few shared, license-cleared base TTS
voices; a few need their own. The GUI gets a character dropdown.

A preset names: `base_voice` (a registry model id), `pitch_offset_st`,
`fx_preset`, `emotion_bias`, and (later) `formant_shift` / `sub_layer`.

Out-of-the-box character set (target):

| Character | Base voice | Pitch | FX | Notes | Ready when |
|---|---|---|---|---|---|
| **Grunt** (gruff low male) | male | low | clean_ps1 + light sat | the default everyman | now-ish |
| **Deep Big** (very low gruff male) | male (shared w/ Grunt) | very low | clean_ps1 + chest sat | big-character voice | needs deeper pitch + sub |
| **Woman (raspy)** | female | mid | clean_ps1 + rasp (sat+bandpass) | raspy character | needs rasp DSP |
| **Orc** (barky monster) | male or neutral | low | monster_ps1 | heavy formant, syllable abstraction | needs formant shift |
| **Robot** | any | flat | robot_ps1 | formant + heavy bitcrush | now-ish (FX exists) |
| **Demon** | male (shared) | very low | monster_ps1 + sub layer | pitch-stacked, sub-octave | needs sub-layer + formant |
| **Yelling Woman** | female (shared) | mid-high | clean/radio + hot gain | urgent/angry prosody, clip-sat | now-ish |
| **Yelling Man** (Wilhelm) | male (shared) | mid | clean + hot gain | max-intensity prosody scream | now-ish |

Honest dependency split:

- **Achievable now** (pitch + existing FX + prosody bias only): Grunt, Robot,
  Yelling Woman, Yelling Man. These can ship as soon as the preset loader +
  two clean base voices exist.
- **Needs new renderer DSP** (Phase 3 work — formant shifting, sub-octave
  layering, rasp): Deep Big, Woman (raspy), Orc, Demon. These improve as the
  renderer gains formant/sub capability.

Tasks:
- [x] `CharacterPreset` type + `data/characters.json` loader (user-editable)
- [x] `--character` on `synth`; applies the recipe (pitch/FX/prosody/gain)
- [x] Real license-cleared base voice in the registry (LJ Speech, public-domain
      dataset, MIT model repo — `piper-en_US-ljspeech`)
- [x] All eight presets render. Formant shift / sub-octave / rasp DSP landed in
      v0.7.0, so Deep Big, Woman (raspy), Orc, and Demon are now fully realized
      rather than approximations.
- [ ] `--character` on `generate` too (generate base voice per character)
- [x] A second base voice (male): `piper-en_US-norman` (public-domain LibriVox,
      trained from scratch, same creator as the LJ Speech voice). Male-coded
      characters (grunt, deep_big, orc, demon, yelling_man, robot) now use it;
      woman_raspy and yelling_woman stay on LJ Speech. Roster spans two timbres.
- [x] GUI character dropdown (the eight presets; applies the recipe) + export
      creates its output dir (v0.14.0)
- [x] PSOLA for clean repitch/retime (v0.8.0): phase-coherent time-stretch,
      pitch-shift via resample+PSOLA-restore, graceful fallback to resample on
      non-periodic clips. Completes Phase 3.
- [ ] Each preset checked against the north star (lowers barrier, stays airtight)

---

## Quality refinements (inspired by external reading) — [ ] POLISH, not blocking

These are quality upgrades on top of working systems, deliberately NOT queued
ahead of breadth work (diphones/Viterbi, male voice, GUI) or ahead of verifying
the existing DSP by ear on Windows. Recorded so the design intent isn't lost.
Each must still pass the north star (and the DSP ones are best judged by ear,
which the sandbox can't do — verify on a real build before relying on them).

### Formant filter bank (refine the current resample-based formant shift)
Today `dsp::formant_shift` slides the whole spectral envelope by one ratio
(cheap, blunt — every formant moves together). The more correct model is a
**parallel bank of band-pass filters** at fixed centre frequencies F1/F2/F3,
independent of pitch — which is what actually distinguishes vocal-tract
*size/shape* (long tube = low formants = orc/big; short = high = small/child)
rather than a tape-speed feel.
- [ ] Biquad band-pass formant bank (3 formants min; F1/F2/F3 + per-formant Q/gain)
- [ ] Canonical vowel formant table as a starting point (ee/oo/i/e/u/a — F1≈270–660,
      F2≈870–2300, F3≈2400–3000 for a male tract); characters scale these by a
      vocal-tract factor instead of a flat envelope ratio
- [ ] Character presets gain optional explicit formant centres (orc: low; demon:
      low+wide Q; raspy: shifted + noisy) — replacing the single `formant_shift`
      scalar when present, falling back to it otherwise
- [ ] Keep the resample shift as the cheap default; bank is opt-in per character
Rationale: a correctness/quality refinement, not a fix — the resample shift
already gives the four DSP characters distinct timbres. Worth doing once the
current DSP is confirmed good by ear.

### Principled emotion → prosody rules
grunt already biases prosody by emotion; this makes the contours principled
rather than ad hoc, drawing on the classic emotion-in-synthetic-speech framework
(per-emotion pitch level, pitch range, tempo, loudness, and contour shape).
- [ ] Per-emotion prosody profiles (anger: higher mean F0, wider range, faster,
      louder, abrupt contours; fear: high + variable; sad: low, narrow, slow) as
      data (data/emotions.json), editable like characters/efforts
- [ ] ProsodyPlanner reads the profile instead of hard-coded biases
- [ ] Compose with character emotion_bias and effort intensity already in place
Rationale: directly serves "characterful, not realistic" — emotion legibility
is exactly the blank grunt fills. Lower-risk than the formant bank (it's
parameter shaping, not new signal processing).

### Positioning note (no build)
The synthetic-voice ethics reading (responsible synthetic voices, "hype vs
substance") reinforces grunt's existing stance rather than adding work: licensing
airtight by construction, real VO reserved for key lines, no voice cloning of
real people. Keep this posture explicit in README/docs; nothing to implement.

---

## Phase 4 — Game Pipeline Integration  (TDD §18 Phase 4)  — [~] PARTIAL

Goal: usable by designers; clean handoff to gool.

- [x] Batch CSV → folder of named clips + `bank.json` manifest (§20)
- [x] Frictionless first run: `grunt quickstart` (zero-config demo from bundled
      bank), `--version`, friendly landing, `examples/barks.csv`, copy-paste
      `SETUP.md`, CI smoke check (v0.10.0)
- [x] `grunt doctor`: checks the generate path (registry, piper, model files,
      writability) with precise fixes; `--live` does a test generation + gate
      check. De-risks the first real bank (v0.15.0)
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

- [x] Executable-relative resource resolution (`ResourcePath`) — data/voices
      found relative to the binary, so a double-clicked app works (v0.12.0)
- [x] Windows GUI package published by release CI (download → unzip →
      double-click `grunt_gui.exe`) (v0.12.0)
- [ ] macOS `.app` + Linux GUI packages in release CI (Windows done first)
- [ ] Code signing / notarization so launch shows no unsigned-app warning
      (needs paid Apple Developer + Windows signing cert — B$ to provide)
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
