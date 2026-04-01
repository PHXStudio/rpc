################################################################
# bintalk
################################################################
cmake_minimum_required(VERSION 3.16)
project(rpc LANGUAGES CXX)
include(GNUInstallDirs)

# MSVC defaults to system code page; UTF-8 sources need /utf-8 to avoid mojibake in diagnostics and literals.
if(MSVC)
  add_compile_options(/utf-8)
endif()

set(CMAKE_DEBUG_POSTFIX "d")

# Test Endianess.
include(TestBigEndian)
test_big_endian(RPC_BIG_ENDIAN)

# The version number.
set(RPC_VERSION_MAJOR 1)
set(RPC_VERSION_MINOR 0)
# configure a header file to pass some of the CMake settings.
configure_file("Config.h.in" "Config.h")

# rapidjson dependency (header-only)
include(FetchContent)
FetchContent_Declare(
  rapidjson
  GIT_REPOSITORY https://github.com/Tencent/rapidjson.git
  GIT_TAG master
)
# Disable rapidjson's tests, examples, and docs
set(RAPIDJSON_BUILD_DOC OFF CACHE INTERNAL "")
set(RAPIDJSON_BUILD_EXAMPLES OFF CACHE INTERNAL "")
set(RAPIDJSON_BUILD_TESTS OFF CACHE INTERNAL "")
FetchContent_MakeAvailable(rapidjson)

# compiler.
add_subdirectory(compiler)
# runtime.
#add_subdirectory(runtime)

if(BUILD_TESTING)
  include(CTest)
  enable_testing()
  add_subdirectory(tests)
endif()
