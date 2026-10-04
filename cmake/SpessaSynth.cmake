# SPDX-License-Identifier: GPL-3.0-or-later
# SpessaSynth remains Apache-2.0 licensed; see the submodule's LICENSE.
set(spessa_source_dir "${CMAKE_CURRENT_LIST_DIR}/../3rdparty/spessasynth_core_c/spessasynth_core")
if(NOT EXISTS "${spessa_source_dir}/CMakeLists.txt")
    message(FATAL_ERROR "Bundled SpessaSynth sources are missing. Run: git submodule update --init --recursive")
endif()
if(CMAKE_VERSION VERSION_LESS 3.21)
    message(FATAL_ERROR "Bundled SpessaSynth requires CMake 3.21 or newer")
endif()

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
    find_package(ZLIB REQUIRED)
    if(MSVC)
        find_package(Vorbis CONFIG REQUIRED)
        find_package(FLAC CONFIG REQUIRED)
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SPESSA_VORBIS REQUIRED IMPORTED_TARGET vorbisfile)
        pkg_check_modules(SPESSA_FLAC REQUIRED IMPORTED_TARGET flac)
    endif()
    add_subdirectory("${spessa_source_dir}" "${CMAKE_CURRENT_BINARY_DIR}/spessasynth" EXCLUDE_FROM_ALL)
    set_target_properties(spessasynth PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden)
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
    else()
        target_link_libraries(spessasynth PRIVATE PkgConfig::SPESSA_VORBIS PkgConfig::SPESSA_FLAC)
    endif()
endfunction()
configure_bundled_spessasynth()

include(GNUInstallDirs)
install(FILES "${spessa_source_dir}/../LICENSE"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/fooyin-plugin-midi/spessasynth_core_c")
