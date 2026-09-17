/* #import behaviour.

   An imported schema does not get an output of its own: its definitions are
   flattened into the importing file's single output, which is named after the
   importing file's stem. Every backend must therefore (a) emit those imported
   definitions and (b) NOT emit a reference to a separate file that is never
   generated.

   Both halves used to be broken in all four backends, which is why this suite
   exists -- see docs/knowledge-base.md. */

#include <gtest/gtest.h>

#include "compiler_harness.h"

using namespace rpc_test;

namespace {

/* Definitions that live in Imported.rpc. They must appear in the root's
   output because that is the only file generated. */
struct BackendExpectation {
	const char* generator;
	const char* outputFile;
	const char* structDefinition;
	const char* enumDefinition;
};

const BackendExpectation kBackends[] = {
	{ "cpp", "ImportRoot.h",  "struct ImportedStruct", "enum ImportedEnum" },
	{ "cs",  "ImportRoot.cs", "class ImportedStruct",  "enum ImportedEnum" },
	{ "py",  "ImportRoot.py", "class ImportedStruct",  nullptr },
	{ "go",  "ImportRoot.go", "type ImportedStruct",   "type ImportedEnum" },
};

} // namespace

/* Every backend compiles the root file and emits exactly one output, named
   after the root -- never after the imported file. */
TEST(Import, EveryBackendEmitsSingleRootOutput) {
	for (const BackendExpectation& be : kBackends) {
		const std::string outDir = makeOutDir(std::string("import_") + be.generator);
		mkdirp(outDir);

		ASSERT_EQ(runCompiler(schemaPath("ImportRoot.rpc"), outDir, be.generator), 0)
			<< "generator=" << be.generator;

		EXPECT_TRUE(fileExists(outDir + "/" + be.outputFile))
			<< "generator=" << be.generator;
		// The imported file gets no output of its own.
		EXPECT_FALSE(fileExists(outDir + "/Imported.h"))
			<< "generator=" << be.generator;
		EXPECT_FALSE(fileExists(outDir + "/Imported.cs"))
			<< "generator=" << be.generator;
		EXPECT_FALSE(fileExists(outDir + "/Imported.py"))
			<< "generator=" << be.generator;
		EXPECT_FALSE(fileExists(outDir + "/Imported.go"))
			<< "generator=" << be.generator;
	}
}

/* The imported definitions are flattened into the root's output, and the
   root's own definitions are there too. */
TEST(Import, ImportedDefinitionsAreFlattenedIntoRoot) {
	for (const BackendExpectation& be : kBackends) {
		const std::string outDir = makeOutDir(std::string("import_defs_") + be.generator);
		mkdirp(outDir);

		ASSERT_EQ(runCompiler(schemaPath("ImportRoot.rpc"), outDir, be.generator), 0)
			<< "generator=" << be.generator;

		const std::string out = outDir + "/" + be.outputFile;
		EXPECT_TRUE(fileContains(out, be.structDefinition))
			<< be.generator << " is missing the imported struct definition";
		if (be.enumDefinition)
			EXPECT_TRUE(fileContains(out, be.enumDefinition))
				<< be.generator << " is missing the imported enum definition";
	}
}

/* No backend may reference a file that is never generated. This is the half
   that made every backend's output uncompilable. */
TEST(Import, NoDanglingReferenceToImportedFile) {
	for (const BackendExpectation& be : kBackends) {
		const std::string outDir = makeOutDir(std::string("import_dangle_") + be.generator);
		mkdirp(outDir);

		ASSERT_EQ(runCompiler(schemaPath("ImportRoot.rpc"), outDir, be.generator), 0)
			<< "generator=" << be.generator;

		const std::string out = outDir + "/" + be.outputFile;
		EXPECT_FALSE(fileContains(out, "#include \"Imported.h\""))
			<< "cpp: dangling include";
		EXPECT_FALSE(fileContains(out, "from Imported import"))
			<< "py: dangling import";
		EXPECT_FALSE(fileContains(out, "import \"imported\""))
			<< "go: dangling import";
	}
}

/* A reference to an imported type from the root file proves the imported
   definition actually reached the parser rather than being parsed and dropped. */
TEST(Import, RootCanReferenceImportedType) {
	const std::string outDir = makeOutDir("import_root_ref");
	mkdirp(outDir);

	ASSERT_EQ(runCompiler(schemaPath("ImportRoot.rpc"), outDir, "cpp"), 0);

	EXPECT_TRUE(fileContains(outDir + "/ImportRoot.h", "ImportedStruct imported_"));
	EXPECT_TRUE(fileContains(outDir + "/ImportRoot.h", "ImportedEnum kind_"));
}

/* A missing import is fatal: the lexer exits(1) instead of raising, so the
   process must come back with a non-zero status. */
TEST(Import, MissingFileIsFatal) {
	const std::string outDir = makeOutDir("import_missing");
	mkdirp(outDir);

	EXPECT_NE(runCompiler(schemaPath("ImportMissing.rpc"), outDir, "cpp"), 0);
}
