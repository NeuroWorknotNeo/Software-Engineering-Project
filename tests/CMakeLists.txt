# GoogleTest: берём системный, если установлен, иначе скачиваем.
find_package(GTest QUIET)
if(NOT GTest_FOUND)
  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

add_executable(mpt_tests test_data.cpp test_stats.cpp test_optim.cpp test_io.cpp)
target_link_libraries(mpt_tests PRIVATE mpt_core GTest::gtest_main)
target_compile_definitions(mpt_tests PRIVATE
  MPT_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/data")

include(GoogleTest)
gtest_discover_tests(mpt_tests)
