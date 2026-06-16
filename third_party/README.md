# third_party — vendored GUI dependencies

The GUI (`grunt_gui`) needs three dependencies that are NOT committed to this
repo. Vendor them here, then build with `-DGRUNT_BUILD_GUI=ON`.

## 1. Dear ImGui  ->  third_party/imgui/
MIT licensed. Clone the repo and copy its root + backends:

    git clone https://github.com/ocornut/imgui third_party/imgui

CMake expects these to exist:
    third_party/imgui/imgui.cpp
    third_party/imgui/imgui_draw.cpp
    third_party/imgui/imgui_tables.cpp
    third_party/imgui/imgui_widgets.cpp
    third_party/imgui/backends/imgui_impl_glfw.cpp
    third_party/imgui/backends/imgui_impl_opengl3.cpp

## 2. miniaudio  ->  third_party/miniaudio/miniaudio.h
Public domain (or MIT-0). Single header:

    curl -L https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h \
        -o third_party/miniaudio/miniaudio.h

## 3. GLFW  (system package or vcpkg)
- Windows: `vcpkg install glfw3` (configure with the vcpkg toolchain)
- Debian/Ubuntu: `sudo apt install libglfw3-dev`
- macOS: `brew install glfw`

## Build with GUI

    cmake -S . -B build -DGRUNT_BUILD_GUI=ON
    cmake --build build
    ./build/grunt_gui

All three are permissively licensed (MIT / public-domain / Zlib), so they don't
affect grunt's Apache-2.0 posture. They're also GUI-only — the CLI and the
baked clips never depend on them.
