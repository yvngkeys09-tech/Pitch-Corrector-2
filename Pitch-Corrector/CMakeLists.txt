cmake_minimum_required(VERSION 3.21)
project(PitchCorrector VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# ---------------------------------------------------------------------------
# JUCE dependency.
#
# The previous setup (GIT_REPOSITORY + GIT_SHALLOW) failed with
#   "FetchContent.cmake ... Build step for juce failed: 1"
# because a shallow git clone of JUCE combined with FetchContent's
# UPDATE_DISCONNECTED / subbuild logic is fragile on Windows (long paths,
# antivirus locks, git grafts).  We now support three robust modes:
#
#   1. -DJUCE_SOURCE_DIR=/path/to/JUCE      -> offline, uses a local checkout
#   2. -DFETCHCONTENT_SOURCE_DIR_JUCE=...   -> same, via standard mechanism
#   3. default                              -> pinned release tarball (no git)
# ---------------------------------------------------------------------------
set(JUCE_SOURCE_DIR "" CACHE PATH
    "Path to an existing JUCE 8.x source tree (skips the download when set)")

if(JUCE_SOURCE_DIR)
  message(STATUS "PitchCorrector: using local JUCE at ${JUCE_SOURCE_DIR}")
  add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/juce-build")
else()
  FetchContent_Declare(JUCE
    URL      https://github.com/juce-framework/JUCE/archive/refs/tags/8.0.4.tar.gz
    URL_HASH SHA256=0effe9823a6fd8a4f98e5c08be7d308350e417b2d15dcf34011e778fefbbf005
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

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
