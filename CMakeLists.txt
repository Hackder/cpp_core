cmake_minimum_required(VERSION 3.14)
project(satsolver)

# GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/d122c0d435a6d305cdd50526127c84a98b77d87b.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Allow dynamic build type selection
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE ReleaseSafe CACHE STRING "Choose the build type: Debug, ReleaseSafe, ReleaseFast" FORCE)
endif()

# Define custom build types
set(CMAKE_CXX_FLAGS_RELEASESAFE "-O3")
set(CMAKE_CXX_FLAGS_RELEASEFAST "-O3 -DNDEBUG -DNO_ASSERTS -march=native -ffast-math")

if(PROFILE)
  message(STATUS "Enable profiling flags")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
  set(CMAKE_CXX_FLAGS_RELEASESAFE "${CMAKE_CXX_FLAGS_RELEASESAFE} -g -finline-functions")
  set(CMAKE_CXX_FLAGS_RELEASEFAST "${CMAKE_CXX_FLAGS_RELEASEFAST} -g -finline-functions")
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Wuninitialized -Wno-gnu-zero-variadic-macro-arguments)

set(SOURCE_FILES
  ./core.hpp
)

add_executable(
  core_test
  ${SOURCE_FILES}
  ./core_test.cpp
)
target_compile_options(core_test PRIVATE)

target_link_libraries(
  core_test
  GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(core_test)


