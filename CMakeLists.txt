cmake_minimum_required(VERSION 3.16)
project(grunt LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

if(MSVC)
    add_compile_options(/W4 /permissive-)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
else()
    add_compile_options(-Wall -Wextra)
endif()

# Lesson from gool: CI should build with strict flags. Enable with
# -DGRUNT_WERROR=ON (CI does this); off by default for local iteration.
option(GRUNT_WERROR "Treat warnings as errors" OFF)
if(GRUNT_WERROR)
    if(MSVC)
        add_compile_options(/WX)
    else()
        add_compile_options(-Werror)
    endif()
endif()

add_executable(grunt
    src/main.cpp
    src/Engine.cpp
    src/Types.cpp
    src/TextNormalizer.cpp
    src/PhonemeMapper.cpp
    src/SyllablePlanner.cpp
    src/ProsodyPlanner.cpp
    src/UnitDatabase.cpp
    src/UnitSelector.cpp
    src/AudioRenderer.cpp
    src/RetroFxChain.cpp
    src/ShipGate.cpp
    src/Wav.cpp
    src/AudioOut.cpp
    src/Json.cpp
)

target_include_directories(grunt PRIVATE include)

# --- OGG Vorbis encoding (default output format) ---------------------------
# grunt defaults to OGG/Vorbis. It needs libvorbisenc + libvorbis + libogg.
# If they aren't found, grunt still builds but can only emit WAV (and will say
# so). On Windows, install via vcpkg: `vcpkg install libvorbis libogg`.
find_library(OGG_LIB ogg)
find_library(VORBIS_LIB vorbis)
find_library(VORBISENC_LIB vorbisenc)
find_path(VORBIS_INCLUDE vorbis/vorbisenc.h)

if(OGG_LIB AND VORBIS_LIB AND VORBISENC_LIB AND VORBIS_INCLUDE)
    message(STATUS "grunt: libvorbis found — OGG/Vorbis output enabled")
    target_compile_definitions(grunt PRIVATE GRUNT_HAVE_VORBIS)
    target_include_directories(grunt PRIVATE ${VORBIS_INCLUDE})
    target_link_libraries(grunt PRIVATE ${VORBISENC_LIB} ${VORBIS_LIB} ${OGG_LIB})
else()
    message(WARNING "grunt: libvorbis NOT found — building WAV-only. "
                    "Install libvorbis/libogg dev packages for OGG output.")
endif()

# unit tests
enable_testing()
add_executable(grunt_tests
    tests/test_main.cpp
    src/Engine.cpp
    src/Types.cpp
    src/TextNormalizer.cpp
    src/PhonemeMapper.cpp
    src/SyllablePlanner.cpp
    src/ProsodyPlanner.cpp
    src/UnitDatabase.cpp
    src/UnitSelector.cpp
    src/AudioRenderer.cpp
    src/RetroFxChain.cpp
    src/ShipGate.cpp
    src/Wav.cpp
    src/AudioOut.cpp
    src/Json.cpp
)
target_include_directories(grunt_tests PRIVATE include)
add_test(NAME grunt_tests COMMAND grunt_tests)

# ---------------------------------------------------------------------------
# Optional GUI (grunt_gui): type text, preview by ear, export a clip.
# Off by default. Enable with -DGRUNT_BUILD_GUI=ON after vendoring deps into
# third_party/ (Dear ImGui + GLFW backends, miniaudio.h). See README "GUI".
# ---------------------------------------------------------------------------
option(GRUNT_BUILD_GUI "Build the grunt GUI (needs Dear ImGui + GLFW + miniaudio)" OFF)

if(GRUNT_BUILD_GUI)
    set(IMGUI_DIR    ${CMAKE_SOURCE_DIR}/third_party/imgui)
    set(MINIAUDIO_DIR ${CMAKE_SOURCE_DIR}/third_party/miniaudio)
    set(STB_DIR       ${CMAKE_SOURCE_DIR}/third_party/stb)

    find_package(glfw3 QUIET)
    find_package(OpenGL REQUIRED)

    if(NOT EXISTS ${IMGUI_DIR}/imgui.cpp)
        message(FATAL_ERROR
            "GRUNT_BUILD_GUI is ON but Dear ImGui was not found at ${IMGUI_DIR}. "
            "See the GUI section of the README for vendoring instructions.")
    endif()

    add_executable(grunt_gui
        src/gui_main.cpp
        src/Engine.cpp
        src/Types.cpp
        src/TextNormalizer.cpp
    src/PhonemeMapper.cpp
        src/SyllablePlanner.cpp
        src/ProsodyPlanner.cpp
        src/UnitDatabase.cpp
        src/UnitSelector.cpp
        src/AudioRenderer.cpp
        src/RetroFxChain.cpp
        src/ShipGate.cpp
        src/Wav.cpp
        src/AudioOut.cpp
        src/Json.cpp
        # Dear ImGui core + backends (vendored)
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
        ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
    )
    target_include_directories(grunt_gui PRIVATE
        include ${IMGUI_DIR} ${IMGUI_DIR}/backends ${MINIAUDIO_DIR} ${STB_DIR})
    target_link_libraries(grunt_gui PRIVATE glfw OpenGL::GL)

    if(OGG_LIB AND VORBIS_LIB AND VORBISENC_LIB AND VORBIS_INCLUDE)
        target_compile_definitions(grunt_gui PRIVATE GRUNT_HAVE_VORBIS)
        target_include_directories(grunt_gui PRIVATE ${VORBIS_INCLUDE})
        target_link_libraries(grunt_gui PRIVATE ${VORBISENC_LIB} ${VORBIS_LIB} ${OGG_LIB})
    endif()
endif()
