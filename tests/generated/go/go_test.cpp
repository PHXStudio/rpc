// Go code generation test - verifies Go generator works correctly
#include <gtest/gtest.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "tests_config.h"

namespace {

// Read file content into string
std::string readFile(const std::string& path) {
	std::ifstream file(path);
	if (!file) {
		return "";
	}
	std::stringstream buffer;
	buffer << file.rdbuf();
	return buffer.str();
}

// Check if string contains substring
bool contains(const std::string& str, const std::string& substr) {
	return str.find(substr) != std::string::npos;
}

// Get output directory for generated files
std::string getOutputDir() {
	return std::string(RPC_TEST_OUTPUT_DIR) + "/go/";
}

// Get schema file path
std::string getSchemaFile(const char* name) {
	return std::string(RPC_TEST_SCHEMA_DIR) + "/" + name;
}

// Build generation command
std::string buildGenCmd(const char* schema, const char* output) {
	std::string schemaFile = getSchemaFile(schema);
	std::string outputDir = getOutputDir() + output;
	return std::string(RPC_TEST_COMPILER_EXE) + " -g go -i " + schemaFile + " -o " + outputDir;
}

} // namespace

// Test that Go code can be generated for FullTest.rpc
TEST(GoGenerationTest, FullTestGenerates) {
	std::string outputDir = getOutputDir();
	std::string mkdirCmd = "mkdir -p " + outputDir;
	std::system(mkdirCmd.c_str());

	std::string cmd = buildGenCmd("FullTest.rpc", "");
	int result = std::system(cmd.c_str());
	EXPECT_EQ(result, 0) << "Go generation command failed: " << cmd;

	// Check that output file was created
	std::string outputFile = outputDir + "fulltest.go";
	std::string content = readFile(outputFile);

	// Verify package declaration
	EXPECT_TRUE(contains(content, "package fulltest"))
		<< "Missing package declaration";

	// Verify imports
	EXPECT_TRUE(contains(content, "\"encoding/json\""))
		<< "Missing json import";
	EXPECT_TRUE(contains(content, "\"github.com/arpc/runtime\""))
		<< "Missing runtime import";

	// Verify enum generation
	EXPECT_TRUE(contains(content, "type EnumName int32"))
		<< "Missing enum type";
	EXPECT_TRUE(contains(content, "EN1 EnumName = iota"))
		<< "Missing enum constants";

	// Verify struct generation
	EXPECT_TRUE(contains(content, "type StructType struct"))
		<< "Missing StructType";
	EXPECT_TRUE(contains(content, "type StructBase struct"))
		<< "Missing StructBase";
	EXPECT_TRUE(contains(content, "type DerivedStruct struct"))
		<< "Missing DerivedStruct";

	// Verify serialization methods
	EXPECT_TRUE(contains(content, "func (s *StructType) Serialize"))
		<< "Missing Serialize method";
	EXPECT_TRUE(contains(content, "func (s *StructType) Deserialize"))
		<< "Missing Deserialize method";

	// Verify service generation
	EXPECT_TRUE(contains(content, "type ServiceBaseStub struct"))
		<< "Missing service stub";
	EXPECT_TRUE(contains(content, "type ServiceBaseProxy interface"))
		<< "Missing service proxy";
	EXPECT_TRUE(contains(content, "type ServiceBaseDispatcher struct"))
		<< "Missing service dispatcher";
}

// Test that Go code contains field mask logic
TEST(GoGenerationTest, ContainsFieldMask) {
	std::string outputDir = getOutputDir();
	std::string cmd = buildGenCmd("FullTest.rpc", "");
	int result = std::system(cmd.c_str());
	ASSERT_EQ(result, 0);

	std::string outputFile = outputDir + "fulltest.go";
	std::string content = readFile(outputFile);

	// Verify field mask usage
	EXPECT_TRUE(contains(content, "arpc.NewFieldMask"))
		<< "Missing field mask creation";
	EXPECT_TRUE(contains(content, "fm.WriteBit"))
		<< "Missing field mask write";
	EXPECT_TRUE(contains(content, "fm.ReadBit"))
		<< "Missing field mask read";
}

// Test that Go code contains method implementations
TEST(GoGenerationTest, ContainsServiceMethods) {
	std::string outputDir = getOutputDir();
	std::string cmd = buildGenCmd("FullTest.rpc", "");
	int result = std::system(cmd.c_str());
	ASSERT_EQ(result, 0);

	std::string outputFile = outputDir + "fulltest.go";
	std::string content = readFile(outputFile);

	// Verify method implementations
	EXPECT_TRUE(contains(content, "func (s *ServiceBaseStub) method1"))
		<< "Missing method1 implementation";
	EXPECT_TRUE(contains(content, "func (s *ServiceBaseStub) method2"))
		<< "Missing method2 implementation";
	EXPECT_TRUE(contains(content, "WriteUint16"))
		<< "Missing method ID writing";

	// Verify dispatch methods
	EXPECT_TRUE(contains(content, "func (d *ServiceBaseDispatcher) dispatchmethod1"))
		<< "Missing dispatch method";
	EXPECT_TRUE(contains(content, "Dispatch(reader arpc.ProtocolReader"))
		<< "Missing Dispatch method";
}

