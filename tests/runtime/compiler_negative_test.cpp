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

/* An "enum Name : <type>" production used to exist in the grammar, but it was
   unusable: the branch took no member list, and its action never registered the
   enum as a definition, so a bare `enum E : int64;` parsed cleanly and was
   silently dropped from every backend's output.

   It has been removed rather than completed -- an enum is one uint8 on the
   wire, so an underlying type could never widen the representable range. Both
   spellings are now ordinary syntax errors. */
TEST(CompilerNegative, EnumUnderlyingTypeIsRejected) {
	// With a member list.
	EXPECT_NE(compileBody(makeOutDir("neg_enum_super_members"),
		"enum E : int64\n{\n\tE1,\n};\n"), 0);

	// And bare -- this one used to parse and vanish without a word.
	EXPECT_NE(compileBody(makeOutDir("neg_enum_super_bare"),
		"enum E : int64;\n"), 0);
}

/* The syntax that IS supported still works: a plain enum with a member list. */
TEST(CompilerNegative, PlainEnumIsAccepted) {
	EXPECT_EQ(compileBody(makeOutDir("neg_enum_plain"),
		"enum E\n{\n\tE1,\n\tE2,\n};\n\nstruct Foo { E e_; };\n"), 0);
}

/* Control: a well-formed schema is accepted, so a passing test above cannot be
   an artefact of the harness always reporting failure. */
TEST(CompilerNegative, WellFormedSchemaIsAccepted) {
	EXPECT_EQ(compileBody(makeOutDir("neg_control"),
		"struct Foo { int32 a_; };\n"), 0);
}
