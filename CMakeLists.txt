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
    src/Types.cpp
    src/TextNormalizer.cpp
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
    src/Types.cpp
    src/TextNormalizer.cpp
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
