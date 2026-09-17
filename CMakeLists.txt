
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

# Build identity: which commit this binary was built from.
#
# A per-commit release is only useful if the artifact can say where it came
# from. Both values below are overridable from the command line, and that path
# is not a convenience: .dockerignore excludes .git/, so a container build has
# no git metadata at all and would otherwise report "unknown". CI and
# scripts/docker-build*.sh always inject them.
#
# The git fallback needs FULL history -- rev-list --count returns 1 in a shallow
# clone, which silently yields "1.0.1-<sha>". CI checkouts must use fetch-depth: 0.
if(NOT RPC_VERSION_STRING OR NOT RPC_BUILD_COMMIT)
  set(_rpc_sha "")
  set(_rpc_tag "")
  set(_rpc_count "")
  set(_rpc_dirty_suffix "")

  find_package(Git QUIET)
  if(GIT_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --short=7 HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE _rpc_sha OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET RESULT_VARIABLE _rpc_rc)
    if(NOT _rpc_rc EQUAL 0)
      set(_rpc_sha "")
    endif()
  endif()

  if(_rpc_sha)
    # --match keeps the rolling "latest" release tag out of the result: that tag
    # always points at the tip of main, so an unqualified describe --exact-match
    # would report the literal string "latest" for exactly the commits we care
    # about most.
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" describe --tags --exact-match --match "v[0-9]*" HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE _rpc_tag OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-list --count HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE _rpc_count OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    # A binary built from a modified tree must not claim a clean commit.
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" status --porcelain
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE _rpc_status OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_rpc_status)
      set(_rpc_dirty_suffix "-dirty")
    endif()
  endif()

  if(NOT RPC_BUILD_COMMIT)
    if(_rpc_sha)
      set(RPC_BUILD_COMMIT "${_rpc_sha}")
    else()
      set(RPC_BUILD_COMMIT "unknown")
    endif()
  endif()

  if(NOT RPC_VERSION_STRING)
    if(_rpc_tag)
      set(RPC_VERSION_STRING "${_rpc_tag}${_rpc_dirty_suffix}")
    elseif(_rpc_sha)
      set(RPC_VERSION_STRING
          "${RPC_VERSION_MAJOR}.${RPC_VERSION_MINOR}.${_rpc_count}-${_rpc_sha}${_rpc_dirty_suffix}")
    else()
      set(RPC_VERSION_STRING "${RPC_VERSION_MAJOR}.${RPC_VERSION_MINOR}.0-unknown")
    endif()
  endif()
endif()

message(STATUS "rpc ${RPC_VERSION_STRING} (commit ${RPC_BUILD_COMMIT})")

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

# include(CTest) is what *defines* BUILD_TESTING, so it has to come first.
# Testing the flag before including the module meant tests/ was never
# configured unless -DBUILD_TESTING=ON was passed explicitly, and a stale
# build tree would silently report "Total Tests: 0".
include(CTest)
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
