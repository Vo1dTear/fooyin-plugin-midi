cmake_minimum_required(VERSION 3.14)

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

add_subdirectory(midiplugin)
