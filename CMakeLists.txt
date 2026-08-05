cmake_minimum_required(VERSION 3.21)

project(Synth
    VERSION 0.1.0
    DESCRIPTION "Portable C synth engine with a desktop audio and MIDI host"
    LANGUAGES C)

include(CTest)

option(SYNTH_BUILD_DESKTOP "Build the desktop synth host and desktop tools" ON)

function(synth_configure_c_target target)
    set_target_properties(${target} PROPERTIES
        C_STANDARD 99
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO)

    if(MSVC)
        target_compile_options(${target} PRIVATE /W4)
        target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra)
    endif()
endfunction()

set(SYNTH_CORE_SOURCES
    src/midi_types.c
    src/wavetable.c
    src/oscillator.c
    src/envelope.c
    src/voice.c
    src/filter.c
    src/effects/saturation.c
    src/effects/distortion.c
    src/effects/bitcrusher.c
    src/effects/delay.c
    src/effects/effect_chain.c
    src/synth.c
    src/lfo.c)

set(SYNTH_PUBLIC_HEADERS
    include/synth/audio_types.h
    include/synth/bitcrusher.h
    include/synth/delay.h
    include/synth/distortion.h
    include/synth/effect_chain.h
    include/synth/envelope.h
    include/synth/filter.h
    include/synth/lfo.h
    include/synth/midi_types.h
    include/synth/oscillator.h
    include/synth/saturation.h
    include/synth/synth.h
    include/synth/synth_config.h
    include/synth/voice.h)

set(SYNTH_PRIVATE_HEADERS
    src/internal/synth_internal.h
    src/internal/wavetable.h)

add_library(synth_core STATIC
    ${SYNTH_CORE_SOURCES}
    ${SYNTH_PUBLIC_HEADERS}
    ${SYNTH_PRIVATE_HEADERS})
