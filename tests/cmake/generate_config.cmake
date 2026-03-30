# generate_config.cmake — generates tests_config.h at build time.
#
# Required variables:
#   INPUT                      — path to tests_config.h.in
#   OUTPUT                     — path to tests_config.h to write
#   RPC_TEST_COMPILER_IN       — resolved path to rpc compiler
#   RPC_TEST_SCHEMA_DIR_VAL    — schema directory
#   RPC_TEST_OUTPUT_DIR_VAL    — output directory for compiler tests

file(READ "${INPUT}" _content)

string(REPLACE "@RPC_TEST_DOTNET_ABS@" "${RPC_TEST_DOTNET_ABS}" _content "${_content}")
string(REPLACE "@RPC_TEST_VERIFIER_DLL_ABS@" "${RPC_TEST_VERIFIER_DLL_ABS}" _content "${_content}")
string(REPLACE "@RPC_TEST_FULL_VERIFIER_DLL_ABS@" "${RPC_TEST_FULL_VERIFIER_DLL_ABS}" _content "${_content}")
string(REPLACE "@RPC_TEST_COMPILER_EXE@" "${RPC_TEST_COMPILER_IN}" _content "${_content}")
string(REPLACE "@RPC_TEST_SCHEMA_DIR@" "${RPC_TEST_SCHEMA_DIR_VAL}" _content "${_content}")
string(REPLACE "@RPC_TEST_OUTPUT_DIR@" "${RPC_TEST_OUTPUT_DIR_VAL}" _content "${_content}")

file(WRITE "${OUTPUT}" "${_content}")
