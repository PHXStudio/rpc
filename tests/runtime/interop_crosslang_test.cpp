/* Feeds the InteropFull golden vector to the C# backend and requires it to
   decode every field and re-encode the identical bytes.

   wire_format_golden_test.cpp asserts these bytes from C++,
   tests/py/interop_test.py from Python, and tests/go/interop_golden_test.go
   from Go. This file closes the loop with C#, so all four backends are held to
   one hand-derived byte string.

   Compiles to a GTEST_SKIP placeholder when no .NET SDK was found at configure
   time, matching the other cross-language tests. */

#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#include "tests_config.h"

#include "InteropFull.h"
#include "ProtocolBytesWriter.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace {

/* The same vector as the C++, Python and Go tests. */
const char* kGoldenHex =
	"03 ff ff c0 ff 02 fd ff 04 00 fb ff ff ff 06 00 00 00"
	" f9 ff ff ff ff ff ff ff 08 00 00 00 00 00 00 00"
	" 00 00 c0 3f 00 00 00 00 00 00 04 40"
	" 01 02 68 69 02 aa bb 01 c0 09 00 00 00"
	" 02 01 00 00 00 02 00 00 00 02 01 61 02 62 63";

std::string toHex(const std::vector<uint8_t>& v) {
	static const char* kDigits = "0123456789abcdef";
	std::string s;
	for (size_t i = 0; i < v.size(); i++) {
		if (i) s += ' ';
		s += kDigits[v[i] >> 4];
		s += kDigits[v[i] & 0x0F];
	}
	return s;
}

/** "01 60" -> {0x01, 0x60} */
std::vector<uint8_t> fromHex(const std::string& hex) {
	std::vector<uint8_t> out;
	int hi = -1;
	for (size_t i = 0; i < hex.size(); i++) {
		const char c = hex[i];
		int d;
		if (c >= '0' && c <= '9') d = c - '0';
		else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
		else continue;
		if (hi < 0) hi = d;
		else { out.push_back((uint8_t)((hi << 4) | d)); hi = -1; }
	}
	return out;
}

int shellExitStatus(int status) {
#ifdef _WIN32
	return status;
#else
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
#endif
}

/* PID-isolated so parallel ctest processes cannot collide. */
std::string goldenFilePath() {
#ifdef _WIN32
	const int pid = _getpid();
#else
	const int pid = (int)getpid();
#endif
	char buf[64];
	std::snprintf(buf, sizeof(buf), "interop_golden_%d.bin", pid);
	return buf;
}

void fillPayload(InteropPayload& p) {
	p.i8_ = -1;
	p.u8_ = 2;
	p.i16_ = -3;
	p.u16_ = 4;
	p.i32_ = -5;
	p.u32_ = 6;
	p.i64_ = -7;
	p.u64_ = 8;
	p.f32_ = 1.5f;
	p.f64_ = 2.5;
	p.b_ = true;
	p.kind_ = (InteropEnum)1;
	p.text_ = "hi";
	p.blob_.push_back(0xAA);
	p.blob_.push_back(0xBB);
	p.inner_.x_ = 9;
	p.inner_.flag_ = true;
	p.ints_.push_back(1);
	p.ints_.push_back(2);
	p.words_.push_back("a");
	p.words_.push_back("bc");
}

} // namespace

/* The C++ encoder must produce exactly the shared golden bytes. */
TEST(InteropCrossLang, CppMatchesSharedGolden) {
	InteropPayload p;
	fillPayload(p);

	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	p.serialize(&w);

	EXPECT_EQ(toHex(buf), std::string(kGoldenHex));
}

#if RPC_HAVE_DOTNET
TEST(InteropCrossLang, CsDecodesGoldenVector) {
	const std::vector<uint8_t> golden = fromHex(kGoldenHex);
	ASSERT_FALSE(golden.empty());

	const std::string path = goldenFilePath();
	{
		std::ofstream f(path.c_str(), std::ios::binary);
		ASSERT_TRUE(f.good());
		f.write((const char*)golden.data(), (std::streamsize)golden.size());
	}

	std::string cmd = "\"" + std::string(RPC_TEST_DOTNET_EXE) + "\" exec \""
		+ std::string(RPC_TEST_INTEROP_VERIFIER_DLL) + "\" verify-read \"" + path + "\"";

	const int rc = shellExitStatus(std::system(cmd.c_str()));
	EXPECT_EQ(rc, 0) << "C# InteropVerifier rejected the shared golden vector";

	std::remove(path.c_str());
}
#else
/* Registered under its real name so it still shows up as skipped rather than
   disappearing from the test list -- see cross_lang_file_test.cpp. */
TEST(InteropCrossLang, CsDecodesGoldenVector) {
	GTEST_SKIP() << "dotnet was not found at CMake configure time; C# interop test disabled.";
}
#endif
