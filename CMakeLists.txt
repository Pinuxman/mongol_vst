cmake_minimum_required(VERSION 3.22)

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
if(APPLE AND NOT CMAKE_OSX_ARCHITECTURES)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

project(TENGRI VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(TENGRI_BUILD_TOOLS "Build the offline render / test tool" OFF)
set(TENGRI_JUCE_PATH "" CACHE PATH "Path to a local JUCE checkout (otherwise JUCE is downloaded)")

if(TENGRI_JUCE_PATH)
    add_subdirectory(${TENGRI_JUCE_PATH} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(TENGRI_SOURCES
    Source/Parameters.cpp
    Source/Presets.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/TengriLookAndFeel.cpp)

function(tengri_configure target)
    target_sources(${target} PRIVATE ${TENGRI_SOURCES})
    target_include_directories(${target} PRIVATE Source)
    if(MSVC)
        target_compile_options(${target} PRIVATE /utf-8)
    endif()
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

# Instrument: TENGRI (MIDI synth + optional audio input for the transformers)
juce_add_plugin(Tengri
    PRODUCT_NAME "TENGRI"
    COMPANY_NAME "Mongol Audio"
    BUNDLE_ID "com.mongolaudio.tengri"
    PLUGIN_MANUFACTURER_CODE Mngl
    PLUGIN_CODE Tngr
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    FORMATS VST3 AU Standalone)
tengri_configure(Tengri)

# Effect: TENGRI FX (voice / string transformer, MIDI still drives the synth layer)
juce_add_plugin(TengriFX
    PRODUCT_NAME "TENGRI FX"
    COMPANY_NAME "Mongol Audio"
    BUNDLE_ID "com.mongolaudio.tengrifx"
    PLUGIN_MANUFACTURER_CODE Mngl
    PLUGIN_CODE Tnfx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Modulation
    FORMATS VST3 AU Standalone)
tengri_configure(TengriFX)

if(TENGRI_BUILD_TOOLS)
    juce_add_console_app(TengriRender PRODUCT_NAME "TengriRender")
    target_sources(TengriRender PRIVATE Tools/Render.cpp)
    tengri_configure(TengriRender)
    target_compile_definitions(TengriRender PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JucePlugin_Name="TENGRI" JucePlugin_IsSynth=0)
    target_link_libraries(TengriRender PRIVATE juce::juce_gui_extra)
endif()
