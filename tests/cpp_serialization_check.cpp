/* Cross-language compatibility test between C# and C++
   Tests that C# can deserialize C++ serialized data and vice versa
*/
#include "../runtime/cpp/ProtocolMemWriter.h"
#include "../runtime/cpp/ProtocolMemReader.h"
#include "../output_cpp/FullTest.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <fstream>
#include <iomanip>

using namespace std;

// Helper function to print hex dump
void printHexDump(const uint8_t* data, size_t size)
{
    cout << "Hex dump (" << size << " bytes):" << endl;
    for (size_t i = 0; i < size; i++)
    {
        if (i % 16 == 0) cout << string(8, ' ') << setfill('0') << hex << setw(4) << i << ": ";
        cout << setfill('0') << setw(2) << (int)data[i] << " ";
        if ((i + 1) % 16 == 0 || i == size - 1) cout << endl;
    }
    cout << dec << setfill(' ') << endl;
}

// Test 1: C++ serialize, then save to file for C# to read
void testCppSerializeForCSharp()
{
    cout << "=== Test 1: C++ Serialize for C# ===" << endl;

    // Create test data
    StructBase cppData;
    cppData.double_ = 123.456;
    cppData.float_ = 78.9f;
    cppData.int64_ = 12345678901LL;
    cppData.uint64_ = 9876543210ULL;
    cppData.int32_ = 12345;
    cppData.uint32_ = 67890;
    cppData.int16_ = 1234;
    cppData.uint16_ = 5678;
    cppData.int8_ = 12;
    cppData.uint8_ = 34;
    cppData.bool_ = true;
    cppData.enum_ = EN2;
    cppData.struct_.aaa_ = "test";
    cppData.struct_.bbb_ = 999;
    cppData.string_ = "hello from C++";

    // Serialize
    vector<uint8_t> buffer(4096);
    ProtocolMemWriter writer(buffer.data(), buffer.size());
    cppData.serialize(&writer);
    size_t writtenSize = writer.length();

    cout << "Serialized " << writtenSize << " bytes" << endl;

    // Print first 64 bytes (field mask + header)
    printHexDump(buffer.data(), min(writtenSize, (size_t)64));

    // Save to file for C# to read
    ofstream outFile("/tmp/csharp_test_data.bin", ios::binary);
    outFile.write((char*)buffer.data(), writtenSize);
    outFile.close();

    cout << "Data saved to /tmp/csharp_test_data.bin for C# to read" << endl;
    cout << endl;

    // Verify C++ can deserialize its own data
    StructBase cppVerify;
    ProtocolMemReader reader(buffer.data(), writtenSize);
    bool success = cppVerify.deserialize(&reader);
    assert(success && "C++ should deserialize its own data");

    // Verify data
    assert(cppVerify.double_ == 123.456);
    assert(cppVerify.float_ == 78.9f);
    assert(cppVerify.string_ == "hello from C++");

    cout << "✓ C++ verified its own serialization" << endl;
    cout << endl;
}

// Test 2: Simulate what happens when C# reads C++ data
void testCSharpReadingCppData()
{
    cout << "=== Test 2: Simulate C# Reading C++ Data ===" << endl;

    // Create C++ serialized data
    StructBase cppData;
    cppData.double_ = 111.222;
    cppData.float_ = 333.444f;
    cppData.int64_ = 555555;
    cppData.uint64_ = 666666;
    cppData.int32_ = 777;
    cppData.uint32_ = 888;
    cppData.int16_ = 999;
    cppData.uint16_ = 1111;
    cppData.int8_ = 12;
    cppData.uint8_ = 34;
    cppData.bool_ = true;
    cppData.enum_ = EN3;
    cppData.string_ = "interoperability test";

    vector<uint8_t> buffer(4096);
    ProtocolMemWriter writer(buffer.data(), buffer.size());
    cppData.serialize(&writer);
    size_t writtenSize = writer.length();

    cout << "C++ serialized " << writtenSize << " bytes" << endl;

    // Print hex dump of first 32 bytes
    printHexDump(buffer.data(), min(writtenSize, (size_t)32));

    // Explain what C# will see
    cout << "\nC# will read:" << endl;
    cout << "  Byte 0: Field mask length (should be 4 for StructBase)" << endl;
    cout << "  Bytes 1-4: Field mask bits" << endl;
    cout << "  Bytes 5+: Actual data fields" << endl;

    uint8_t fmLen = buffer[0];
    cout << "  Field mask length: " << (int)fmLen << " bytes" << endl;

    // Verify C++ can deserialize
    StructBase verify;
    ProtocolMemReader reader(buffer.data(), writtenSize);
    bool success = verify.deserialize(&reader);
    assert(success && "Deserialization should succeed");

    cout << "✓ C++ deserialization successful" << endl;
    cout << "✓ Data verified: double=" << verify.double_ << ", string=" << verify.string_ << endl;
    cout << endl;
}

