/* Simple version compatibility test - no JSON dependency
   Tests the FieldMask length prefix feature for version compatibility
*/
#include "../runtime/cpp/ProtocolMemWriter.h"
#include "../runtime/cpp/ProtocolMemReader.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <iomanip>
#include <fstream>

using namespace std;

// Test 1: Verify FieldMask length prefix is written correctly
void testFieldMaskLengthPrefix()
{
    cout << "=== Test 1: FieldMask Length Prefix ===" << endl;

    vector<uint8_t> buffer(1024);
    ProtocolMemWriter writer(buffer.data(), buffer.size());

    // Simulate what generated code does: write FieldMask length
    uint8_t fmLen = 4;
    writer.writeType(fmLen);

    // Write FieldMask data
    uint8_t fmData[4] = {0xFF, 0x1F, 0x00, 0x00};
    writer.write(fmData, 4);

    // Write some field data
    double d = 123.456;
    writer.writeType(d);

    size_t writtenSize = writer.length();

    // Print hex dump
    cout << "Serialized " << writtenSize << " bytes:" << endl;
    for (size_t i = 0; i < writtenSize; i++) {
        if (i % 16 == 0) cout << "    " << setfill('0') << hex << setw(4) << i << ": ";
        cout << setfill('0') << setw(2) << (int)buffer[i] << " ";
        if ((i + 1) % 16 == 0) cout << endl;
    }
    cout << dec << endl;

    // Verify: first byte should be FieldMask length
    assert(buffer[0] == 4 && "First byte must be FieldMask length");
    cout << "✓ First byte is FieldMask length: " << (int)buffer[0] << endl;
    cout << "✓ PASSED" << endl;
    cout << endl;
}

