cmake_minimum_required(VERSION 3.22)
project(PhysicalModelGuitar VERSION 0.4.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    add_compile_options(/utf-8)
endif()
include(CTest)
add_library(PMGDSP STATIC DSP/StringEngine.cpp)
target_include_directories(PMGDSP PUBLIC DSP)
if(MSVC)
    target_compile_options(PMGDSP PRIVATE /W4)
else()
    target_compile_options(PMGDSP PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(BUILD_TESTING)
    add_executable(PMGDSPTests Tests/RealismTests.cpp)
    target_link_libraries(PMGDSPTests PRIVATE PMGDSP)
    foreach(case basic physics interaction stability realtime)
        add_test(NAME PMG04_${case} COMMAND PMGDSPTests ${case})
    endforeach()
endif()
add_executable(PMGOfflineRender tools/OfflineRender.cpp)
target_link_libraries(PMGOfflineRender PRIVATE PMGDSP)
option(PMG_BUILD_PLUGIN "Build the JUCE VST3 plugin (downloads JUCE on first configure)" OFF)
if(PMG_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.4 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
    juce_add_plugin(PhysicalModelGuitar
        COMPANY_NAME "Personal Utility Lab"
        BUNDLE_ID "org.personalutilitylab.physicalmodelguitar.realism2"
        PLUGIN_MANUFACTURER_CODE PmLu
        PLUGIN_CODE PmG4
        PRODUCT_NAME "Physical Model Guitar 0.4"
        DESCRIPTION "Six-string coupled waveguide guitar with nonlinear contacts"
        IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE COPY_PLUGIN_AFTER_BUILD FALSE
        FORMATS VST3 Standalone VST3_CATEGORIES Instrument)
    juce_generate_juce_header(PhysicalModelGuitar)
    target_sources(PhysicalModelGuitar PRIVATE Source/UpgradedProcessor.cpp Source/UpgradedEditor.cpp)
    target_compile_features(PhysicalModelGuitar PRIVATE cxx_std_17)
    target_compile_definitions(PhysicalModelGuitar PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)
    target_link_libraries(PhysicalModelGuitar PRIVATE PMGDSP juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()
