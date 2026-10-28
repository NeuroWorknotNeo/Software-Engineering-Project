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

add_executable(bs_tests test_black_scholes.cpp)
target_link_libraries(bs_tests PRIVATE black_scholes GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(bs_tests)
