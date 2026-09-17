/* Negative compiler tests: malformed schemas must be rejected with a non-zero
   exit status.

   compiler_test.cpp only ever asserts success (EXPECT_EQ(runCompiler(...), 0)),
   so before this file the parser and the semantic checker had no coverage of
   their error paths at all -- a regression that silently accepted bad input
   would have gone unnoticed. */

#include <gtest/gtest.h>

#include <fstream>

#include "compiler_harness.h"

using namespace rpc_test;

namespace {

/* Compile a throwaway schema and return the exit status. */
int compileBody(const std::string& dir, const std::string& body) {
	mkdirp(dir);
	const std::string path = dir + "/bad.rpc";
	std::ofstream f(path.c_str());
	f << body;
	f.close();
	return runCompiler(path, dir, "cpp");
}

} // namespace

/* The struct marker (skipcomp) was removed from the language, so a leftover
   parenthesis is a syntax error rather than a silent no-op. */
TEST(CompilerNegative, SyntaxErrorIsRejected) {
	EXPECT_NE(compileBody(makeOutDir("neg_syntax"),
		"struct Foo (skipcomp)\n{\n\tint32 a_;\n};\n"), 0);
}

TEST(CompilerNegative, DuplicateDefinitionIsRejected) {
	EXPECT_NE(compileBody(makeOutDir("neg_dupdef"),
		"struct Foo { int32 a_; };\nstruct Foo { int32 b_; };\n"), 0);
}

TEST(CompilerNegative, DuplicateFieldIsRejected) {
	EXPECT_NE(compileBody(makeOutDir("neg_dupfield"),
		"struct Foo { int32 a_; int32 a_; };\n"), 0);
}

TEST(CompilerNegative, UnknownTypeIsRejected) {
	EXPECT_NE(compileBody(makeOutDir("neg_unknowntype"),
		"struct Foo { UnknownType a_; };\n"), 0);
}

TEST(CompilerNegative, SelfInheritanceIsRejected) {
	EXPECT_NE(compileBody(makeOutDir("neg_selfinherit"),
		"struct Foo : Foo { int32 a_; };\n"), 0);
}

/* A service is not a type: using one as a field type must be diagnosed. */
TEST(CompilerNegative, ServiceAsFieldTypeIsRejected) {
	EXPECT_NE(compileBody(makeOutDir("neg_serviceastype"),
		"service S { m1(); };\nstruct Foo { S a_; };\n"), 0);
}

/* Control: a well-formed schema is accepted, so a passing test above cannot be
   an artefact of the harness always reporting failure. */
TEST(CompilerNegative, WellFormedSchemaIsAccepted) {
	EXPECT_EQ(compileBody(makeOutDir("neg_control"),
		"struct Foo { int32 a_; };\n"), 0);
}
