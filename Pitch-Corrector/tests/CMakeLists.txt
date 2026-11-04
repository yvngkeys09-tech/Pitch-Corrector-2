include(FetchContent)
FetchContent_Declare(Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.5.2
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

add_executable(pc_tests test_yin.cpp test_quantizer.cpp test_shifter.cpp test_engine.cpp)
target_include_directories(pc_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(pc_tests PRIVATE Catch2::Catch2WithMain)

include(CTest)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)
catch_discover_tests(pc_tests)
