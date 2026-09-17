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

# ---------- Nested schema (nested-struct field mask) ----------
set(_nested_schema "${CMAKE_CURRENT_SOURCE_DIR}/schema/Nested.rpc")
set(_nested_gen_cpp "${CMAKE_CURRENT_BINARY_DIR}/generated/nested_cpp")
set(_nested_gen_py "${CMAKE_CURRENT_BINARY_DIR}/generated/nested_py")
set(_nested_gen_cpp_slash "${_nested_gen_cpp}/")
set(_nested_gen_py_slash "${_nested_gen_py}/")

add_custom_command(
  OUTPUT
    "${_nested_gen_cpp}/Nested.h"
    "${_nested_gen_cpp}/Nested.cpp"
    "${_nested_gen_py}/Nested.py"
  COMMAND ${CMAKE_COMMAND} -E make_directory "${_nested_gen_cpp}" "${_nested_gen_py}"
  COMMAND "$<TARGET_FILE:rpc>" -i "${_nested_schema}" -o "${_nested_gen_cpp_slash}" -g cpp
  COMMAND "$<TARGET_FILE:rpc>" -i "${_nested_schema}" -o "${_nested_gen_py_slash}" -g py
  DEPENDS rpc "${_nested_schema}"
  COMMENT "Generating Nested (cpp/py) from RPC schema"
  VERBATIM
)

# ---------- InteropFull schema (cross-language width coverage) ----------
# CrossLangTest/FullCrossLang never exercise int8/16/64, float/double, a nested
# struct or an array of strings, so this schema is what pins those widths across
# backends. All four generators are run from the same schema.
set(_interop_schema "${CMAKE_CURRENT_SOURCE_DIR}/schema/InteropFull.rpc")
set(_interop_gen_cpp "${CMAKE_CURRENT_BINARY_DIR}/generated/interop_cpp")
set(_interop_gen_py "${CMAKE_CURRENT_BINARY_DIR}/generated/interop_py")
set(_interop_gen_go "${CMAKE_CURRENT_BINARY_DIR}/generated/interop_go")
set(_interop_gen_cs "${CMAKE_CURRENT_BINARY_DIR}/generated/interop_cs")
set(_interop_gen_cpp_slash "${_interop_gen_cpp}/")
set(_interop_gen_py_slash "${_interop_gen_py}/")
set(_interop_gen_go_slash "${_interop_gen_go}/")
set(_interop_gen_cs_slash "${_interop_gen_cs}/")

