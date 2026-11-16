cmake_minimum_required(VERSION 3.22)

project(FrancEq
    VERSION 1.0.0
    DESCRIPTION "FrancEq parametric EQ audio plugin"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

# Optional local JUCE checkout. When empty, JUCE 8 is fetched from GitHub.
set(FRANCEQ_JUCE_PATH "" CACHE PATH
    "Path to a local JUCE checkout. When set, it is added with add_subdirectory instead of FetchContent.")

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_WEB_BROWSER OFF CACHE BOOL "Disable the JUCE web browser (and its Linux webkit dependency)" FORCE)
set(JUCE_USE_CURL OFF CACHE BOOL "Disable JUCE curl support" FORCE)

if(FRANCEQ_JUCE_PATH)
    message(STATUS "FrancEq: using local JUCE from ${FRANCEQ_JUCE_PATH}")
    add_subdirectory("${FRANCEQ_JUCE_PATH}" "${CMAKE_BINARY_DIR}/juce-local")
else()
    message(STATUS "FrancEq: fetching JUCE 8.0.15")
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(FrancEq
    COMPANY_NAME "Franc Audio"
    BUNDLE_ID com.francaudio.franceq
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Frnc
    PLUGIN_CODE FrEq
    FORMATS VST3 Standalone
    PRODUCT_NAME "FrancEq"
    VST3_CATEGORIES "EQ" "Dynamics" "Analyzer" "Fx")

target_sources(FrancEq
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h
        Source/EqDisplay.cpp
        Source/EqDisplay.h
        Source/EqDesign.h
        Source/Palette.h)

target_compile_features(FrancEq PRIVATE cxx_std_20)
set_target_properties(FrancEq PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS OFF)

target_compile_definitions(FrancEq
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)

target_link_libraries(FrancEq
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

option(FRANCEQ_BUILD_TESTS "Build the offline FrancEq sanity test" ON)

if(FRANCEQ_BUILD_TESTS)
    juce_add_console_app(FrancEqSanity PRODUCT_NAME "FrancEqSanity")
    target_sources(FrancEqSanity
        PRIVATE
            tests/Sanity.cpp
            Source/PluginProcessor.cpp
            Source/PluginProcessor.h
            Source/PluginEditor.cpp
            Source/PluginEditor.h
            Source/EqDisplay.cpp
            Source/EqDisplay.h
            Source/EqDesign.h
            Source/Palette.h)
    target_include_directories(FrancEqSanity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/Source)
    target_compile_features(FrancEqSanity PRIVATE cxx_std_20)
    target_compile_definitions(FrancEqSanity
        PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
            JUCE_DISPLAY_SPLASH_SCREEN=0
            JUCE_REPORT_APP_USAGE=0)
    target_link_libraries(FrancEqSanity
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