// Test struct inheritance
TEST(GoGenerationTest, StructInheritance) {
	std::string outputDir = getOutputDir();
	std::string cmd = buildGenCmd("FullTest.rpc", "");
	int result = std::system(cmd.c_str());
	ASSERT_EQ(result, 0);

	std::string outputFile = outputDir + "fulltest.go";
	std::string content = readFile(outputFile);

	// DerivedStruct should embed StructBase
	EXPECT_TRUE(contains(content, "type DerivedStruct struct {"))
		<< "Missing DerivedStruct declaration";

	// Check that derived struct serializes base first
	EXPECT_TRUE(contains(content, "s.StructBase.Serialize(writer)"))
		<< "Missing base class serialization call";
}

// Test JSON tags
TEST(GoGenerationTest, JsonTags) {
	std::string outputDir = getOutputDir();
	std::string cmd = buildGenCmd("FullTest.rpc", "");
	int result = std::system(cmd.c_str());
	ASSERT_EQ(result, 0);

	std::string outputFile = outputDir + "fulltest.go";
	std::string content = readFile(outputFile);

	// Verify JSON struct tags
	EXPECT_TRUE(contains(content, "`json:\"aaa_\"`"))
		<< "Missing JSON tag for aaa_";
	EXPECT_TRUE(contains(content, "`json:\"bbb_\"`"))
		<< "Missing JSON tag for bbb_";
	EXPECT_TRUE(contains(content, "`json:\"double_\"`"))
		<< "Missing JSON tag for double_";
}

// Test enum JSON methods
TEST(GoGenerationTest, EnumJsonMethods) {
	std::string outputDir = getOutputDir();
	std::string cmd = buildGenCmd("FullTest.rpc", "");
	int result = std::system(cmd.c_str());
	ASSERT_EQ(result, 0);

	std::string outputFile = outputDir + "fulltest.go";
	std::string content = readFile(outputFile);

	// Verify enum JSON methods
	EXPECT_TRUE(contains(content, "func (e EnumName) String() string"))
		<< "Missing enum String method";
	EXPECT_TRUE(contains(content, "func (e EnumName) MarshalJSON"))
		<< "Missing enum MarshalJSON method";
	EXPECT_TRUE(contains(content, "func (e *EnumName) UnmarshalJSON"))
		<< "Missing enum UnmarshalJSON method";
}

// Test array types
TEST(GoGenerationTest, ArrayTypes) {
	std::string outputDir = getOutputDir();
	std::string cmd = buildGenCmd("FullTest.rpc", "");
	int result = std::system(cmd.c_str());
	ASSERT_EQ(result, 0);

	std::string outputFile = outputDir + "fulltest.go";
	std::string content = readFile(outputFile);

	// Verify slice types for dynamic arrays
	EXPECT_TRUE(contains(content, "DoubleArray []float64"))
		<< "Missing float64 array";
	EXPECT_TRUE(contains(content, "StrArray []string"))
		<< "Missing string array";
	EXPECT_TRUE(contains(content, "EnumArray []EnumName"))
		<< "Missing enum array";

	// Verify array serialization
	EXPECT_TRUE(contains(content, "WriteDynSize(uint32(len(s.DoubleArray)))"))
		<< "Missing array size writing";
	EXPECT_TRUE(contains(content, "for _, v := range s.DoubleArray"))
		<< "Missing array iteration";
}

// Test constructor generation
TEST(GoGenerationTest, ConstructorGeneration) {
	std::string outputDir = getOutputDir();
	std::string cmd = buildGenCmd("FullTest.rpc", "");
	int result = std::system(cmd.c_str());
	ASSERT_EQ(result, 0);

	std::string outputFile = outputDir + "fulltest.go";
	std::string content = readFile(outputFile);

	// Verify New* constructors
	EXPECT_TRUE(contains(content, "func NewStructType() *StructType"))
		<< "Missing StructType constructor";
	EXPECT_TRUE(contains(content, "func NewStructBase() *StructBase"))
		<< "Missing StructBase constructor";
	EXPECT_TRUE(contains(content, "func NewDerivedStruct() *DerivedStruct"))
		<< "Missing DerivedStruct constructor";

	// Verify default values
	EXPECT_TRUE(contains(content, "Bbb: 0"))
		<< "Missing default value for Bbb";
	EXPECT_TRUE(contains(content, "Aaa: \"\""))
		<< "Missing default value for Aaa";
}

// Test field ID constants
TEST(GoGenerationTest, FieldIdConstants) {
	std::string outputDir = getOutputDir();
	std::string cmd = buildGenCmd("FullTest.rpc", "");
	int result = std::system(cmd.c_str());
	ASSERT_EQ(result, 0);

	std::string outputFile = outputDir + "fulltest.go";
	std::string content = readFile(outputFile);

	// Verify field ID constants
	EXPECT_TRUE(contains(content, "FIDStructTypeAaa"))
		<< "Missing FIDStructTypeAaa";
	EXPECT_TRUE(contains(content, "FIDStructTypeBbb"))
		<< "Missing FIDStructTypeBbb";
	EXPECT_TRUE(contains(content, "FIDStructTypeMax"))
		<< "Missing FIDStructTypeMax";
}
