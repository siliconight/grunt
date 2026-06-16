# grunt — setup

## Just want to click and go? (Windows, no building)

Each tagged release has a **`grunt-vX.Y.Z-windows.zip`** attached (under
Assets on the GitHub Releases page). It contains a ready-to-run GUI — no
toolchain, no build.

1. Download and unzip it anywhere.
2. Double-click **`grunt_gui.exe`**.
3. Type a line, pick a character, preview by ear, export an `.ogg`.

The folder already has `data/` and the bundled demo bank beside the exe, so it
finds everything no matter where you put it.

> First launch, Windows may show a SmartScreen notice because the app isn't
> code-signed (signing needs a paid certificate). Click **More info → Run
> anyway**. The app is the same binary built by the project's release CI from
> this source.

Prefer the command line, or on macOS/Linux? Build from source below.

## Fastest path from source: hear it now (no setup)

If you've built grunt, you already have everything for a first sound:

```
grunt quickstart
```

That renders demo clips (a couple of characters, a couple of efforts/screams)
from the bundled voice bank into `./grunt_quickstart/`. No Piper, no downloads.
Play them, then try your own line:

```
grunt synth --text "take cover" --character grunt --voice voices/heavy_brother --out test.ogg
```

The bundled bank is a small demo. To make banks in *your* characters' voices,
set up the generate path below.

---

## Build

Needs a C++20 compiler. OGG output additionally needs **libvorbis /
libvorbisenc / libogg** dev headers (without them grunt still builds and writes
WAV).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

No CMake? Direct build (WAV-only):

```
g++ -std=c++20 -O2 -Iinclude src/*.cpp -o grunt   # exclude src/gui_main.cpp unless building the GUI
```

Check it:

```
grunt --version
```

---

## Generate path: type text → spoken banks (needs Piper)

This is grunt's headline workflow: synthesize a bank's units from a
license-cleared open TTS voice, with no recording. It has a one-time setup.

### 1. Install Piper (a build-time tool — grunt shells out to it, never links it)

Piper is a small, offline, MIT-licensed TTS engine. Get a release binary from
its project page and put `piper` on your PATH. Verify:

```
piper --help
```

### 2. Download a license-cleared voice model

grunt only generates from voices listed in `data/voice_models.json` (each one
is license diligence done once, then the ship gate enforces it). Two are
registered and ready:

| Registry id | Voice | Files to download |
|---|---|---|
| `piper-en_US-ljspeech` | US English female | `en_US-ljspeech-high.onnx` + `.onnx.json` |
| `piper-en_US-norman` | US English male | `norman.onnx` + `.onnx.json` |

- LJ Speech: from the rhasspy/piper-voices repo, path
  `en/en_US/ljspeech/high/`.
- Norman: from `https://brycebeattie.com/files/tts/` (the `norman.onnx` and its
  `.onnx.json`).

Put the two files for whichever voice you want next to each other, and make sure
the `.onnx` filename matches `model_file` in `data/voice_models.json` (or edit
that field to match what you downloaded). That's the whole setup.

> Licensing note: both registered voices were trained from scratch on
> public-domain corpora — about as clean as open TTS gets. This isn't legal
> advice; confirm provenance before a commercial release. The ship gate refuses
> to bake any clip whose model isn't marked commercial + redistributable, so a
> bad model can't slip into a shipped bank.

### 3. Generate a bank

**First, check your setup:**

```
grunt doctor
```

It verifies each piece of the generate path — registry, the `piper` binary,
whether a voice model is downloaded, and that grunt can write a bank — and for
anything missing it prints the exact fix. When it says **ready**, you're set.
Add `--live` to also have it generate one test word and run it through the ship
gate:

```
grunt doctor --live
```

**Then make your first talking bank.** There's an example script at
`examples/barks.csv` (rows are `key,text`):

```
grunt generate --units examples/barks.csv --voice voices/my_guards --model piper-en_US-norman
```

That writes word units into `voices/my_guards/` with provenance stamped from the
model's license. Render lines from it, through a character:

```
grunt synth --text "intruder" --character orc --voice voices/my_guards --out spotted.ogg
grunt synth --effort pain_death --character yelling_man --voice voices/my_guards --out death.ogg
```

In the GUI, open **advanced** and point the voice-bank folder at
`voices/my_guards`, then pick a character from the dropdown — now you'll hear
words, not just grunts (the bundled demo bank is grunt-only by design).

### 4. Verify before you ship

```
grunt verify --voice voices/my_guards
```

The gate confirms every clip is commercial + redistributable. If it passes, the
bank is clean to ship in your game.

---

## Into Godot

grunt writes plain `.ogg` files. Drop them in your project under `res://` and
they import like any other audio asset — or hand the folder to gool, which
plays clips by name at runtime. No grunt runtime, no plugin: the clips are just
files, and they're yours.
