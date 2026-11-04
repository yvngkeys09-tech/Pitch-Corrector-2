include(FetchContent)

# Catch2: pinned release archive (same reasoning as JUCE above - git shallow
# clones under FetchContent fail intermittently on Windows).
set(JUCE_LOCAL_CATCH2 "" CACHE PATH "Local Catch2 source dir (optional)")
if(JUCE_LOCAL_CATCH2)
  add_subdirectory("${JUCE_LOCAL_CATCH2}" "${CMAKE_BINARY_DIR}/catch2-build")
else()
  FetchContent_Declare(Catch2
    URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.5.2.tar.gz
    URL_HASH SHA256=269543a49eb76f40b3f93ff231d4c24c27a7e16c90e47d2e45bcc564de470c6e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(Catch2)
endif()

add_executable(pc_tests test_yin.cpp test_quantizer.cpp test_shifter.cpp test_engine.cpp)
target_include_directories(pc_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(pc_tests PRIVATE Catch2::Catch2WithMain)

include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(pc_tests)
