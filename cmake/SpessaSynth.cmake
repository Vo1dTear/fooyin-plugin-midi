# SPDX-License-Identifier: GPL-3.0-or-later
# SpessaSynth remains Apache-2.0 licensed; see the submodule's LICENSE.
set(spessa_source_dir "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/spessasynth_core_c/spessasynth_core")
if(NOT EXISTS "${spessa_source_dir}/CMakeLists.txt")
    message(FATAL_ERROR "Bundled SpessaSynth sources are missing. Run: git submodule update --init --recursive")
endif()
if(CMAKE_VERSION VERSION_LESS 3.21)
    message(FATAL_ERROR "Bundled SpessaSynth requires CMake 3.21 or newer")
endif()

set(MIDI_STATIC_CODEC_DIR "" CACHE PATH "Static codec installation prefix (PIC on Linux, /MD on Windows)")

function(configure_bundled_spessasynth)
    # Scope these settings to the dependency; leave the plugin and other
    # dependencies' build options untouched. Exclude upstream install rules.
    set(SS_BUILD_SHARED OFF)
    set(SS_BUILD_EXAMPLES OFF)
    set(SS_ENABLE_SF3_VORBIS ON)
    set(SS_ENABLE_SF3_FLAC ON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_AUTOMOC OFF)
    set(CMAKE_AUTOUIC OFF)

    # Require codecs rather than silently losing compressed bank/XMF support.
    if(MIDI_STATIC_CODEC_DIR)
        # Do not let upstream discover shared codecs from the host installation.
        set(SS_ENABLE_SF3_VORBIS OFF)
        set(SS_ENABLE_SF3_FLAC OFF)
        set(CMAKE_DISABLE_FIND_PACKAGE_ZLIB TRUE)
        set(CMAKE_FIND_LIBRARY_SUFFIXES "${CMAKE_STATIC_LIBRARY_SUFFIX}")
        foreach(codec FLAC vorbisfile vorbis ogg zlib)
            if(codec STREQUAL "zlib")
                set(names z zlib zlibstatic)
            else()
                set(names "${codec}" "lib${codec}")
            endif()
            find_library(midi_static_${codec} NAMES ${names}
                PATHS "${MIDI_STATIC_CODEC_DIR}/lib" NO_DEFAULT_PATH NO_CACHE REQUIRED)
            list(APPEND codec_libraries "${midi_static_${codec}}")
        endforeach()
        foreach(header FLAC/stream_decoder.h vorbis/vorbisfile.h ogg/ogg.h zlib.h)
            if(NOT EXISTS "${MIDI_STATIC_CODEC_DIR}/include/${header}")
                message(FATAL_ERROR "Missing static codec header: ${header}")
            endif()
        endforeach()
    else()
        find_package(ZLIB REQUIRED)
        if(MSVC)
            find_package(Vorbis CONFIG REQUIRED)
            find_package(FLAC CONFIG REQUIRED)
        else()
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(SPESSA_VORBIS REQUIRED IMPORTED_TARGET vorbisfile)
            pkg_check_modules(SPESSA_FLAC REQUIRED IMPORTED_TARGET flac)
        endif()
    endif()
    add_subdirectory("${spessa_source_dir}" "${CMAKE_CURRENT_BINARY_DIR}/spessasynth" EXCLUDE_FROM_ALL)
    set_target_properties(spessasynth PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden)
    if(MIDI_STATIC_CODEC_DIR)
        target_sources(spessasynth PRIVATE
            "${spessa_source_dir}/src/soundbank/vorbis_decode.c"
            "${spessa_source_dir}/src/soundbank/flac_decode.c")
        target_include_directories(spessasynth BEFORE PRIVATE "${MIDI_STATIC_CODEC_DIR}/include")
        target_compile_definitions(spessasynth PRIVATE
            SS_HAVE_LIBVORBISFILE=1 SS_HAVE_LIBFLAC=1 SS_HAVE_ZLIB=1 FLAC__NO_DLL)
        find_package(Threads REQUIRED)
        target_link_libraries(spessasynth PRIVATE ${codec_libraries} Threads::Threads)
    endif()
    if(MSVC)
        # Upstream headers include this file for MSVC, but upstream currently
        # generates it only for shared builds. Generate the static variant too.
        include(GenerateExportHeader)
        generate_export_header(spessasynth
            BASE_NAME spessasynth
            EXPORT_MACRO_NAME SPESSASYNTH_EXPORTS
            EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/spessasynth/spessasynth_exports.h"
            STATIC_DEFINE SHARED_EXPORTS_BUILT_AS_STATIC)
        target_include_directories(spessasynth PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/spessasynth")
        target_compile_definitions(spessasynth PUBLIC SHARED_EXPORTS_BUILT_AS_STATIC)
    elseif(NOT MIDI_STATIC_CODEC_DIR)
        target_link_libraries(spessasynth PRIVATE PkgConfig::SPESSA_VORBIS PkgConfig::SPESSA_FLAC)
    endif()
endfunction()
configure_bundled_spessasynth()

include(GNUInstallDirs)
install(FILES "${spessa_source_dir}/../LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/fooyin-plugin-midi/spessasynth_core_c")
