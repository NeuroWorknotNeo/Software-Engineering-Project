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

add_executable(hello_tests test_hello.cpp)
target_link_libraries(hello_tests PRIVATE hello GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(hello_tests)
