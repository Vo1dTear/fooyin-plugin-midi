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

option(MIDI_ENABLE_NUKED_SC55 "Build the Nuked-SC55 synthesis engine" ON)
if(MIDI_ENABLE_NUKED_SC55)
    include(cmake/NukedSC55.cmake)
endif()

option(MIDI_ENABLE_EXTERNAL "Build external MIDI application output (RtMidi)" ON)
if(MIDI_ENABLE_EXTERNAL)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(RTMIDI REQUIRED IMPORTED_TARGET rtmidi)
endif()

add_subdirectory(midiplugin)

option(MIDI_BUILD_TESTS "Build MIDI player regression tests" OFF)
if(MIDI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
