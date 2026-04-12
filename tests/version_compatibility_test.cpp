/* Version compatibility test for struct serialization. */
#include "../runtime/cpp/ProtocolMemWriter.h"
#include "../runtime/cpp/ProtocolMemReader.h"
#include "../output_cpp/FullTest.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cstring>

using namespace std;

void testOldReadsNew()
{
    cout << "Test: Old version reads new data..." << endl;

    StructBase newStruct;
    newStruct.double_ = 123.456;
    newStruct.float_ = 78.9f;
    newStruct.int64_ = 12345678901LL;
    newStruct.uint64_ = 9876543210ULL;
    newStruct.int32_ = 12345;
    newStruct.uint32_ = 67890;
    newStruct.int16_ = 1234;
    newStruct.uint16_ = 5678;
    newStruct.int8_ = 12;
    newStruct.uint8_ = 34;
    newStruct.bool_ = true;
    newStruct.enum_ = EN2;
    newStruct.struct_.aaa_ = "test";
    newStruct.struct_.bbb_ = 999;
    newStruct.string_ = "hello";

    vector<uint8_t> buffer(4096);
    ProtocolMemWriter writer(buffer.data(), buffer.size());
    newStruct.serialize(&writer);
    size_t writtenSize = writer.length();

    StructBase oldStruct;
    ProtocolMemReader reader(buffer.data(), writtenSize);
    bool success = oldStruct.deserialize(&reader);
    assert(success && "Old version should successfully deserialize new data");

    assert(oldStruct.double_ == 123.456);
    assert(oldStruct.float_ == 78.9f);
    assert(oldStruct.int64_ == 12345678901LL);
    assert(oldStruct.uint64_ == 9876543210ULL);
    assert(oldStruct.int32_ == 12345);
    assert(oldStruct.uint32_ == 67890);
    assert(oldStruct.int16_ == 1234);
    assert(oldStruct.uint16_ == 5678);
    assert(oldStruct.int8_ == 12);
    assert(oldStruct.uint8_ == 34);
    assert(oldStruct.bool_ == true);
    assert(oldStruct.enum_ == EN2);
    assert(oldStruct.struct_.aaa_ == "test");
    assert(oldStruct.struct_.bbb_ == 999);
    assert(oldStruct.string_ == "hello");

    cout << "  PASSED: Old version correctly reads new data" << endl;
}

void testNewReadsOld()
{
    cout << "Test: New version reads old data..." << endl;

    StructBase oldStruct;
    oldStruct.double_ = 123.456;
    oldStruct.float_ = 78.9f;
    oldStruct.int64_ = 12345678901LL;
    oldStruct.uint64_ = 9876543210ULL;
    oldStruct.int32_ = 12345;
    oldStruct.uint32_ = 67890;
    oldStruct.int16_ = 1234;
    oldStruct.uint16_ = 5678;
    oldStruct.int8_ = 12;
    oldStruct.uint8_ = 34;
    oldStruct.bool_ = true;
    oldStruct.enum_ = EN2;
    oldStruct.struct_.aaa_ = "test";
    oldStruct.struct_.bbb_ = 999;
    oldStruct.string_ = "hello";

    vector<uint8_t> buffer(4096);
    ProtocolMemWriter writer(buffer.data(), buffer.size());
    oldStruct.serialize(&writer);
    size_t writtenSize = writer.length();

    StructBase newStruct;
    ProtocolMemReader reader(buffer.data(), writtenSize);
    bool success = newStruct.deserialize(&reader);
    assert(success && "New version should successfully deserialize old data");

    assert(newStruct.double_ == 123.456);
    assert(newStruct.float_ == 78.9f);
    assert(newStruct.int64_ == 12345678901LL);
    assert(newStruct.uint64_ == 9876543210ULL);
    assert(newStruct.int32_ == 12345);
    assert(newStruct.uint32_ == 67890);
    assert(newStruct.int16_ == 1234);
    assert(newStruct.uint16_ == 5678);
    assert(newStruct.int8_ == 12);
    assert(newStruct.uint8_ == 34);
    assert(newStruct.bool_ == true);
    assert(newStruct.enum_ == EN2);
    assert(newStruct.struct_.aaa_ == "test");
    assert(newStruct.struct_.bbb_ == 999);
    assert(newStruct.string_ == "hello");

    cout << "  PASSED: New version correctly reads old data" << endl;
}

void testFieldMaskLength()
{
    cout << "Test: Field mask length prefix..." << endl;

    StructBase testStruct;
    testStruct.double_ = 1.0;
    testStruct.float_ = 2.0f;
    testStruct.string_ = "test";

    vector<uint8_t> buffer(4096);
    ProtocolMemWriter writer(buffer.data(), buffer.size());
    testStruct.serialize(&writer);
    size_t writtenSize = writer.length();

    assert(writtenSize > 0 && "Serialized data should not be empty");
    uint8_t fmLen = buffer[0];
    assert(fmLen == 4 && "Field mask length should be 4 bytes for StructBase");

    StructBase result;
    ProtocolMemReader reader(buffer.data(), writtenSize);
    bool success = result.deserialize(&reader);
    assert(success && "Should successfully deserialize");

    assert(result.double_ == 1.0);
    assert(result.float_ == 2.0f);
    assert(result.string_ == "test");

    cout << "  PASSED: Field mask length prefix is correct" << endl;
}

void testSkipFunctionality()
{
    cout << "Test: Skip functionality..." << endl;

    vector<uint8_t> buffer(1024);
    memset(buffer.data(), 0, buffer.size());

    buffer[0] = 10;
    memset(buffer.data() + 1, 0xFF, 10);
    buffer[11] = 42;

    ProtocolMemReader reader(buffer.data(), buffer.size());

    uint8_t actualFmLen = 0;
    bool success = reader.readType(actualFmLen);
    assert(success && actualFmLen == 10);

    uint8_t readFmLen = 5;
    uint8_t fmData[10];
    success = reader.read(fmData, readFmLen);
    assert(success);

    success = reader.skip(actualFmLen - readFmLen);
    assert(success);

    uint8_t nextByte = 0;
    success = reader.readType(nextByte);
    assert(success && nextByte == 42);

    cout << "  PASSED: Skip functionality works correctly" << endl;
}

int main()
{
    cout << "=== Version Compatibility Tests ===" << endl;

    try {
        testFieldMaskLength();
        testSkipFunctionality();
        testOldReadsNew();
        testNewReadsOld();

        cout << "\n=== All tests PASSED ===" << endl;
        return 0;
    } catch (const exception& e) {
        cerr << "TEST FAILED: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "TEST FAILED: Unknown exception" << endl;
        return 1;
    }
}
