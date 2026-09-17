/* Compiler smoke tests: the compiler exits cleanly and its output contains the
   expected declarations.

   These are TEXT assertions only -- the generated code is neither compiled nor
   run here. The error paths live in compiler_negative_test.cpp and the
   #import behaviour in import_test.cpp.

   Two tests used to live here (ImportRpcCpp, ExampleRpcCpp) pointing at
   bin/Import.rpc and bin/Example.rpc. That directory was removed when the
   namespace unification renamed things, so both failed unconditionally and
   were the only thing standing in for #import coverage. They are replaced by
   import_test.cpp, which uses schemas that still exist. */

#include <gtest/gtest.h>

#include "compiler_harness.h"

using namespace rpc_test;

TEST(Compiler, FullTestRpcCpp) {
	const std::string outDir = makeOutDir("fulltest_cpp");
	mkdirp(outDir);

	EXPECT_EQ(runCompiler(schemaPath("FullTest.rpc"), outDir, "cpp"), 0);
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

	EXPECT_EQ(runCompiler(schemaPath("FullTest.rpc"), outDir, "cs"), 0);
	EXPECT_TRUE(fileContains(outDir + "/FullTest.cs", "class StructType"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.cs", "class StructBase"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.cs", "class DerivedStruct"));
}

TEST(Compiler, FullTestRpcPy) {
	const std::string outDir = makeOutDir("fulltest_py");
	mkdirp(outDir);

	EXPECT_EQ(runCompiler(schemaPath("FullTest.rpc"), outDir, "py"), 0);
	EXPECT_TRUE(fileContains(outDir + "/FullTest.py", "StructType"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.py", "StructBase"));
}

TEST(Compiler, FullTestRpcGo) {
	const std::string outDir = makeOutDir("fulltest_go");
	mkdirp(outDir);

	EXPECT_EQ(runCompiler(schemaPath("FullTest.rpc"), outDir, "go"), 0);
	EXPECT_TRUE(fileContains(outDir + "/FullTest.go", "type StructType struct"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.go", "type StructBase struct"));
	EXPECT_TRUE(fileContains(outDir + "/FullTest.go", "type DerivedStruct struct"));
}

/* An unrecognised -g value silently falls back to cpp rather than failing.
   Pinning the observed behaviour so the trap documented in CLAUDE.md cannot
   change without someone noticing. */
TEST(Compiler, UnknownGeneratorFallsBackToCpp) {
	const std::string outDir = makeOutDir("unknown_generator");
	mkdirp(outDir);

	EXPECT_EQ(runCompiler(schemaPath("FullTest.rpc"), outDir, "not-a-generator"), 0);
	EXPECT_TRUE(fileExists(outDir + "/FullTest.h"))
		<< "unknown -g silently produced C++ output";
}

/* ---------- Edge.rpc: C++ code injection and a multi-byte field mask ---------- */

/* #< ... #> injects at file scope, #{ ... #} into the enclosing struct body.
   This checks the generated text; that the injected code actually compiles is
   covered by rpc_wire_format_tests, which includes this header. */
TEST(Compiler, EdgeRpcCppInjectsSnippets) {
	const std::string outDir = makeOutDir("edge_cpp");
	mkdirp(outDir);

	ASSERT_EQ(runCompiler(schemaPath("Edge.rpc"), outDir, "cpp"), 0);

	const std::string header = outDir + "/Edge.h";
	EXPECT_TRUE(fileContains(header, "kFileLevelInjected"))
		<< "file-level #< ... #> snippet missing from the header";
	EXPECT_TRUE(fileContains(header, "injectedMember_"))
		<< "struct-level #{ ... #} snippet missing from the header";
}

/* Both escapes are documented as C++-only. The other three backends must
   ignore them rather than emit the raw text, which would not compile. */
TEST(Compiler, EdgeRpcSnippetsAreCppOnly) {
	const char* generators[] = { "cs", "py", "go" };
	const char* outputs[] = { "Edge.cs", "Edge.py", "Edge.go" };

	for (size_t i = 0; i < 3; i++) {
		const std::string outDir = makeOutDir(std::string("edge_") + generators[i]);
		mkdirp(outDir);

		ASSERT_EQ(runCompiler(schemaPath("Edge.rpc"), outDir, generators[i]), 0)
			<< "generator=" << generators[i];

		const std::string out = outDir + "/" + outputs[i];
		EXPECT_FALSE(fileContains(out, "kFileLevelInjected"))
			<< generators[i] << " emitted the C++-only file-level snippet";
		EXPECT_FALSE(fileContains(out, "injectedMember_"))
			<< generators[i] << " emitted the C++-only struct-level snippet";
	}
}

/* Ten fields need (10-1)/8+1 = 2 mask bytes; every other test schema stays
   within a single byte. */
TEST(Compiler, EdgeRpcCoversMultiByteMask) {
	const std::string outDir = makeOutDir("edge_mask");
	mkdirp(outDir);

	ASSERT_EQ(runCompiler(schemaPath("Edge.rpc"), outDir, "cpp"), 0);
	EXPECT_TRUE(fileContains(outDir + "/Edge.h", "struct ManyFields"));
	EXPECT_TRUE(fileContains(outDir + "/Edge.h", "struct WithSnippet"));
}
