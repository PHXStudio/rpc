#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "tests_config.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

static int shellExitStatus(int status) {
#ifdef _WIN32
	return status;
#else
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
#endif
}

static std::string makeOutDir(const char* subdir) {
	return std::string(RPC_TEST_OUTPUT_DIR) + "/" + subdir;
}

static void mkdirp(const std::string& dir) {
	std::string cmd =
#ifdef _WIN32
		"mkdir \"" + dir + "\" 2>NUL";
#else
		"mkdir -p \"" + dir + "\"";
#endif
	std::system(cmd.c_str());
}

/** Run rpc compiler and return exit status. */
static int runCompiler(const std::string& inputFile, const std::string& outputDir,
                       const std::string& generator) {
	std::ostringstream cmd;
	cmd << "\"" << RPC_TEST_COMPILER_EXE << "\"";
	cmd << " -i \"" << inputFile << "\"";
	cmd << " -o \"" << outputDir << "/\"";
	cmd << " -g " << generator;

	return shellExitStatus(std::system(cmd.str().c_str()));
}

static bool fileContains(const std::string& path, const std::string& needle) {
	std::ifstream f(path);
	if (!f) return false;
	std::string content((std::istreambuf_iterator<char>(f)),
	                     std::istreambuf_iterator<char>());
	return content.find(needle) != std::string::npos;
}

} // namespace

// ============================================================
// Import.rpc + Example.rpc: compile with C++ generator
// ============================================================

TEST(Compiler, ImportRpcCpp) {
	const std::string outDir = makeOutDir("import_cpp");
	mkdirp(outDir);

	EXPECT_EQ(runCompiler(std::string(RPC_TEST_SCHEMA_DIR) + "/../../bin/Import.rpc", outDir, "cpp"), 0);
	EXPECT_TRUE(fileContains(outDir + "/Import.h", "struct StructType"));
	EXPECT_TRUE(fileContains(outDir + "/Import.h", "struct StructBase"));
	EXPECT_TRUE(fileContains(outDir + "/Import.h", "enum EnumName"));
	EXPECT_TRUE(fileContains(outDir + "/Import.h", "ServiceBaseStub"));
	EXPECT_TRUE(fileContains(outDir + "/Import.h", "ServiceBaseProxy"));
}

TEST(Compiler, ExampleRpcCpp) {
	const std::string outDir = makeOutDir("example_cpp");
	mkdirp(outDir);

	EXPECT_EQ(runCompiler(std::string(RPC_TEST_SCHEMA_DIR) + "/../../bin/Example.rpc", outDir, "cpp"), 0);
	EXPECT_TRUE(fileContains(outDir + "/Example.h", "struct DerivedStruct"));
	EXPECT_TRUE(fileContains(outDir + "/Example.h", "DerivedServiceStub"));
	EXPECT_TRUE(fileContains(outDir + "/Example.h", "DerivedServiceProxy"));
	EXPECT_TRUE(fileContains(outDir + "/Example.h", "method3"));
	EXPECT_TRUE(fileContains(outDir + "/Example.h", "method4"));
}

TEST(Compiler, FullTestRpcCpp) {
	const std::string outDir = makeOutDir("fulltest_cpp");
	mkdirp(outDir);

	EXPECT_EQ(runCompiler(std::string(RPC_TEST_SCHEMA_DIR) + "/FullTest.rpc", outDir, "cpp"), 0);
	EXPECT_TRUE(fileContains(outDir + "/FullTest.h", "struct StructType"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.h", "struct StructBase"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.h", "struct DerivedStruct"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.h", "enum EnumName"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.h", "doubleArray_"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.h", "strarray1_"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.h", "bytes_"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.h", "bytes11_"));
}

TEST(Compiler, FullTestRpcCs) {
	const std::string outDir = makeOutDir("fulltest_cs");
	mkdirp(outDir);

	EXPECT_EQ(runCompiler(std::string(RPC_TEST_SCHEMA_DIR) + "/FullTest.rpc", outDir, "cs"), 0);
	EXPECT_TRUE(fileContains(outDir + "/FullTest.cs", "class StructType"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.cs", "class StructBase"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.cs", "class DerivedStruct"));
}

TEST(Compiler, FullTestRpcPy) {
	const std::string outDir = makeOutDir("fulltest_py");
	mkdirp(outDir);

	EXPECT_EQ(runCompiler(std::string(RPC_TEST_SCHEMA_DIR) + "/FullTest.rpc", outDir, "py"), 0);
	EXPECT_TRUE(fileContains(outDir + "/FullTest.py", "StructType"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.py", "StructBase"));
}
