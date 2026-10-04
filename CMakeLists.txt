cmake_minimum_required(VERSION 3.20)

project(
    midiinput
    VERSION 0.1.1
    DESCRIPTION "MIDI plugin for fooyin, modified by Vo1dTear"
    HOMEPAGE_URL "https://github.com/Vo1dTear/fooyin-plugin-midi"
    LANGUAGES C CXX
)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)

find_package(Fooyin)

option(MIDI_USE_BUNDLED_SPESSASYNTH "Link SpessaSynth statically from the local submodule" OFF)
if(MIDI_USE_BUNDLED_SPESSASYNTH)
    include(cmake/SpessaSynth.cmake)
endif()

option(MIDI_ENABLE_NUKED_SC55 "Build the Nuked-SC55 synthesis engine" ON)
if(MIDI_ENABLE_NUKED_SC55)
    include(cmake/NukedSC55.cmake)
endif()

option(MIDI_ENABLE_EXTERNAL "Build external MIDI application output (RtMidi)" ON)
option(MIDI_USE_BUNDLED_RTMIDI "Link RtMidi statically from the local submodule" OFF)
if(MIDI_ENABLE_EXTERNAL)
    add_library(midi_rtmidi INTERFACE)
    if(MIDI_USE_BUNDLED_RTMIDI)
        include(cmake/RtMidi.cmake)
        target_link_libraries(midi_rtmidi INTERFACE fooyin_rtmidi)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(RTMIDI REQUIRED IMPORTED_TARGET rtmidi)
        target_link_libraries(midi_rtmidi INTERFACE PkgConfig::RTMIDI)
    endif()
endif()

add_subdirectory(midiplugin)

option(MIDI_BUILD_TESTS "Build MIDI player regression tests" OFF)
if(MIDI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
