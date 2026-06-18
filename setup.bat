@echo off
REM ===========================================================================
REM  grunt - NPC archetype setup for Windows
REM
REM  Installs Piper, downloads the three public-domain voices the archetypes
REM  use (Norman / John / Bryce), and bakes all four ready-to-use NPC bark sets
REM  (cop, jersey, russian, delco) into their own folders. Run from the
REM  unzipped grunt package (where grunt.exe lives). Re-running is safe.
REM
REM  The window STAYS OPEN at the end (success or failure) so you can read it.
REM ===========================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

echo(
echo ============================================
echo   grunt - NPC archetype setup
echo ============================================
echo(

set "GRUNT=grunt.exe"
if not exist "%GRUNT%" (
  echo [X] grunt.exe not found next to this script.
  echo     Run setup.bat from inside the unzipped grunt package folder.
  goto :fail
)
echo [ok] found grunt.exe

REM --- 1. Piper engine (modern Python package) -----------------------------
set "PIPER_CMD="
piper --help >NUL 2>&1 && set "PIPER_CMD=piper"
if not defined PIPER_CMD ( python -m piper --help >NUL 2>&1 && set "PIPER_CMD=python -m piper" )
if not defined PIPER_CMD ( py -m piper --help >NUL 2>&1 && set "PIPER_CMD=py -m piper" )

if defined PIPER_CMD (
  echo [ok] Piper already available via: !PIPER_CMD!
) else (
  echo [1/3] Installing Piper TTS engine via pip...
  set "PY="
  python --version >NUL 2>&1 && set "PY=python"
  if not defined PY ( py --version >NUL 2>&1 && set "PY=py" )
  if not defined PY (
    echo [X] Python not found. Install Python 3.9+ from https://python.org
    echo     ^(check "Add python.exe to PATH" during install^), then re-run setup.bat.
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

REM --- 2. Voices the four archetypes use (public domain, auto-downloaded) ---
REM   norman -> jersey + russian   john -> cop   bryce -> delco
echo(
echo [2/3] Downloading archetype voices (public domain)...
for %%V in (piper-en_US-norman piper-en_US-john piper-en_US-bryce) do (
  echo   - %%V
  "%GRUNT%" fetch-voice --model %%V
  if errorlevel 1 ( echo [X] could not download %%V. Check your connection. & goto :fail )
)
echo [ok] archetype voices ready

REM --- 3. Bake the four ready-to-use NPC bark sets -------------------------
echo(
echo [3/3] Baking the four NPC bark sets...
call :bake cop      cop_barks.csv      cop_vo
if errorlevel 1 goto :fail
call :bake jersey   jersey_barks.csv   jersey_vo
if errorlevel 1 goto :fail
call :bake russian  russian_barks.csv  russian_vo
if errorlevel 1 goto :fail
call :bake delco    delco_barks.csv    delco_vo
if errorlevel 1 goto :fail

echo(
echo ============================================
echo   SUCCESS - four NPC voice sets are ready.
echo ============================================
echo   cop_vo\      FBI / cop      (clean, radio)
echo   jersey_vo\   Italian NJ mob (raspy, mid)
echo   russian_vo\  Russian mafia  (deep, cold)
echo   delco_vo\    Delco PA lead  (young, scrappy)
echo(
echo Each folder holds one .ogg per line, named by its trigger key - drop a
echo folder into your Godot project's res:// and play clips by name with gool.
echo(
echo To make your OWN lines: open grunt_gui.exe, pick a character, type a line,
echo tune it with the sliders, and use the Bark list panel to build + bake a set.
echo Edit any examples\*_barks.csv to change what these characters say.
echo(
goto :done

REM --- helper: bake one archetype -------------------------------------------
:bake
REM %1 = label  %2 = csv name (in examples\)  %3 = out folder
echo   - %~1 -^> %~3\
"%GRUNT%" batch --csv "examples\%~2" --out-dir "%~3"
if errorlevel 1 ( echo [X] baking %~1 failed - see messages above. & exit /b 1 )
exit /b 0

:fail
echo(
echo ============================================
echo   SETUP DID NOT FINISH
echo ============================================
echo The step marked [X] above is what failed. Fixes:
echo   - check your internet connection and re-run setup.bat
echo   - or see SETUP.md for the manual steps
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
