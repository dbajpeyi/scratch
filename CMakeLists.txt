cmake_minimum_required(VERSION 3.22)

project(CrateCut VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Universal binary (Apple Silicon + Intel) unless the caller says otherwise.
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")

option(CRATECUT_BUILD_TESTS "Build the offline render / snapshot test harness" OFF)

# ---------------------------------------------------------------------------
# JUCE: use a local checkout if JUCE_DIR is given, otherwise fetch it.
# ---------------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.4
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
set(CRATECUT_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND CRATECUT_FORMATS AU)
endif()

juce_add_plugin(CrateCut
    COMPANY_NAME                "CrateCut"
    BUNDLE_ID                   "com.cratecut.cratecut"
    PLUGIN_MANUFACTURER_CODE    Crcu
    PLUGIN_CODE                 Ccdk
    PRODUCT_NAME                "CrateCut"
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Sampler
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice
    FORMATS                     ${CRATECUT_FORMATS})

set(CRATECUT_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp
    Source/dsp/RecordData.cpp
    Source/dsp/DefaultSample.cpp
    Source/dsp/TurntableEngine.cpp
    Source/ui/CrateLookAndFeel.cpp
    Source/ui/PlatterView.cpp
    Source/ui/WaveformView.cpp)

target_sources(CrateCut PRIVATE ${CRATECUT_SOURCES})
target_include_directories(CrateCut PRIVATE Source)

target_compile_definitions(CrateCut PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(CrateCut
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Test harness (offline renders + UI snapshot). Not part of the plugin.
# ---------------------------------------------------------------------------
if(CRATECUT_BUILD_TESTS)
    juce_add_console_app(CrateCutTests PRODUCT_NAME "CrateCutTests")
    target_sources(CrateCutTests PRIVATE tests/OfflineTests.cpp ${CRATECUT_SOURCES})
    target_include_directories(CrateCutTests PRIVATE Source)
    target_compile_definitions(CrateCutTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JucePlugin_Name="CrateCut"
        JucePlugin_IsSynth=1
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        CRATECUT_TEST_BUILD=1)
    target_link_libraries(CrateCutTests PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
endif()
