# Integration tests: gtest memory roundtrip + C++ <-> C# file exchange.

set(_rpc_schema "${CMAKE_CURRENT_SOURCE_DIR}/schema/CrossLangTest.rpc")
set(_gen_cpp "${CMAKE_CURRENT_BINARY_DIR}/generated/cpp")
set(_gen_cs "${CMAKE_CURRENT_BINARY_DIR}/generated/cs")
# Code generator expects output dir with trailing slash.
set(_gen_cpp_slash "${_gen_cpp}/")
set(_gen_cs_slash "${_gen_cs}/")

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

# ---------- FullTest schema (all types) ----------
set(_full_schema "${CMAKE_CURRENT_SOURCE_DIR}/schema/FullTest.rpc")
set(_full_gen_cpp "${CMAKE_CURRENT_BINARY_DIR}/generated/fulltest_cpp")
set(_full_gen_cpp_slash "${_full_gen_cpp}/")

add_custom_command(
  OUTPUT
    "${_full_gen_cpp}/FullTest.h"
    "${_full_gen_cpp}/FullTest.cpp"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_full_gen_cpp}"
  COMMAND "$<TARGET_FILE:rpc>" -i "${_full_schema}" -o "${_full_gen_cpp_slash}" -g cpp
  DEPENDS rpc "${_full_schema}"
  COMMENT "Generating FullTest (cpp) from RPC schema"
  VERBATIM
)

# ---------- FullCrossLang schema (C++ <-> C# cross-language) ----------
set(_fullcl_schema "${CMAKE_CURRENT_SOURCE_DIR}/schema/FullCrossLang.rpc")
set(_fullcl_gen_cpp "${CMAKE_CURRENT_BINARY_DIR}/generated/fullcl_cpp")
set(_fullcl_gen_cs "${CMAKE_CURRENT_BINARY_DIR}/generated/fullcl_cs")
set(_fullcl_gen_cpp_slash "${_fullcl_gen_cpp}/")
set(_fullcl_gen_cs_slash "${_fullcl_gen_cs}/")

add_custom_command(
  OUTPUT
    "${_fullcl_gen_cpp}/FullCrossLang.h"
    "${_fullcl_gen_cpp}/FullCrossLang.cpp"
    "${_fullcl_gen_cs}/FullCrossLang.cs"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_fullcl_gen_cpp}" "${_fullcl_gen_cs}"
  COMMAND "$<TARGET_FILE:rpc>" -i "${_fullcl_schema}" -o "${_fullcl_gen_cpp_slash}" -g cpp
  COMMAND "$<TARGET_FILE:rpc>" -i "${_fullcl_schema}" -o "${_fullcl_gen_cs_slash}" -g cs
  DEPENDS rpc "${_fullcl_schema}"
  COMMENT "Generating FullCrossLang (cpp/cs) from RPC schema"
  VERBATIM
)

# ---------- GoogleTest ----------
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

# ---------- .NET SDK detection ----------
# Prefer DOTNET_ROOT (official install layout) when PATH is not set (e.g. IDEs).
if(DEFINED ENV{DOTNET_ROOT})
  set(_dotnet_hint "$ENV{DOTNET_ROOT}/dotnet")
  if(EXISTS "${_dotnet_hint}")
    set(RPC_TEST_DOTNET "${_dotnet_hint}" CACHE FILEPATH ".NET CLI for cross-language tests")
  endif()
endif()
# Prefer PATH when available; GUI CMake / minimal shells often omit Homebrew.
if(NOT RPC_TEST_DOTNET)
  find_program(RPC_TEST_DOTNET dotnet DOC ".NET CLI for cross-language tests")
endif()
if(NOT RPC_TEST_DOTNET)
  foreach(_rpc_dotnet_candidate IN ITEMS
      "$ENV{HOME}/.dotnet/dotnet"
      /usr/local/share/dotnet/dotnet
      /opt/homebrew/bin/dotnet)
    if(EXISTS "${_rpc_dotnet_candidate}")
      set(RPC_TEST_DOTNET "${_rpc_dotnet_candidate}" CACHE FILEPATH ".NET CLI for cross-language tests")
      break()
    endif()
  endforeach()
endif()
# Typical install locations when PATH is not set (Visual Studio / CMake GUI).
if(NOT RPC_TEST_DOTNET AND WIN32)
  foreach(_rpc_dotnet_candidate IN ITEMS
      "$ENV{ProgramW6432}/dotnet/dotnet.exe"
      "$ENV{ProgramFiles}/dotnet/dotnet.exe"
      "$ENV{LOCALAPPDATA}/Microsoft/dotnet/dotnet.exe")
    if(EXISTS "${_rpc_dotnet_candidate}")
      set(RPC_TEST_DOTNET "${_rpc_dotnet_candidate}" CACHE FILEPATH ".NET CLI for cross-language tests")
      break()
    endif()
  endforeach()
endif()

set(RPC_TEST_DOTNET_ABS "")
if(RPC_TEST_DOTNET)
  get_filename_component(RPC_TEST_DOTNET_ABS "${RPC_TEST_DOTNET}" ABSOLUTE)
endif()

