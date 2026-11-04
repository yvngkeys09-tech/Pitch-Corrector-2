cmake_minimum_required(VERSION 3.21)
project(PitchCorrector VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 8.0.4
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(PitchCorrector
  PRODUCT_NAME "Pitch-Corrector"
  COMPANY_NAME "OpenAudio"
  PLUGIN_MANUFACTURER_CODE Opau
  PLUGIN_CODE Pcor
  FORMATS VST3
  IS_SYNTH FALSE
  NEEDS_MIDI_INPUT FALSE
  COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PitchCorrector PRIVATE
  src/PluginProcessor.cpp
  src/PluginEditor.cpp)

target_compile_definitions(PitchCorrector PUBLIC
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(PitchCorrector
  PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
  PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

option(PC_BUILD_TESTS "Build unit tests" ON)
if(PC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