add_library(Synth::Core ALIAS synth_core)
synth_configure_c_target(synth_core)
target_include_directories(synth_core
    PUBLIC
        "${PROJECT_SOURCE_DIR}/include"
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src")

if(NOT MSVC)
    target_link_libraries(synth_core PUBLIC m)
endif()

source_group(TREE "${PROJECT_SOURCE_DIR}" FILES
    ${SYNTH_CORE_SOURCES}
    ${SYNTH_PUBLIC_HEADERS}
    ${SYNTH_PRIVATE_HEADERS})

if(SYNTH_BUILD_DESKTOP)
    find_package(Threads REQUIRED)

    add_library(synth_miniaudio INTERFACE)
    add_library(Synth::Miniaudio ALIAS synth_miniaudio)
    target_include_directories(synth_miniaudio SYSTEM INTERFACE
        "${PROJECT_SOURCE_DIR}/third_party")

    add_library(synth_desktop_system STATIC
        platform/desktop/system/desktop_system.c
        platform/desktop/system/desktop_system.h)
    add_library(Synth::DesktopSystem ALIAS synth_desktop_system)
    synth_configure_c_target(synth_desktop_system)
    target_include_directories(synth_desktop_system PUBLIC
        "${PROJECT_SOURCE_DIR}/platform/desktop")

    add_library(synth_desktop_midi STATIC
        platform/desktop/midi/chord_mode.c
        platform/desktop/midi/chord_mode.h
        platform/desktop/midi/midi_mapping.c
        platform/desktop/midi/midi_mapping.h
        platform/desktop/midi/midi_device.h
        platform/desktop/midi/midi_portmidi.c
        platform/desktop/midi/midi_portmidi.h)
    add_library(Synth::DesktopMidi ALIAS synth_desktop_midi)
    synth_configure_c_target(synth_desktop_midi)
    target_include_directories(synth_desktop_midi PUBLIC
        "${PROJECT_SOURCE_DIR}/platform/desktop")
    target_link_libraries(synth_desktop_midi
        PUBLIC
            Synth::Core
        PRIVATE
            ${CMAKE_DL_LIBS})

    add_library(synth_desktop_audio STATIC
        platform/desktop/audio/audio_miniaudio.c
        platform/desktop/audio/audio_miniaudio.h
        platform/desktop/audio/audio_device.h)
    add_library(Synth::DesktopAudio ALIAS synth_desktop_audio)
    synth_configure_c_target(synth_desktop_audio)
    target_include_directories(synth_desktop_audio PUBLIC
        "${PROJECT_SOURCE_DIR}/platform/desktop")
    target_link_libraries(synth_desktop_audio
        PUBLIC
            Synth::Miniaudio
            Threads::Threads)

    if(APPLE)
        target_link_libraries(synth_desktop_audio PRIVATE
            "-framework CoreFoundation"
            "-framework CoreAudio"
            "-framework AudioToolbox"
            "-framework AudioUnit")
    elseif(UNIX)
        target_link_libraries(synth_desktop_audio PRIVATE ${CMAKE_DL_LIBS})
    endif()

    add_executable(synth platform/desktop/main.c)
    synth_configure_c_target(synth)
    target_link_libraries(synth PRIVATE
        Synth::DesktopAudio
        Synth::DesktopMidi
        Synth::DesktopSystem)

    add_executable(midi_monitor tools/midi_monitor.c)
    synth_configure_c_target(midi_monitor)
    target_link_libraries(midi_monitor PRIVATE
        Synth::DesktopMidi
        Synth::DesktopSystem)

    add_custom_target(run_synth
        COMMAND "$<TARGET_FILE:synth>"
        DEPENDS synth
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Running desktop synth from the repository root")

    add_custom_target(run_midi_monitor
        COMMAND "$<TARGET_FILE:midi_monitor>"
        DEPENDS midi_monitor
        WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
        USES_TERMINAL
        COMMENT "Running MIDI monitor from the repository root")

    source_group(TREE "${PROJECT_SOURCE_DIR}" FILES
        platform/desktop/main.c
        platform/desktop/audio/audio_miniaudio.c
        platform/desktop/audio/audio_miniaudio.h
        platform/desktop/audio/audio_device.h
        platform/desktop/midi/chord_mode.c
        platform/desktop/midi/chord_mode.h
        platform/desktop/midi/midi_mapping.c
        platform/desktop/midi/midi_mapping.h
        platform/desktop/midi/midi_device.h
        platform/desktop/midi/midi_portmidi.c
        platform/desktop/midi/midi_portmidi.h
        platform/desktop/system/desktop_system.c
        platform/desktop/system/desktop_system.h
        tools/midi_monitor.c)
endif()

if(BUILD_TESTING)
    set(SYNTH_CORE_TESTS
        test_oscillator
        test_envelope
        test_filter
        test_distortion
        test_saturation
        test_bitcrusher
        test_delay
        test_lfo
        test_voice
        test_midi_types)

    foreach(test_name IN LISTS SYNTH_CORE_TESTS)
        add_executable(${test_name} "tests/${test_name}.c")
        synth_configure_c_target(${test_name})
        target_link_libraries(${test_name} PRIVATE Synth::Core)
        add_test(NAME ${test_name}
            COMMAND "$<TARGET_FILE:${test_name}>"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    endforeach()

    if(SYNTH_BUILD_DESKTOP)
        add_executable(test_midi_mapping tests/test_midi_mapping.c)
        synth_configure_c_target(test_midi_mapping)
        target_link_libraries(test_midi_mapping PRIVATE Synth::DesktopMidi)
        add_test(NAME test_midi_mapping
            COMMAND "$<TARGET_FILE:test_midi_mapping>"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")

        add_executable(test_chord_mode tests/test_chord_mode.c)
        synth_configure_c_target(test_chord_mode)
        target_link_libraries(test_chord_mode PRIVATE Synth::DesktopMidi)
        add_test(NAME test_chord_mode
            COMMAND "$<TARGET_FILE:test_chord_mode>"
            WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}")
    endif()
endif()
