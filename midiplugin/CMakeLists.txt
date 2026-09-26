cmake_minimum_required(VERSION 3.14)

project(
    midiplugin
    VERSION 0.1.1
    DESCRIPTION "MIDI plugin for fooyin, modified by Vo1dTear"
    HOMEPAGE_URL "https://github.com/Vo1dTear/fooyin-plugin-midi"
    LANGUAGES C CXX
)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

create_fooyin_plugin(
    midiplugin
    DEPENDS Fooyin::Gui
    PRIVATE spessasynth
    SOURCES midiinputplugin.cpp
            midiinputplugin.h
            midiinput.cpp
            midiinput.h
            midiinputdefs.h
            midiinputsettings.cpp
            midiinputsettings.h
            SpessaPlayer.cpp
            SpessaPlayer.h
            MIDIPlayer.cpp
            MIDIPlayer.h
)

if(MIDI_ENABLE_NUKED_SC55)
    target_sources(midiplugin PRIVATE NukedSC55Player.cpp NukedSC55Player.h)
    target_link_libraries(midiplugin PRIVATE fooyin_nuked_sc55)
    target_compile_definitions(midiplugin PRIVATE MIDI_ENABLE_NUKED_SC55)
endif()

if(MIDI_ENABLE_EXTERNAL)
    target_sources(midiplugin PRIVATE
        ExternalMIDIPlayer.cpp ExternalMIDIPlayer.h
        ExternalMIDIPort.cpp ExternalMIDIPort.h
        ExternalMIDIController.cpp ExternalMIDIController.h)
    target_link_libraries(midiplugin PRIVATE PkgConfig::RTMIDI)
    target_compile_definitions(midiplugin PRIVATE MIDI_ENABLE_EXTERNAL)
endif()