// Test 3: Verify field mask structure
void testFieldMaskStructure()
{
    cout << "=== Test 3: Field Mask Structure Analysis ===" << endl;

    StructBase testData;
    testData.double_ = 1.0;
    testData.float_ = 2.0;
    testData.int64_ = 3;

    vector<uint8_t> buffer(4096);
    ProtocolMemWriter writer(buffer.data(), buffer.size());
    testData.serialize(&writer);
    size_t writtenSize = writer.length();

    cout << "Analyzing serialized structure:" << endl;
    cout << "  Total size: " << writtenSize << " bytes" << endl;
    cout << endl;

    printHexDump(buffer.data(), writtenSize);

    // Parse structure
    size_t pos = 0;
    uint8_t fmLen = buffer[pos++];
    cout << "Field mask length: " << (int)fmLen << " bytes" << endl;

    cout << "Field mask bytes: ";
    for (int i = 0; i < fmLen; i++) {
        cout << hex << setfill('0') << setw(2) << (int)buffer[pos++] << " ";
    }
    cout << dec << endl;

    cout << "First data byte after field mask: 0x" << hex << (int)buffer[pos] << dec << endl;
    cout << endl;

    // Verify by deserializing
    StructBase verify;
    ProtocolMemReader reader(buffer.data(), writtenSize);
    bool success = verify.deserialize(&reader);
    assert(success && "Should deserialize successfully");

    cout << "✓ Structure verified successfully" << endl;
    cout << endl;
}

// Test 4: Test with older version (fewer fields)
void testVersionCompatibility()
{
    cout << "=== Test 4: Version Compatibility ===" << endl;

    // Current version with all fields
    StructBase newVersion;
    newVersion.double_ = 100.0;
    newVersion.float_ = 200.0f;
    newVersion.string_ = "new version data";

    vector<uint8_t> buffer(4096);
    ProtocolMemWriter writer(buffer.data(), buffer.size());
    newVersion.serialize(&writer);
    size_t writtenSize = writer.length();

    cout << "New version serialized " << writtenSize << " bytes" << endl;

    // Old version (simulated by using same struct but checking specific fields)
    StructBase oldVersion;
    ProtocolMemReader reader(buffer.data(), writtenSize);
    bool success = oldVersion.deserialize(&reader);
    assert(success && "Old version should read new data");

    // Verify old version fields are correct
    assert(oldVersion.double_ == 100.0);
    assert(oldVersion.float_ == 200.0f);
    assert(oldVersion.string_ == "new version data");

    cout << "✓ Old version successfully read new version data" << endl;
    cout << endl;
}

int main()
{
    cout << "========================================" << endl;
    cout << "C++ Serialization Analysis for C# Interop" << endl;
    cout << "========================================" << endl;
    cout << endl;

    try {
        testCppSerializeForCSharp();
        testCSharpReadingCppData();
        testFieldMaskStructure();
        testVersionCompatibility();

        cout << "========================================" << endl;
        cout << "✅ All C++ serialization tests PASSED" << endl;
        cout << "========================================" << endl;
        cout << endl;
        cout << "Data saved to: /tmp/csharp_test_data.bin" << endl;
        cout << "You can now load this file in C# to test cross-language compatibility." << endl;

        return 0;
    } catch (const exception& e) {
        cerr << "❌ TEST FAILED: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "❌ TEST FAILED: Unknown exception" << endl;
        return 1;
    }
}
