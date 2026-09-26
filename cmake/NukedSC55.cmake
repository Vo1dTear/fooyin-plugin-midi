# SPDX-License-Identifier: GPL-3.0-or-later
# This integration script is licensed under GPL-3.0-or-later.
# The Nuked-SC55 backend is licensed under GPL-2.0-or-later;
# its copyright notices and LICENSE remain in the upstream source tree.
# Upstream: https://github.com/jcmoyer/Nuked-SC55

# Build only the embeddable backend; upstream's frontends require SDL/RtMidi.
set(nuked_sc55_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/Nuked-SC55")
set(nuked_sc55_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/nuked-sc55")
if(NOT EXISTS "${nuked_sc55_SOURCE_DIR}/src/backend/emu.h")
    message(FATAL_ERROR
        "Nuked-SC55 sources are missing. Run from the repository root: "
        "git submodule update --init --recursive. "
        "Alternatively, configure with -DMIDI_ENABLE_NUKED_SC55=OFF.")
endif()

set(NUKED_ENABLE_DECODER2 OFF)
set(NUKED_ENABLE_ASIO OFF)
set(NUKED_SOURCE "fooyin-plugin-midi")
function(configure_nuked_version)
    set(CMAKE_PROJECT_VERSION "0.7.0")
    set(CMAKE_PROJECT_VERSION_MAJOR 0)
    set(CMAKE_PROJECT_VERSION_MINOR 7)
    set(CMAKE_PROJECT_VERSION_PATCH 0)
    configure_file("${nuked_sc55_SOURCE_DIR}/src/backend/config.h.in"
                   "${nuked_sc55_BINARY_DIR}/backend/config.h" @ONLY)
endfunction()
configure_nuked_version()
set(nuked_sources
    config.cpp diagnostics.cpp emu.cpp file_hashing.cpp file_io.cpp lcd.cpp
    mcu_interrupt.cpp mcu_opcodes.cpp mcu_timer.cpp mcu.cpp pcm.cpp
    rom_io.cpp rom.cpp standard_romsets.cpp submcu.cpp sha/sha224-256.c
)
list(TRANSFORM nuked_sources PREPEND "${nuked_sc55_SOURCE_DIR}/src/backend/")
add_library(fooyin_nuked_sc55 STATIC ${nuked_sources}
    "${nuked_sc55_SOURCE_DIR}/src/common/rom_loader.cpp"
    "${nuked_sc55_SOURCE_DIR}/src/common/term_io.cpp"
)
set_target_properties(fooyin_nuked_sc55 PROPERTIES
    POSITION_INDEPENDENT_CODE ON AUTOMOC OFF AUTOUIC OFF
    CXX_VISIBILITY_PRESET hidden C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES)
target_compile_features(fooyin_nuked_sc55 PUBLIC cxx_std_23)
target_include_directories(fooyin_nuked_sc55 SYSTEM PUBLIC
    "${nuked_sc55_SOURCE_DIR}/src"
    "${nuked_sc55_SOURCE_DIR}/src/backend"
    "${nuked_sc55_BINARY_DIR}/backend")