add_custom_command(
  OUTPUT
    "${_interop_gen_cpp}/InteropFull.h"
    "${_interop_gen_cpp}/InteropFull.cpp"
    "${_interop_gen_py}/InteropFull.py"
    "${_interop_gen_go}/InteropFull.go"
    "${_interop_gen_cs}/InteropFull.cs"
  COMMAND ${CMAKE_COMMAND} -E make_directory
          "${_interop_gen_cpp}" "${_interop_gen_py}" "${_interop_gen_go}" "${_interop_gen_cs}"
  COMMAND "$<TARGET_FILE:rpc>" -i "${_interop_schema}" -o "${_interop_gen_cpp_slash}" -g cpp
  COMMAND "$<TARGET_FILE:rpc>" -i "${_interop_schema}" -o "${_interop_gen_py_slash}" -g py
  COMMAND "$<TARGET_FILE:rpc>" -i "${_interop_schema}" -o "${_interop_gen_go_slash}" -g go
  COMMAND "$<TARGET_FILE:rpc>" -i "${_interop_schema}" -o "${_interop_gen_cs_slash}" -g cs
  DEPENDS rpc "${_interop_schema}"
  COMMENT "Generating InteropFull (cpp/py/go/cs) from RPC schema"
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

# ---------- InteropVerifier (.NET) ----------
# Checks the InteropFull golden vector from C#, so all four backends are held
# to the same bytes. Building it also compiles the generated InteropFull.cs,
# which is how a never-compiled FullTest.cs let a CS0108 warning survive.
set(RPC_TEST_INTEROP_VERIFIER_DLL_ABS "")

if(RPC_TEST_DOTNET)
  set(_rpc_interop_verifier_dll "${CMAKE_CURRENT_SOURCE_DIR}/cs/bin/${RPC_TEST_VERIFIER_CONFIGURATION}/net6.0/InteropVerifier.dll")
  get_filename_component(RPC_TEST_INTEROP_VERIFIER_DLL_ABS "${_rpc_interop_verifier_dll}" ABSOLUTE)
endif()

if(RPC_TEST_DOTNET)
  set(_rpc_interop_verifier_stamp "${CMAKE_CURRENT_BINARY_DIR}/interop_verifier_dotnet.stamp")
  file(GLOB _rpc_interop_verifier_cs CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/cs/InteropVerifier.cs"
  )
  add_custom_command(
    OUTPUT "${_rpc_interop_verifier_stamp}"
    COMMAND ${RPC_TEST_DOTNET} build "${CMAKE_CURRENT_SOURCE_DIR}/cs/InteropVerifier.csproj"
            --configuration "${RPC_TEST_VERIFIER_CONFIGURATION}"
            --verbosity minimal
            "-p:GeneratedCsDir=${_interop_gen_cs}"
    COMMAND ${CMAKE_COMMAND} -E touch "${_rpc_interop_verifier_stamp}"
    DEPENDS
      "${_interop_gen_cs}/InteropFull.cs"
      "${CMAKE_CURRENT_SOURCE_DIR}/cs/InteropVerifier.csproj"
      ${_rpc_interop_verifier_cs}
    COMMENT "Building InteropVerifier (.NET) for integration tests"
    VERBATIM
  )
  add_custom_target(rpc_interop_verifier_build DEPENDS "${_rpc_interop_verifier_stamp}")
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

# 4b. Negative compiler tests: malformed schemas must be rejected.
add_executable(rpc_compiler_negative_tests
  runtime/compiler_negative_test.cpp
)
add_dependencies(rpc_compiler_negative_tests rpc)
target_include_directories(rpc_compiler_negative_tests PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_features(rpc_compiler_negative_tests PRIVATE cxx_std_11)
target_link_libraries(rpc_compiler_negative_tests PRIVATE GTest::gtest_main GTest::gtest)

# 4c. #import tests: imported definitions are flattened into the root's output.
add_executable(rpc_import_tests
  runtime/import_test.cpp
)
add_dependencies(rpc_import_tests rpc)
target_include_directories(rpc_import_tests PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_features(rpc_import_tests PRIVATE cxx_std_11)
target_link_libraries(rpc_import_tests PRIVATE GTest::gtest_main GTest::gtest)

# 4d. Runtime boundary tests: Skip limits and the version-compat read path.
add_executable(rpc_runtime_edge_tests
  runtime/runtime_edge_test.cpp
)
target_include_directories(rpc_runtime_edge_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/runtime/cpp"
)
target_compile_features(rpc_runtime_edge_tests PRIVATE cxx_std_11)
target_link_libraries(rpc_runtime_edge_tests PRIVATE GTest::gtest_main GTest::gtest)

# 4e. InteropFull cross-language test: the same golden vector, checked in C#.
add_executable(rpc_interop_crosslang_tests
  runtime/interop_crosslang_test.cpp
  "${_interop_gen_cpp}/InteropFull.cpp"
)
add_dependencies(rpc_interop_crosslang_tests rpc)
if(RPC_TEST_DOTNET)
  add_dependencies(rpc_interop_crosslang_tests rpc_interop_verifier_build)
endif()
target_include_directories(rpc_interop_crosslang_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/runtime/cpp"
  "${_interop_gen_cpp}"
  "${CMAKE_CURRENT_BINARY_DIR}"
  "${rapidjson_SOURCE_DIR}/include"
)
target_compile_features(rpc_interop_crosslang_tests PRIVATE cxx_std_11)
if(RPC_TEST_DOTNET)
  target_compile_definitions(rpc_interop_crosslang_tests PRIVATE RPC_HAVE_DOTNET=1)
else()
  target_compile_definitions(rpc_interop_crosslang_tests PRIVATE RPC_HAVE_DOTNET=0)
endif()
target_link_libraries(rpc_interop_crosslang_tests PRIVATE GTest::gtest_main GTest::gtest)

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
# Note: there is no add_custom_command here. go_test.cpp is a checked-in source
# file that re-runs the compiler itself into RPC_TEST_OUTPUT_DIR/go/, so a
# build-time generation step would produce an output nothing consumes.
add_executable(rpc_go_generator_tests
  generated/go/go_test.cpp
)
add_dependencies(rpc_go_generator_tests rpc)
target_include_directories(rpc_go_generator_tests PRIVATE
  "${CMAKE_CURRENT_BINARY_DIR}"
)
target_compile_features(rpc_go_generator_tests PRIVATE cxx_std_11)
target_link_libraries(rpc_go_generator_tests PRIVATE GTest::gtest_main GTest::gtest)

# 8. Wire format golden vectors
# Pins the exact bytes of the encoding so a change to any generator or runtime
# cannot pass unnoticed; the rest of the suite only checks roundtrip equality,
# which any self-consistent (including wrong) encoding would satisfy.
add_executable(rpc_wire_format_tests
  runtime/wire_format_golden_test.cpp
  "${_gen_cpp}/CrossLangTest.cpp"
  "${_fullcl_gen_cpp}/FullCrossLang.cpp"
  "${_nested_gen_cpp}/Nested.cpp"
  "${_interop_gen_cpp}/InteropFull.cpp"
)
add_dependencies(rpc_wire_format_tests rpc)
target_include_directories(rpc_wire_format_tests PRIVATE
  "${CMAKE_SOURCE_DIR}/runtime/cpp"
  "${_gen_cpp}"
  "${_fullcl_gen_cpp}"
  "${_nested_gen_cpp}"
  "${_interop_gen_cpp}"
  "${rapidjson_SOURCE_DIR}/include"
)
target_compile_features(rpc_wire_format_tests PRIVATE cxx_std_11)
target_link_libraries(rpc_wire_format_tests PRIVATE GTest::gtest_main GTest::gtest)

# ---------- Go cross-language test ----------
# Unlike rpc_go_generator_tests (which greps the generated source), this one
# actually compiles and runs the generated Go code against the same golden
# vector the C++ and Python tests use. Skipped when no Go toolchain is found,
# mirroring how the .NET tests behave.
find_program(RPC_TEST_GO go)
if(RPC_TEST_GO)
  set(RPC_RUNTIME_GO_DIR "${CMAKE_SOURCE_DIR}/runtime/go")
  set(_interop_go_mod "${CMAKE_CURRENT_BINARY_DIR}/interop_go_module")
  file(MAKE_DIRECTORY "${_interop_go_mod}")

  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/go/go.mod.in"
    "${_interop_go_mod}/go.mod"
    @ONLY
  )
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/go/interop_golden_test.go"
    "${_interop_go_mod}/interop_golden_test.go"
    COPYONLY
  )
  add_custom_command(
    OUTPUT "${_interop_go_mod}/InteropFull.go"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${_interop_gen_go}/InteropFull.go" "${_interop_go_mod}/InteropFull.go"
    DEPENDS "${_interop_gen_go}/InteropFull.go"
    COMMENT "Staging generated Go code into a testable module"
    VERBATIM
  )
  add_custom_target(rpc_interop_go_module ALL DEPENDS "${_interop_go_mod}/InteropFull.go")

  add_test(
    NAME GoInteropGolden
    COMMAND "${RPC_TEST_GO}" test ./...
    WORKING_DIRECTORY "${_interop_go_mod}"
  )
  set_tests_properties(GoInteropGolden PROPERTIES LABELS "rpc")
endif()

# ---------- Python conformance test ----------
# The C++ golden vectors pin the wire format but cannot catch a Python-only
# regression, which is exactly what the nested-struct mask bug was. Skipped
# when no interpreter is found, mirroring how the .NET tests behave.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME PythonWireFormatGolden
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/py/wire_format_test.py"
            "${_nested_gen_py}"
            "${CMAKE_SOURCE_DIR}/runtime/py"
  )
  # The generated Python comes from the same custom command that produces the
  # C++ sources, so it exists as soon as rpc_wire_format_tests has been built.
  set_tests_properties(PythonWireFormatGolden PROPERTIES LABELS "rpc")

  # Same idea, against the InteropFull vector shared with the C++ and Go tests.
  add_test(
    NAME PythonInteropGolden
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/py/interop_test.py"
            "${_interop_gen_py}"
            "${CMAKE_SOURCE_DIR}/runtime/py"
  )
  set_tests_properties(PythonInteropGolden PROPERTIES LABELS "rpc")
endif()

# ---------- Discover tests ----------
include(GoogleTest)
gtest_discover_tests(rpc_serialization_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_wire_format_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_full_schema_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_full_crosslang_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_compiler_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_compiler_negative_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_import_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_runtime_edge_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_interop_crosslang_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_service_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_json_tests PROPERTIES LABELS "rpc")
gtest_discover_tests(rpc_go_generator_tests PROPERTIES LABELS "rpc")
