@echo off
REM ===========================================================================
REM  grunt — first-bank setup for Windows
REM
REM  Automates the generate-path dependencies so you can hear real English words:
REM    1. downloads the Piper TTS engine (offline, MIT) into .\piper\
REM    2. downloads a license-cleared voice model (LJ Speech, public domain)
REM    3. puts piper on PATH for this session
REM    4. runs `grunt doctor --live` to verify the whole chain
REM    5. generates your first talking bank from examples\barks.csv
REM    6. plays a real spoken word
REM
REM  Run this from the unzipped grunt package folder (where grunt.exe lives).
REM  Re-running is safe: it skips downloads that are already present.
REM ===========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo(
echo === grunt first-bank setup ===
echo(

REM --- locate grunt.exe (this script ships beside it) -----------------------
set "GRUNT=grunt.exe"
if not exist "%GRUNT%" (
  echo ERROR: grunt.exe not found next to this script.
  echo Run setup.bat from inside the unzipped grunt package folder.
  goto :fail
)

REM --- 1. Piper engine ------------------------------------------------------
set "PIPER_DIR=%cd%\piper"
set "PIPER_EXE=%PIPER_DIR%\piper\piper.exe"
if exist "%PIPER_EXE%" (
  echo [skip] Piper already present at %PIPER_EXE%
) else (
  echo [1/6] Downloading Piper TTS engine...
  set "PIPER_ZIP=%cd%\piper_windows_amd64.zip"
  set "PIPER_URL=https://github.com/rhasspy/piper/releases/download/2023.11.14-2/piper_windows_amd64.zip"
  powershell -NoProfile -Command "try { Invoke-WebRequest -Uri '!PIPER_URL!' -OutFile '!PIPER_ZIP!' } catch { exit 1 }"
  if errorlevel 1 ( echo ERROR: could not download Piper. Check your connection. & goto :fail )
  echo       Extracting...
  powershell -NoProfile -Command "Expand-Archive -Force '!PIPER_ZIP!' '%PIPER_DIR%'"
  del "!PIPER_ZIP!" >nul 2>&1
  if not exist "%PIPER_EXE%" (
    REM some releases extract piper.exe at the dir root rather than piper\piper\
    if exist "%PIPER_DIR%\piper.exe" set "PIPER_EXE=%PIPER_DIR%\piper.exe"
  )
)

REM put piper on PATH for this session and for the model dir lookup
for %%I in ("%PIPER_EXE%") do set "PIPER_BIN_DIR=%%~dpI"
set "PATH=%PIPER_BIN_DIR%;%PATH%"

REM --- 2. Voice model (LJ Speech — public domain, verifiable URL) -----------
REM  This is the female base voice. To use the MALE Norman voice instead,
REM  download norman.onnx + norman.onnx.json from https://brycebeattie.com/files/tts
REM  into this folder and change MODEL_ID below to piper-en_US-norman.
set "MODEL=en_US-ljspeech-high.onnx"
set "MODEL_ID=piper-en_US-ljspeech"
set "MODEL_BASE=https://huggingface.co/rhasspy/piper-voices/resolve/v1.0.0/en/en_US/ljspeech/high"
if exist "%MODEL%" (
  echo [skip] Voice model already present: %MODEL%
) else (
  echo [2/6] Downloading voice model (LJ Speech, public domain)...
  powershell -NoProfile -Command "try { Invoke-WebRequest -Uri '%MODEL_BASE%/%MODEL%?download=true' -OutFile '%MODEL%' } catch { exit 1 }"
  if errorlevel 1 ( echo ERROR: could not download the voice model. & goto :fail )
  powershell -NoProfile -Command "try { Invoke-WebRequest -Uri '%MODEL_BASE%/%MODEL%.json?download=true' -OutFile '%MODEL%.json' } catch { exit 1 }"
  if errorlevel 1 ( echo ERROR: could not download the model config (.json). & goto :fail )
)

echo(
echo [3/6] Piper on PATH for this session: %PIPER_BIN_DIR%
echo(

REM --- 4. doctor (live) -----------------------------------------------------
echo [4/6] Verifying the setup with: grunt doctor --live
echo(
"%GRUNT%" doctor --live
echo(

REM --- 5. first bank --------------------------------------------------------
echo [5/6] Generating your first talking bank into voices\my_guards ...
"%GRUNT%" generate --units examples\barks.csv --voice voices\my_guards --model %MODEL_ID%
if errorlevel 1 ( echo ERROR: generate failed. See messages above. & goto :fail )

REM --- 6. speak -------------------------------------------------------------
echo(
echo [6/6] Rendering a real spoken word...
"%GRUNT%" synth --text "intruder" --character orc --voice voices\my_guards --out first_word.ogg
if errorlevel 1 ( echo ERROR: synth failed. & goto :fail )

echo(
echo === done ===
echo Playing first_word.ogg  (an orc saying "intruder")
start "" "first_word.ogg"
echo(
echo You're set up. From here:
echo   - GUI: run grunt_gui.exe, open "advanced", point the bank at voices\my_guards, pick a character
echo   - CLI: grunt synth --text "your line" --character orc --voice voices\my_guards --out line.ogg
echo   - edit examples\barks.csv to add your own lines, then re-run the generate step
echo(
goto :end

:fail
echo(
echo Setup did not finish. You can still try the zero-setup demo:  grunt.exe quickstart
echo Or see SETUP.md for the manual steps.
exit /b 1

:end
endlocal