# ---------- CrossLangVerifier (.NET) ----------
set(RPC_TEST_VERIFIER_DLL_ABS "")
# Match .NET configuration to CMake build (output path under tests/cs/bin/...).
if(CMAKE_CONFIGURATION_TYPES)
  set(RPC_TEST_VERIFIER_CONFIGURATION "Debug")
else()
  if(CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "MinSizeRel"
      OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    set(RPC_TEST_VERIFIER_CONFIGURATION "Release")
  else()
    set(RPC_TEST_VERIFIER_CONFIGURATION "Debug")
  endif()
endif()

if(RPC_TEST_DOTNET)
  set(_rpc_verifier_dll "${CMAKE_CURRENT_SOURCE_DIR}/cs/bin/${RPC_TEST_VERIFIER_CONFIGURATION}/net6.0/CrossLangVerifier.dll")
  get_filename_component(RPC_TEST_VERIFIER_DLL_ABS "${_rpc_verifier_dll}" ABSOLUTE)
endif()

if(RPC_TEST_DOTNET)
  set(_rpc_verifier_stamp "${CMAKE_CURRENT_BINARY_DIR}/crosslang_verifier_dotnet.stamp")
  file(GLOB _rpc_verifier_cs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cs/*.cs"
    "${CMAKE_SOURCE_DIR}/runtime/cs/*.cs"
  )
  add_custom_command(
    OUTPUT "${_rpc_verifier_stamp}"
    COMMAND ${RPC_TEST_DOTNET} build "${CMAKE_CURRENT_SOURCE_DIR}/cs/CrossLangVerifier.csproj"
            --configuration "${RPC_TEST_VERIFIER_CONFIGURATION}"
            --verbosity minimal
            "-p:GeneratedCsDir=${_gen_cs}"
    COMMAND ${CMAKE_COMMAND} -E touch "${_rpc_verifier_stamp}"
    DEPENDS
      "${_gen_cs}/CrossLangTest.cs"
      "${CMAKE_CURRENT_SOURCE_DIR}/cs/CrossLangVerifier.csproj"
      ${_rpc_verifier_cs}
    COMMENT "Building CrossLangVerifier (.NET) for integration tests"
    VERBATIM
  )
  add_custom_target(rpc_crosslang_verifier_build DEPENDS "${_rpc_verifier_stamp}")
endif()

# ---------- FullCrossLangVerifier (.NET) ----------
set(RPC_TEST_FULL_VERIFIER_DLL_ABS "")

if(RPC_TEST_DOTNET)
  set(_rpc_full_verifier_dll "${CMAKE_CURRENT_SOURCE_DIR}/cs/bin/${RPC_TEST_VERIFIER_CONFIGURATION}/net6.0/FullCrossLangVerifier.dll")
  get_filename_component(RPC_TEST_FULL_VERIFIER_DLL_ABS "${_rpc_full_verifier_dll}" ABSOLUTE)
endif()

if(RPC_TEST_DOTNET)
  set(_rpc_full_verifier_stamp "${CMAKE_CURRENT_BINARY_DIR}/full_crosslang_verifier_dotnet.stamp")
  file(GLOB _rpc_full_verifier_cs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cs/FullCrossLangVerifier.cs"
  )
  add_custom_command(
    OUTPUT "${_rpc_full_verifier_stamp}"
    COMMAND ${RPC_TEST_DOTNET} build "${CMAKE_CURRENT_SOURCE_DIR}/cs/FullCrossLangVerifier.csproj"
            --configuration "${RPC_TEST_VERIFIER_CONFIGURATION}"
            --verbosity minimal
            "-p:GeneratedCsDir=${_fullcl_gen_cs}"
    COMMAND ${CMAKE_COMMAND} -E touch "${_rpc_full_verifier_stamp}"
    DEPENDS
      "${_fullcl_gen_cs}/FullCrossLang.cs"
      "${CMAKE_CURRENT_SOURCE_DIR}/cs/FullCrossLangVerifier.csproj"
      ${_rpc_full_verifier_cs}
    COMMENT "Building FullCrossLangVerifier (.NET) for integration tests"
    VERBATIM
  )
  add_custom_target(rpc_full_crosslang_verifier_build DEPENDS "${_rpc_full_verifier_stamp}")
endif()

# ---------- Config header ----------
set(RPC_TEST_SCHEMA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/schema")
set(RPC_TEST_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/compiler_output")
file(MAKE_DIRECTORY "${RPC_TEST_OUTPUT_DIR}")

# Compiler path is known relative to the build directory.
set(RPC_TEST_COMPILER_EXE "${PROJECT_BINARY_DIR}/compiler/rpc")

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/tests_config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/tests_config.h"
  @ONLY
)

# ---------- Test executables ----------

# 1. Original serialization + cross-lang tests
add_executable(rpc_serialization_tests
  runtime/cpp_serialization_memory_test.cpp
  runtime/cross_lang_file_test.cpp
  "${_gen_cpp}/CrossLangTest.cpp"
)
add_dependencies(rpc_serialization_tests rpc)
if(RPC_TEST_DOTNET)
  add_dependencies(rpc_serialization_tests rpc_crosslang_verifier_build)
endif()
target_include_directories(rpc_serialization_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/runtime/cpp"
  "${_gen_cpp}"
  "${CMAKE_CURRENT_BINARY_DIR}"
	  "${rapidjson_SOURCE_DIR}/include"
)
target_compile_features(rpc_serialization_tests PRIVATE cxx_std_11)
if(RPC_TEST_DOTNET)
  target_compile_definitions(rpc_serialization_tests PRIVATE RPC_HAVE_DOTNET=1)
else()
  target_compile_definitions(rpc_serialization_tests PRIVATE RPC_HAVE_DOTNET=0)
endif()
target_link_libraries(rpc_serialization_tests PRIVATE GTest::gtest_main GTest::gtest)

# 2. Full schema serialization tests
add_executable(rpc_full_schema_tests
  runtime/full_schema_test.cpp
  "${_full_gen_cpp}/FullTest.cpp"
)
add_dependencies(rpc_full_schema_tests rpc)
target_include_directories(rpc_full_schema_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/runtime/cpp"
  "${_full_gen_cpp}"
  "${rapidjson_SOURCE_DIR}/include"
)
target_compile_features(rpc_full_schema_tests PRIVATE cxx_std_11)
target_link_libraries(rpc_full_schema_tests PRIVATE GTest::gtest_main GTest::gtest)

# 3. Full cross-lang tests
add_executable(rpc_full_crosslang_tests
  runtime/full_cross_lang_test.cpp
  "${_fullcl_gen_cpp}/FullCrossLang.cpp"
)
add_dependencies(rpc_full_crosslang_tests rpc)
if(RPC_TEST_DOTNET)
  add_dependencies(rpc_full_crosslang_tests rpc_full_crosslang_verifier_build)
endif()
target_include_directories(rpc_full_crosslang_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/runtime/cpp"
  "${_fullcl_gen_cpp}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${rapidjson_SOURCE_DIR}/include"
)
target_compile_features(rpc_full_crosslang_tests PRIVATE cxx_std_11)
if(RPC_TEST_DOTNET)
  target_compile_definitions(rpc_full_crosslang_tests PRIVATE RPC_HAVE_DOTNET=1)
else()
  target_compile_definitions(rpc_full_crosslang_tests PRIVATE RPC_HAVE_DOTNET=0)
endif()
target_link_libraries(rpc_full_crosslang_tests PRIVATE GTest::gtest_main GTest::gtest)

# 4. Compiler tests
add_executable(rpc_compiler_tests
  runtime/compiler_test.cpp
)
add_dependencies(rpc_compiler_tests rpc)
target_include_directories(rpc_compiler_tests PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_features(rpc_compiler_tests PRIVATE cxx_std_11)
target_link_libraries(rpc_compiler_tests PRIVATE GTest::gtest_main GTest::gtest)

# 5. Service tests
add_executable(rpc_service_tests
  runtime/service_test.cpp
  "${_full_gen_cpp}/FullTest.cpp"
)
add_dependencies(rpc_service_tests rpc)
target_include_directories(rpc_service_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/runtime/cpp"
  "${_full_gen_cpp}"
  "${rapidjson_SOURCE_DIR}/include"
)
target_compile_features(rpc_service_tests PRIVATE cxx_std_11)
target_link_libraries(rpc_service_tests PRIVATE GTest::gtest_main GTest::gtest)

# 6. JSON serialization/deserialization tests
add_executable(rpc_json_tests
  runtime/json_test.cpp
  "${_full_gen_cpp}/FullTest.cpp"
)
add_dependencies(rpc_json_tests rpc)
target_include_directories(rpc_json_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/runtime/cpp"
  "${_full_gen_cpp}"
  "${rapidjson_SOURCE_DIR}/include"
)
target_compile_features(rpc_json_tests PRIVATE cxx_std_11)
target_link_libraries(rpc_json_tests PRIVATE GTest::gtest_main GTest::gtest)

# 7. Go code generation tests
set(_full_gen_go "${CMAKE_CURRENT_BINARY_DIR}/generated/go")
set(_full_gen_go_slash "${_full_gen_go}/")

add_custom_command(
  OUTPUT "${_full_gen_go}/fulltest.go"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_full_gen_go}"
  COMMAND "$<TARGET_FILE:rpc>" -i "${_full_schema}" -o "${_full_gen_go_slash}" -g go
  DEPENDS rpc "${_full_schema}"
  COMMENT "Generating FullTest (go) from RPC schema"
  VERBATIM
)

add_executable(rpc_go_generator_tests
  generated/go/go_test.cpp
)
add_dependencies(rpc_go_generator_tests rpc)
target_include_directories(rpc_go_generator_tests PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_features(rpc_go_generator_tests PRIVATE cxx_std_11)
target_link_libraries(rpc_go_generator_tests PRIVATE GTest::gtest_main GTest::gtest)

# ---------- Discover tests ----------
include(GoogleTest)
gtest_discover_tests(rpc_serialization_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_full_schema_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_full_crosslang_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_compiler_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_service_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_json_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_go_generator_tests PROPERTIES LABELS "rpc")
