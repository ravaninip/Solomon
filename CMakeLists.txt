cmake_minimum_required(VERSION 3.22)
project(Solomon VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SOLOMON_BUILD_PLUGIN "Build the JUCE plugin (downloads JUCE)" ON)
option(SOLOMON_BUILD_TESTS  "Build the engine tests" ON)
option(SOLOMON_BUILD_PLUGIN_TESTS "Host-less smoke test of the plugin processor and editor" OFF)
option(SOLOMON_COPY_AFTER_BUILD "Copy the plugin into the user's VST3/AU folder after building" OFF)
set(SOLOMON_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (empty = download from GitHub)")
set(SOLOMON_JUCE_TAG "8.0.15" CACHE STRING "JUCE tag to download")

# ---------------------------------------------------------------- third-party DSP (MIT)
# Oscillator models from the open-source Plaits code base and its stmlib helpers, vendored in
# third_party/mutable (see its README). stmlib selects its desktop code paths with the TEST macro,
# so every translation unit that includes these headers must define it consistently: that is the
# vendored sources and Source/Engine/PlaitsOscillator.cc only (no other file includes them).
file(STRINGS third_party/mutable/MANIFEST.txt SOLOMON_MUTABLE_FILES)
set(SOLOMON_MUTABLE_SOURCES "")
foreach(f IN LISTS SOLOMON_MUTABLE_FILES)
    if(f MATCHES "\\.cc$")
        list(APPEND SOLOMON_MUTABLE_SOURCES third_party/mutable/${f})
    endif()
endforeach()
add_library(solomon_mutable STATIC ${SOLOMON_MUTABLE_SOURCES})
target_include_directories(solomon_mutable PUBLIC third_party/mutable)
target_compile_definitions(solomon_mutable PRIVATE TEST)
set_target_properties(solomon_mutable PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(solomon_mutable PRIVATE /W0 /utf-8)
else()
    target_compile_options(solomon_mutable PRIVATE -w)
endif()

# ---------------------------------------------------------------- synth engine (no JUCE)
add_library(solomon_engine STATIC
    Source/Engine/PlaitsOscillator.cc
    Source/Engine/Resampler.cc
    Source/Engine/Synth.cc)
target_include_directories(solomon_engine PUBLIC Source)
target_link_libraries(solomon_engine PRIVATE solomon_mutable)
set_source_files_properties(Source/Engine/PlaitsOscillator.cc PROPERTIES COMPILE_DEFINITIONS TEST)
set_target_properties(solomon_engine PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(solomon_engine PRIVATE /utf-8)
endif()

if(SOLOMON_BUILD_TESTS)
    enable_testing()
    add_executable(solomon_tests Tests/engine_tests.cpp)
    target_link_libraries(solomon_tests PRIVATE solomon_engine)
    if(MSVC)
        target_compile_options(solomon_tests PRIVATE /utf-8)
    endif()
    add_test(NAME engine COMMAND solomon_tests)
endif()

# ---------------------------------------------------------------- plugin
if(SOLOMON_BUILD_PLUGIN)
    if(SOLOMON_JUCE_PATH)
        add_subdirectory(${SOLOMON_JUCE_PATH} JUCE EXCLUDE_FROM_ALL)
    else()
        include(FetchContent)
        FetchContent_Declare(JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG ${SOLOMON_JUCE_TAG}
            GIT_SHALLOW TRUE)
        FetchContent_MakeAvailable(JUCE)
    endif()

    set(SOLOMON_FORMATS VST3)
    if(APPLE)
        list(APPEND SOLOMON_FORMATS AU)
    endif()

    # PLUGIN_MANUFACTURER_CODE and PLUGIN_CODE identify the plugin in saved host sessions:
    # never change them after the first public release.
    juce_add_plugin(Solomon
        COMPANY_NAME "Paolo"
        PLUGIN_MANUFACTURER_CODE Paol
        PLUGIN_CODE Solo
        PRODUCT_NAME "Solomon"
        FORMATS ${SOLOMON_FORMATS}
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        VST3_CATEGORIES Instrument Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        COPY_PLUGIN_AFTER_BUILD ${SOLOMON_COPY_AFTER_BUILD})

    target_sources(Solomon PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/Presets.cpp)

    # no network, no browser, no JUCE splash/telemetry
    target_compile_definitions(Solomon PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)

    if(MSVC)
        target_compile_options(Solomon PRIVATE /utf-8)
    endif()

    target_link_libraries(Solomon
        PRIVATE
            solomon_engine
            juce::juce_audio_basics
            juce::juce_audio_processors
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    if(SOLOMON_BUILD_PLUGIN_TESTS)
        enable_testing()
        add_executable(solomon_plugin_smoke
            Tests/plugin_smoke.cpp
            Source/PluginProcessor.cpp
            Source/PluginEditor.cpp
            Source/Presets.cpp)
        target_include_directories(solomon_plugin_smoke PRIVATE Source)
        target_compile_definitions(solomon_plugin_smoke PRIVATE
            JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_STANDALONE_APPLICATION=1)
        target_link_libraries(solomon_plugin_smoke PRIVATE
            solomon_engine
            juce::juce_audio_basics
            juce::juce_audio_processors
            juce::juce_gui_basics
            juce::juce_recommended_config_flags)
        if(MSVC)
            target_compile_options(solomon_plugin_smoke PRIVATE /utf-8)
        endif()
        add_test(NAME plugin_smoke COMMAND solomon_plugin_smoke)
    endif()
endif()
