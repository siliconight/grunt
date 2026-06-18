# grunt — Piper auto-bootstrap (first-run "just works" for typed lines)

## The goal
Today the Windows package speaks the pre-baked **starter** words out of the box,
but **typing a new line** needs Piper, which the user installs by hand. This plan
closes that gap with the lightest option: on first need, grunt runs
`pip install piper-tts` itself, so the user does nothing.

Decision (chosen): **auto-run `pip install piper-tts`** — assumes Python is
present, downloads Piper from PyPI on the user's machine. grunt ships nothing
extra and never redistributes a GPL binary.

## Why this option is clean on licensing
- piper-tts is **GPL-3.0-or-later**. Bundling its binary inside grunt's
  Apache-2.0 zip would mean shipping a GPL artifact alongside — a question worth
  avoiding for now.
- With pip-bootstrap, **the user's own machine fetches Piper from PyPI.** grunt
  only *invokes* it as a separate process (already how it works — a build-time
  shell-out, never linked). No GPL code ships in grunt's package. Same posture
  grunt already relies on; this just automates the install step.

## What it does NOT solve (be honest in the UI)
1. **Python must already be present.** If there's no Python, pip-bootstrap can't
   run. grunt must detect this and tell the user plainly: "Install Python 3.9+
   from python.org, then click Retry." (A future option could bootstrap a
   standalone Piper that needs no Python — deferred; heavier.)
2. **Norman is still a manual voice download.** Separate from Piper. The gangster
   character wants `norman` (no auto-fetch URL). Bootstrapping Piper does not get
   Norman. The user still drops `norman.onnx` + `.json` next to grunt, OR uses a
   character whose base voice is the auto-fetchable `ljspeech`.

So even after this feature: "type anything as **Norman**, zero setup" = Piper
auto-installs + ljspeech auto-fetches, but **Norman is one manual step**. "Type
anything in a **female** voice (ljspeech), zero setup" = fully automatic.

## Design

### 1. Where it hooks in
`detect_piper_cmd()` in `src/Generator.cpp` is the single seam. It already probes
a list of candidate commands and returns the first that works, or `""`. Today,
empty → error "install it with: pip install piper-tts". The change: when the
probe comes up empty, **offer to run the install**, then re-probe.

Probe order should also gain a **bundled/exe-relative** slot first (future-proof
for a standalone Piper), but for this version the list is unchanged; we only add
the bootstrap-on-miss behavior.

### 2. CLI behavior (`synth`, `batch`, `generate`)
- On `detect_piper_cmd()` == "" → print: "Piper isn't installed. Install it now?
  [y/N]" (only when stdin is a TTY). On `y`: run the pip command, stream output,
  re-probe, continue. On `n` or non-TTY (CI): keep today's actionable error and
  exit — **never auto-install silently in CI or scripts.**
- Add an explicit `grunt setup-piper` subcommand that just runs the install
  (no prompt) — scriptable, and what the GUI button calls.

### 3. GUI behavior (the real win)
- When Line mode is used and `detect_piper_cmd()` is empty, the status line
  already surfaces the error. Replace the dead-end with an actionable panel:
  - If **Python present**: a button **"Install speech engine"** → runs the pip
    command in a worker thread, shows streaming progress in the status area,
    re-probes on finish, then the Play that triggered it can be retried.
  - If **Python absent**: a message + link guidance: "Speech needs Python 3.9+.
    Get it from python.org, then click Retry." (No silent failure.)
- This is the difference between "GUI says Synth failed" (today) and "GUI offers
  to fix it with one click" (goal).

### 4. The pip command (cross-platform, robust)
- Resolve a Python: try `py -3` (Windows launcher), then `python`, then
  `python3`. First that reports `--version` >= 3.9 wins.
- Run: `<python> -m pip install --user --upgrade piper-tts`
  - `--user` avoids permission issues on system Python.
  - Capture stdout+stderr to show the user; a failure (no network, externally-
    managed env, etc.) shows the raw pip message + a one-line hint.
- After install, set `GRUNT_PIPER_CMD` for the session to the resolved
  `<python> -m piper` so the very next synth uses it without a fresh probe.

### 5. Detecting "Python present"
A tiny probe before offering pip: run `<python> --version`, parse `Python 3.x`,
require x>=9 on the 3-series. Drives the present/absent branch in §3.

## Edge cases to handle (these are where it'll bite)
- **Externally-managed environment** (PEP 668; some Python installs refuse
  `pip install` outside a venv). pip errors with "externally-managed-
  environment". Detect that string and fall back to `--user --break-system-
  packages`, or instruct a venv. (Common on Linux distros; less so on the
  python.org Windows installer, which is the target here.)
- **No network.** pip fails clearly; surface it; do not hang the GUI (worker
  thread + timeout).
- **Two Pythons / wrong one on PATH.** Pin the resolved interpreter into
  `GRUNT_PIPER_CMD` so detection and invocation agree.
- **First synth latency.** The pip install is ~14 MB wheel + onnxruntime (~18 MB)
  + numpy — tens of MB, a few seconds to a minute. The GUI must show progress,
  not look frozen.
- **CI must never auto-install via the prompt path.** CI already does an explicit
  `pip install piper-tts` step, and the prompt only fires on a TTY, so CI is
  unaffected — but add a guard regardless.

## Scope estimate
- `detect_piper_cmd()` + a new `install_piper()` helper in Generator: ~40 lines.
- CLI prompt + `setup-piper` subcommand in main.cpp: ~30 lines.
- GUI: a status-area button + worker thread + progress text: ~60 lines (the
  fiddliest part — threading the pip subprocess so the UI stays responsive).
- All of it is verifiable in the sandbox only at the logic level (`--generator
  stub`, and unit-testing the version-parse / env-resolve helpers); the actual
  pip run + GUI button are B$-machine / CI verified, same as everything else.

## What this changes for the user's wish
- **"Download zip, type a female-voice line, hear it"** → fully automatic after
  this ships (Piper auto-installs, ljspeech auto-fetches).
- **"...in Norman's voice"** → still one manual voice drop. To make Norman
  one-click too would need either a redistribution-clean host with a direct URL
  in the registry (so `fetch-voice` can grab it), or shipping norman in the zip
  (provenance is a personal attestation, not a redistribution license — the
  thing to ask IP counsel about before doing).

## Suggested order
1. `install_piper()` helper + `setup-piper` CLI subcommand (smallest, testable).
2. CLI prompt-on-miss.
3. GUI button + worker thread (the actual UX payoff).
4. Separately, pursue a registry direct-URL for Norman (or counsel sign-off to
   ship it) so the male voice can be one-click too.