// Test 2: Verify old version can skip extra FieldMask bytes
void testOldVersionSkipsExtraBytes()
{
    cout << "=== Test 2: Old Version Skips Extra Bytes ===" << endl;

    // Simulate "new version" with 8-byte FieldMask
    vector<uint8_t> buffer(1024);
    ProtocolMemWriter writer(buffer.data(), buffer.size());

    uint8_t newFmLen = 8;
    writer.writeType(newFmLen);

    uint8_t newFmData[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    writer.write(newFmData, 8);

    double d = 999.888;
    writer.writeType(d);

    size_t dataSize = writer.length();

    // Simulate "old version" with 4-byte FieldMask reading
    ProtocolMemReader reader(buffer.data(), dataSize);

    uint8_t actualFmLen = 0;
    bool success = reader.readType(actualFmLen);
    assert(success && actualFmLen == 8);
    cout << "New version FieldMask length: " << (int)actualFmLen << endl;

    // Old version only reads 4 bytes
    uint8_t oldFmLen = 4;
    uint8_t readFmLen = (actualFmLen < oldFmLen) ? actualFmLen : oldFmLen;
    cout << "Old version reads: " << (int)readFmLen << " bytes" << endl;

    uint8_t fmData[4] = {0};
    success = reader.read(fmData, readFmLen);
    assert(success);

    // Old version skips remaining bytes
    cout << "Old version skips: " << (int)(actualFmLen - readFmLen) << " bytes" << endl;
    success = reader.skip(actualFmLen - readFmLen);
    assert(success);

    // Old version can still read the field data
    double result = 0;
    success = reader.readType(result);
    assert(success && result == 999.888);
    cout << "Old version successfully read field data: " << result << endl;

    cout << "✓ PASSED" << endl;
    cout << endl;
}

// Test 3: Verify new version can read old data
void testNewVersionReadsOldData()
{
    cout << "=== Test 3: New Version Reads Old Data ===" << endl;

    // Simulate "old version" with 4-byte FieldMask
    vector<uint8_t> buffer(1024);
    ProtocolMemWriter writer(buffer.data(), buffer.size());

    uint8_t oldFmLen = 4;
    writer.writeType(oldFmLen);

    uint8_t oldFmData[4] = {0xFF, 0x0F, 0x00, 0x00};
    writer.write(oldFmData, 4);

    double d = 111.222;
    writer.writeType(d);

    size_t dataSize = writer.length();

    // Simulate "new version" with 8-byte FieldMask reading
    ProtocolMemReader reader(buffer.data(), dataSize);

    uint8_t actualFmLen = 0;
    bool success = reader.readType(actualFmLen);
    assert(success && actualFmLen == 4);
    cout << "Old version FieldMask length: " << (int)actualFmLen << endl;

    uint8_t newFmLen = 8;
    uint8_t readFmLen = (actualFmLen < newFmLen) ? actualFmLen : newFmLen;
    cout << "New version reads: " << (int)readFmLen << " bytes" << endl;

    uint8_t fmData[8] = {0};
    success = reader.read(fmData, readFmLen);
    assert(success);

    // Remaining bytes should be zero (new fields are default)
    for (int i = readFmLen; i < newFmLen; i++) {
        fmData[i] = 0;
    }

    cout << "FieldMask data: ";
    for (int i = 0; i < 8; i++) cout << hex << setfill('0') << setw(2) << (int)fmData[i] << " ";
    cout << dec << endl;

    // New version can read the field data
    double result = 0;
    success = reader.readType(result);
    assert(success && result == 111.222);
    cout << "New version successfully read field data: " << result << endl;

    cout << "✓ PASSED" << endl;
    cout << endl;
}

// Test 4: Verify skip() functionality
void testSkipFunction()
{
    cout << "=== Test 4: Skip Function ===" << endl;

    vector<uint8_t> buffer(1024);
    for (size_t i = 0; i < buffer.size(); i++) buffer[i] = i & 0xFF;

    ProtocolMemReader reader(buffer.data(), buffer.size());

    // Skip 10 bytes
    bool success = reader.skip(10);
    assert(success);

    // Read next byte - should be byte at index 10
    uint8_t b = 0;
    success = reader.readType(b);
    assert(success && b == 10);
    cout << "After skipping 10 bytes, read byte: " << (int)b << " (expected 10)" << endl;

    // Try to skip beyond buffer - should fail
    success = reader.skip(buffer.size());
    assert(!success);
    cout << "Skipping beyond buffer correctly fails" << endl;

    cout << "✓ PASSED" << endl;
    cout << endl;
}

// Test 5: Generate test data for C#
void testGenerateCSharpTestData()
{
    cout << "=== Test 5: Generate C# Test Data ===" << endl;

    vector<uint8_t> buffer(4096);
    ProtocolMemWriter writer(buffer.data(), buffer.size());

    // Write FieldMask length
    uint8_t fmLen = 4;
    writer.writeType(fmLen);

    // Write FieldMask
    uint8_t fmData[4] = {0xFF, 0x1F, 0x00, 0x00};
    writer.write(fmData, 4);

    // Write test data matching the generated StructBase format
    double double_ = 123.456;
    float float_ = 78.9f;
    int64_t int64_ = 12345678901LL;
    uint64_t uint64_ = 9876543210ULL;
    int32_t int32_ = 12345;
    uint32_t uint32_ = 67890;
    int16_t int16_ = 1234;
    uint16_t uint16_ = 5678;
    int8_t int8_ = 12;
    uint8_t uint8_ = 34;

    writer.writeType(double_);
    writer.writeType(float_);
    writer.writeType(int64_);
    writer.writeType(uint64_);
    writer.writeType(int32_);
    writer.writeType(uint32_);
    writer.writeType(int16_);
    writer.writeType(uint16_);
    writer.writeType(int8_);
    writer.writeType(uint8_);

    // bool (written as byte with value 1)
    char boolVal = 1;
    writer.write(&boolVal, 1);

    // enum (written as byte)
    uint8_t enumVal = 2;
    writer.writeType(enumVal);

    // string
    string str = "hello from C++";
    uint32_t strLen = str.length();
    writer.writeDynSize(strLen);
    writer.write(str.c_str(), str.length());

    size_t dataSize = writer.length();

    // Save to file (in current directory)
    ofstream outFile("csharp_test_data.bin", ios::binary);
    outFile.write((char*)buffer.data(), dataSize);
    outFile.close();

    cout << "Generated " << dataSize << " bytes" << endl;
    cout << "Saved to: csharp_test_data.bin (in tests directory)" << endl;
    cout << endl;

    cout << "Binary structure:" << endl;
    cout << "  Byte 0: FieldMask length = " << (int)buffer[0] << endl;
    cout << "  Bytes 1-4: FieldMask = ";
    for (int i = 1; i <= 4; i++) cout << hex << setfill('0') << setw(2) << (int)buffer[i] << " ";
    cout << dec << endl;
    cout << "  Bytes 5+: Field data" << endl;
    cout << endl;

    // Verify by reading back
    ProtocolMemReader reader(buffer.data(), dataSize);
    uint8_t fmLenCheck = 0;
    reader.readType(fmLenCheck);
    assert(fmLenCheck == 4);

    uint8_t fmCheck[4] = {0};
    reader.read(fmCheck, 4);

    double dCheck = 0;
    reader.readType(dCheck);
    assert(dCheck == 123.456);

    cout << "Verification successful" << endl;
    cout << "✓ PASSED" << endl;
    cout << endl;
}

int main()
{
    cout << "========================================" << endl;
    cout << "Version Compatibility Feature Test" << endl;
    cout << "No JSON Dependency" << endl;
    cout << "========================================" << endl;
    cout << endl;

    try {
        testFieldMaskLengthPrefix();
        testOldVersionSkipsExtraBytes();
        testNewVersionReadsOldData();
        testSkipFunction();
        testGenerateCSharpTestData();

        cout << "========================================" << endl;
        cout << "✅ All tests PASSED" << endl;
        cout << "========================================" << endl;
        cout << endl;
        cout << "Summary:" << endl;
        cout << "  - FieldMask length prefix is correctly written" << endl;
        cout << "  - Old versions can skip extra FieldMask bytes" << endl;
        cout << "  - New versions can read old data (defaults for new fields)" << endl;
        cout << "  - skip() function works correctly" << endl;
        cout << endl;
        cout << "Binary test data saved to: tests/csharp_test_data.bin" << endl;

        return 0;
    } catch (const exception& e) {
        cerr << "❌ TEST FAILED: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "❌ TEST FAILED: Unknown exception" << endl;
        return 1;
    }
}
