# Integration tests: gtest memory roundtrip + C++ <-> C# file exchange.

set(_rpc_schema "${CMAKE_CURRENT_SOURCE_DIR}/schema/CrossLangTest.rpc")
set(_gen_cpp "${CMAKE_CURRENT_BINARY_DIR}/generated/cpp")
set(_gen_cs "${CMAKE_CURRENT_BINARY_DIR}/generated/cs")
# Code generator expects output dir with trailing slash.
set(_gen_cpp_slash "${_gen_cpp}/")
set(_gen_cs_slash "${_gen_cs}/")

set(RPC_TEST_GEN_CS_DIR "${_gen_cs}")
set(RPC_TEST_VERIFIER_CSPROJ "${CMAKE_CURRENT_SOURCE_DIR}/cs/CrossLangVerifier.csproj")

add_custom_command(
  OUTPUT
    "${_gen_cpp}/CrossLangTest.h"
    "${_gen_cpp}/CrossLangTest.cpp"
    "${_gen_cs}/CrossLangTest.cs"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_gen_cpp}" "${_gen_cs}"
  COMMAND "$<TARGET_FILE:rpc>" -i "${_rpc_schema}" -o "${_gen_cpp_slash}" -g cpp
  COMMAND "$<TARGET_FILE:rpc>" -i "${_rpc_schema}" -o "${_gen_cs_slash}" -g cs
  DEPENDS rpc "${_rpc_schema}"
  COMMENT "Generating CrossLangTest (cpp/cs) from RPC schema"
  VERBATIM
)

find_package(GTest QUIET)
if(NOT GTest_FOUND)
  include(FetchContent)
  FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)
endif()

# Prefer DOTNET_ROOT (official install layout) when PATH is not set (e.g. IDEs).
if(DEFINED ENV{DOTNET_ROOT})
  set(_dotnet_hint "$ENV{DOTNET_ROOT}/dotnet")
  if(EXISTS "${_dotnet_hint}")
    set(RPC_TEST_DOTNET "${_dotnet_hint}" CACHE FILEPATH ".NET CLI for cross-language tests")
  endif()
endif()
if(NOT RPC_TEST_DOTNET)
  find_program(RPC_TEST_DOTNET dotnet DOC ".NET CLI for cross-language tests")
endif()

set(RPC_TEST_DOTNET_ABS "")
if(RPC_TEST_DOTNET)
  get_filename_component(RPC_TEST_DOTNET_ABS "${RPC_TEST_DOTNET}" ABSOLUTE)
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/tests_config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/tests_config.h"
  @ONLY
)

add_executable(rpc_serialization_tests
  runtime/cpp_serialization_memory_test.cpp
  runtime/cross_lang_file_test.cpp
  "${_gen_cpp}/CrossLangTest.cpp"
)
add_dependencies(rpc_serialization_tests rpc)

target_include_directories(rpc_serialization_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/runtime/cpp"
  "${_gen_cpp}"
  "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_features(rpc_serialization_tests PRIVATE cxx_std_11)

if(RPC_TEST_DOTNET)
  target_compile_definitions(rpc_serialization_tests PRIVATE RPC_HAVE_DOTNET=1)
else()
  target_compile_definitions(rpc_serialization_tests PRIVATE RPC_HAVE_DOTNET=0)
endif()

target_link_libraries(rpc_serialization_tests PRIVATE GTest::gtest_main GTest::gtest)

include(GoogleTest)
gtest_discover_tests(rpc_serialization_tests PROPERTIES LABELS "rpc")
