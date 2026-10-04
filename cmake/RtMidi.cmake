# SPDX-License-Identifier: GPL-3.0-or-later
# RtMidi retains its own license; see the submodule's LICENSE.
set(rtmidi_source_dir "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/rtmidi")
if(NOT EXISTS "${rtmidi_source_dir}/RtMidi.cpp")
    message(FATAL_ERROR "Bundled RtMidi sources are missing. Run: git submodule update --init --recursive")
endif()

# Build the library only, without upstream tools/install rules or automatic
# JACK detection. The plugin explicitly selects ALSA on Linux.
add_library(fooyin_rtmidi STATIC
    "${rtmidi_source_dir}/RtMidi.cpp"
    "${rtmidi_source_dir}/rtmidi_c.cpp")
set_target_properties(fooyin_rtmidi PROPERTIES
    POSITION_INDEPENDENT_CODE ON AUTOMOC OFF AUTOUIC OFF
    CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES)
target_compile_features(fooyin_rtmidi PUBLIC cxx_std_11)
target_include_directories(fooyin_rtmidi SYSTEM PUBLIC "${rtmidi_source_dir}")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(ALSA REQUIRED)
    find_package(Threads REQUIRED)
    target_compile_definitions(fooyin_rtmidi PRIVATE __LINUX_ALSA__)
    target_link_libraries(fooyin_rtmidi PRIVATE ALSA::ALSA Threads::Threads)
elseif(WIN32)
    target_compile_definitions(fooyin_rtmidi PRIVATE __WINDOWS_MM__)
    target_link_libraries(fooyin_rtmidi PRIVATE winmm)
elseif(APPLE)
    target_compile_definitions(fooyin_rtmidi PRIVATE __MACOSX_CORE__)
    foreach(framework CoreMIDI CoreAudio CoreFoundation CoreServices)
        find_library(rtmidi_${framework} NAMES ${framework} REQUIRED)
        target_link_libraries(fooyin_rtmidi PRIVATE "${rtmidi_${framework}}")
    endforeach()
else()
    message(FATAL_ERROR "Bundled RtMidi supports Linux, Windows and macOS; use system RtMidi on this platform")
endif()

include(GNUInstallDirs)
install(FILES "${rtmidi_source_dir}/LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/fooyin-plugin-midi/rtmidi")
