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
test_big_endian(BINTALK_BIG_ENDIAN)

# The version number.
set(RPC_VERSION_MAJOR 1)
set(RPC_VERSION_MINOR 0)
# configure a header file to pass some of the CMake settings.
configure_file("Config.h.in" "Config.h")

# compiler.
add_subdirectory(compiler)
# runtime.
#add_subdirectory(runtime)
