cmake_minimum_required(VERSION 3.24)

project(PitchTimePro VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build for Intel + Apple Silicon" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment target" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
    juce
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.7
)

FetchContent_MakeAvailable(juce)

FetchContent_Declare(
    signalsmith_stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG 57b93f4e9206a089a45387eaa39bdc9f310d3308
)

FetchContent_MakeAvailable(signalsmith_stretch)

option(FFORM_BUILD_AAX "Build the AAX plugin" OFF)
option(FFORM_BUILD_QA_HARNESS "Build the headless DSP QA renderer" OFF)
option(FFORM_BUILD_OFFLINE_HARNESS "Build the standalone offline renderer" OFF)
set(FFORM_AAX_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/vendor/aax-sdk-2-9-0" CACHE PATH
    "Path to the AAX SDK")

set(FFORM_FORMATS VST3 AU)

if(FFORM_BUILD_AAX)
    if(NOT EXISTS "${FFORM_AAX_SDK_PATH}/Interfaces/ACF")
        message(FATAL_ERROR
            "AAX SDK not found. Set FFORM_AAX_SDK_PATH or configure with -DFFORM_BUILD_AAX=OFF.")
    endif()

    juce_set_aax_sdk_path ("${FFORM_AAX_SDK_PATH}")
    list(APPEND FFORM_FORMATS AAX)
endif()

juce_add_plugin(PitchTimePro
    COMPANY_NAME "Guerra Magica Audio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_CODE "FFor"
    FORMATS ${FFORM_FORMATS}
    PRODUCT_NAME "F-Form"
    BUNDLE_ID "com.guerramagica.f-form"
)

juce_generate_juce_header(PitchTimePro)

target_sources(PitchTimePro
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/PluginEditor.cpp
        src/PluginEditor.h
        src/TimeStretchEngine.h
)

target_compile_definitions(PitchTimePro
    PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(PitchTimePro
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_gui_extra
        juce::juce_gui_basics
        juce::juce_dsp
        signalsmith-stretch
)

target_include_directories(PitchTimePro
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(PitchTimePro PROPERTIES
    XCODE_ATTRIBUTE_DEVELOPMENT_TEAM ""
)

if(FFORM_BUILD_QA_HARNESS)
    add_executable(fform-qa-renderer
        tools/fform-qa/main.cpp
        src/TimeStretchEngine.h
    )

    target_include_directories(fform-qa-renderer
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/include
            ${CMAKE_CURRENT_BINARY_DIR}/PitchTimePro_artefacts/JuceLibraryCode
    )

    target_compile_definitions(fform-qa-renderer
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )

    target_link_libraries(fform-qa-renderer
        PRIVATE
            juce::juce_audio_formats
            juce::juce_audio_basics
            juce::juce_dsp
            juce::juce_graphics
            signalsmith-stretch
    )
endif()

if(FFORM_BUILD_OFFLINE_HARNESS)
    add_executable(fform-offline-renderer
        tools/fform-offline/main.cpp
        src/dsp/OfflineStretchEngine.h
    )

    target_include_directories(fform-offline-renderer
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/src/dsp
            ${CMAKE_CURRENT_BINARY_DIR}/PitchTimePro_artefacts/JuceLibraryCode
    )

    target_compile_definitions(fform-offline-renderer
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
    )

    target_link_libraries(fform-offline-renderer
        PRIVATE
            juce::juce_audio_formats
            juce::juce_audio_basics
            juce::juce_dsp
            juce::juce_graphics
            signalsmith-stretch
    )
endif()
