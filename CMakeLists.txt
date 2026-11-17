cmake_minimum_required(VERSION 3.22)

# Mac: binario universal (Apple Silicon + Intel)
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "" FORCE)
endif()

project(ArtisVoxOne VERSION 1.0.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE: carpeta ./JUCE (git submodule o clon) o descarga automatica
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(ArtisVoxOne
    COMPANY_NAME "ArtiVox"
    COMPANY_WEBSITE "https://artivoxaudio.com"
    BUNDLE_ID "com.artivox.artisvoxone"
    PLUGIN_MANUFACTURER_CODE Artv
    PLUGIN_CODE AvOn
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "ArtisVox One"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

juce_add_binary_data(ArtisVoxOneData SOURCES
    Resources/logo.png
    Resources/fonts/Inter-Regular.otf
    Resources/fonts/Inter-Medium.otf
    Resources/fonts/Inter-SemiBold.otf
    Resources/fonts/Inter-Bold.otf
    Resources/fonts/Inter-ExtraBold.otf
    Resources/fonts/JetBrainsMono-Medium.ttf
    Resources/fonts/JetBrainsMono-Bold.ttf)

target_sources(ArtisVoxOne PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(ArtisVoxOne PRIVATE Source)

if(MSVC)
    target_compile_options(ArtisVoxOne PRIVATE /utf-8)
endif()

target_compile_definitions(ArtisVoxOne PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(ArtisVoxOne
    PRIVATE
        ArtisVoxOneData
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Pruebas offline (opcional): cmake -DARTISVOX_TESTS=ON
option(ARTISVOX_TESTS "Compilar pruebas offline" OFF)
if(ARTISVOX_TESTS)
    juce_add_console_app(ArtisVoxOneTests PRODUCT_NAME "ArtisVoxOneTests")
    target_sources(ArtisVoxOneTests PRIVATE Tests/TestMain.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(ArtisVoxOneTests PRIVATE Source)
    target_compile_definitions(ArtisVoxOneTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="ArtisVox One")
    target_link_libraries(ArtisVoxOneTests PRIVATE ArtisVoxOneData juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
