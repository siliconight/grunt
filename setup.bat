@echo off
REM ===========================================================================
REM  grunt - first-bank setup for Windows
REM
REM  Downloads Piper + a public-domain voice, builds a real talking bank, and
REM  plays a spoken word. Run from the unzipped grunt package (where grunt.exe
REM  lives). Re-running is safe (skips what's already downloaded).
REM
REM  The window STAYS OPEN at the end (success or failure) so you can read it.
REM ===========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo(
echo ============================================
echo   grunt first-bank setup
echo ============================================
echo(

set "GRUNT=grunt.exe"
if not exist "%GRUNT%" (
  echo [X] grunt.exe not found next to this script.
  echo     Run setup.bat from inside the unzipped grunt package folder.
  goto :fail
)
echo [ok] found grunt.exe

REM --- 1. Piper engine (modern Python package; the old standalone exe crashes
REM        with a ucrtbase error on current Windows) -------------------------
set "PIPER_CMD="
REM is a usable piper already available?
piper --help >NUL 2>&1 && set "PIPER_CMD=piper"
if not defined PIPER_CMD ( python -m piper --help >NUL 2>&1 && set "PIPER_CMD=python -m piper" )
if not defined PIPER_CMD ( py -m piper --help >NUL 2>&1 && set "PIPER_CMD=py -m piper" )

if defined PIPER_CMD (
  echo [ok] Piper already available via: !PIPER_CMD!
) else (
  echo [1/6] Installing Piper TTS engine via pip...
  REM need Python + pip
  set "PY="
  python --version >NUL 2>&1 && set "PY=python"
  if not defined PY ( py --version >NUL 2>&1 && set "PY=py" )
  if not defined PY (
    echo [X] Python not found. Install Python 3.9+ from https://python.org
    echo     (check "Add python.exe to PATH" during install^), then re-run setup.bat.
    goto :fail
  )
  !PY! -m pip install --quiet --upgrade piper-tts
  if errorlevel 1 ( echo [X] pip install piper-tts failed. Check your connection. & goto :fail )
  !PY! -m piper --help >NUL 2>&1 && set "PIPER_CMD=!PY! -m piper"
)

if not defined PIPER_CMD (
  echo [X] Piper still not runnable after install. See SETUP.md.
  goto :fail
)
echo [ok] piper ready: !PIPER_CMD!
REM grunt reads this to know how to invoke piper
set "GRUNT_PIPER_CMD=!PIPER_CMD!"

REM --- 2. Voice model (LJ Speech - public domain, verifiable URL) -----------
REM  Female base voice. For the MALE Norman voice: download norman.onnx +
REM  norman.onnx.json from https://brycebeattie.com/files/tts into this folder
REM  and set MODEL_ID=piper-en_US-norman, MODEL=norman.onnx below.
set "MODEL=en_US-ljspeech-high.onnx"
set "MODEL_ID=piper-en_US-ljspeech"
set "MODEL_BASE=https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/en/en_US/ljspeech/high"
if exist "%MODEL%" (
  echo [ok] Voice model already present: %MODEL%
) else (
  echo [2/6] Downloading voice model - LJ Speech, public domain...
  powershell -NoProfile -Command "try { Invoke-WebRequest -Uri '%MODEL_BASE%/%MODEL%?download=true' -OutFile '%MODEL%' -UseBasicParsing } catch { Write-Host $_.Exception.Message; exit 1 }"
  if errorlevel 1 ( echo [X] Could not download the voice model. & goto :fail )
  powershell -NoProfile -Command "try { Invoke-WebRequest -Uri '%MODEL_BASE%/%MODEL%.json?download=true' -OutFile '%MODEL%.json' -UseBasicParsing } catch { Write-Host $_.Exception.Message; exit 1 }"
  if errorlevel 1 ( echo [X] Could not download the model config json. & goto :fail )
)
if not exist "%MODEL%"      ( echo [X] model file missing after download. & goto :fail )
if not exist "%MODEL%.json" ( echo [X] model .json missing after download. & goto :fail )
echo [ok] voice model ready: %MODEL%

REM --- 3/4. verify the chain ------------------------------------------------
echo(
echo [3/6] Using piper: %GRUNT_PIPER_CMD%
echo [4/6] Verifying with: grunt doctor --live
echo(
"%GRUNT%" doctor --live
echo(

REM --- 5. first bank --------------------------------------------------------
echo [5/6] Generating your first talking bank into voices\my_guards ...
"%GRUNT%" generate --units examples\barks.csv --voice voices\my_guards --model %MODEL_ID%
if errorlevel 1 ( echo [X] generate failed - see the messages above. & goto :fail )

REM confirm the bank actually got units (catches a silent empty generate)
if not exist "voices\my_guards\metadata\units.json" (
  echo [X] generate ran but no units.json was written. The bank is empty.
  goto :fail
)
echo [ok] bank generated: voices\my_guards

REM --- 6. speak -------------------------------------------------------------
echo(
echo [6/6] Rendering a real spoken word...
"%GRUNT%" synth --text "intruder" --character orc --voice voices\my_guards --out first_word.ogg
if errorlevel 1 ( echo [X] synth failed. & goto :fail )
if not exist "first_word.ogg" ( echo [X] no output file was written. & goto :fail )

echo(
echo ============================================
echo   SUCCESS - you have a real talking bank.
echo ============================================
echo Playing first_word.ogg - an orc saying intruder...
start "" "first_word.ogg"
echo(
echo From here:
echo   GUI: run grunt_gui.exe, open "advanced", point the bank at
echo        voices\my_guards, then pick a character from the dropdown.
echo   CLI: grunt synth --text "your line" --character orc --voice voices\my_guards --out line.ogg
echo   Edit examples\barks.csv to add your own lines, then re-run this.
echo(
echo NOTE: the bundled demo bank heavy_brother is grunt-only by design, so
echo       it sounds like grunts. voices\my_guards is your REAL words bank -
echo       point the GUI at it to hear speech.
echo(
goto :done

:fail
echo(
echo ============================================
echo   SETUP DID NOT FINISH
echo ============================================
echo The step marked [X] above is what failed. Fixes:
echo   - check your internet connection and re-run setup.bat
echo   - or see SETUP.md for the manual steps
echo   - you can always hear the grunt-only demo: grunt.exe quickstart
echo(
echo This window will stay open. Press any key to close it.
pause >nul
endlocal
exit /b 1

:done
echo This window will stay open so you can read the results.
echo Press any key to close it.
pause >nul
endlocal
