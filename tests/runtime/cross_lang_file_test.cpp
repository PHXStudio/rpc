#include <gtest/gtest.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "CrossLangTest.h"
#include "ProtocolBytesReader.h"
#include "ProtocolBytesWriter.h"
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

const int32_t kTestI32 = -19088743;
const char* kTestUtf8 = u8"привет/hello";
const std::vector<uint8_t> kTestBytes = {0x01, 0x02, 0xDE, 0xAD, 0xBE, 0xEF};

static int shellExitStatus(int status) {
#ifdef _WIN32
	return status;
#else
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
#endif
}

#ifdef _WIN32
static int rpcPid() { return static_cast<int>(_getpid()); }
#else
static int rpcPid() { return static_cast<int>(getpid()); }
#endif

std::string tempDirBase() {
#ifdef _WIN32
	char buf[MAX_PATH + 2];
	const DWORD n = GetTempPathA(static_cast<DWORD>(sizeof(buf)), buf);
	if (n > 0 && n < sizeof(buf))
		return std::string(buf);
#endif
	const char* d = std::getenv("TMPDIR");
	if (!d) d = std::getenv("TEMP");
	if (!d) d = std::getenv("TMP");
	if (!d) d = "/tmp";
	return std::string(d);
}

std::string makeTempPath(const char* tag) {
	static int counter;
	std::string base = tempDirBase();
	if (!base.empty()) {
		const char lc = base[base.size() - 1];
		if (lc != '/' && lc != '\\')
#ifdef _WIN32
			base += '\\';
#else
			base += '/';
#endif
	}
	std::ostringstream o;
	o << base << "rpc_crosslang_" << tag << "_" << rpcPid() << "_" << (counter++) << ".bin";
	return o.str();
}

void writeAll(const std::string& path, const std::vector<uint8_t>& data) {
	std::ofstream f(path, std::ios::binary);
	ASSERT_TRUE(f) << "open for write: " << path;
	f.write(reinterpret_cast<const char*>(data.data()),
		static_cast<std::streamsize>(data.size()));
	ASSERT_TRUE(f) << "write: " << path;
}

bool readAll(const std::string& path, std::vector<uint8_t>& out) {
	std::ifstream f(path, std::ios::binary | std::ios::ate);
	if (!f)
		return false;
	const std::streamoff sz = f.tellg();
	if (sz <= 0)
		return false;
	f.seekg(0);
	out.resize(static_cast<size_t>(sz));
	f.read(reinterpret_cast<char*>(out.data()), sz);
	return static_cast<bool>(f);
}

std::string toHex(const std::vector<uint8_t>& v, size_t maxBytes = 64) {
	std::ostringstream o;
	o << std::hex << std::setfill('0');
	const size_t n = std::min(maxBytes, v.size());
	for (size_t i = 0; i < n; ++i)
		o << std::setw(2) << static_cast<int>(v[i]);
	if (v.size() > maxBytes)
		o << "...(" << v.size() << " bytes)";
	return o.str();
}

std::string utf8ToHex(const std::string& utf8) {
	std::ostringstream o;
	o << std::hex << std::setfill('0');
	for (unsigned char c : utf8)
		o << std::setw(2) << static_cast<int>(c);
	return o.str();
}

std::string bytesToHex(const std::vector<uint8_t>& b) {
	std::ostringstream o;
	o << std::hex << std::setfill('0');
	for (uint8_t c : b)
		o << std::setw(2) << static_cast<int>(c);
	return o.str();
}

void fillPayload(CrossLangPayload& p) {
	p.i32_ = kTestI32;
	p.s_ = kTestUtf8;
	p.b_ = kTestBytes;
}

void expectPayload(const CrossLangPayload& p) {
	EXPECT_EQ(p.i32_, kTestI32);
	EXPECT_EQ(p.s_, kTestUtf8);
	ASSERT_EQ(p.b_.size(), kTestBytes.size());
	EXPECT_EQ(p.b_, kTestBytes);
}

#if RPC_HAVE_DOTNET
/** Forward slashes in cmd.exe-quoted paths avoid `\\"` escape issues on Windows. */
static std::string cmdLinePath(const std::string& p) {
#ifdef _WIN32
	std::string o;
	o.reserve(p.size());
	for (char c : p)
		o += (c == '\\') ? '/' : c;
	return o;
#else
	return p;
#endif
}

int runDotnetVerifier(const std::vector<std::string>& args) {
	std::ostringstream cmd;
	// `dotnet run` re-evaluates the project and can stall; tests use `dotnet exec` on the CMake-built DLL.
#ifdef _WIN32
	cmd << "set \"DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1\" && set \"DOTNET_CLI_TELEMETRY_OPTOUT=1\" && set "
		   "\"DOTNET_NOLOGO=1\" && ";
#else
	cmd << "env DOTNET_SKIP_FIRST_TIME_EXPERIENCE=1 DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1 ";
#endif
	cmd << "\"" << cmdLinePath(RPC_TEST_DOTNET_EXE) << "\" exec \"" << cmdLinePath(RPC_TEST_VERIFIER_DLL) << "\" ";
	for (const auto& a : args) {
		cmd << " \"";
		for (char c : a) {
			if (c == '"' || c == '\\')
				cmd << '\\';
			cmd << c;
		}
		cmd << '"';
	}
	return shellExitStatus(std::system(cmd.str().c_str()));
}
#endif

} // namespace

TEST(CrossLangPayloadMemory, CppRoundtrip) {
	CrossLangPayload original;
	fillPayload(original);
	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	original.serialize(&w);

	ProtocolBytesReader r(buf);
	CrossLangPayload decoded;
	ASSERT_TRUE(decoded.deserialize(&r)) << "deserialize failed, hex=" << toHex(buf);
	expectPayload(decoded);
}

#if RPC_HAVE_DOTNET
TEST(CrossLangFile, CppProducesCsConsumes) {
	const std::string path = makeTempPath("cpp2cs");
	CrossLangPayload p;
	fillPayload(p);
	std::vector<uint8_t> buf;
	ProtocolBytesWriter w(buf);
	p.serialize(&w);
	writeAll(path, buf);

	const std::string strHex = utf8ToHex(std::string(kTestUtf8));
	const std::string bytesHex = bytesToHex(kTestBytes);
	const int rc = runDotnetVerifier(
		{"verify-read", path, std::to_string(static_cast<long long>(kTestI32)), strHex, bytesHex});
	EXPECT_EQ(rc, 0) << "dotnet verify-read failed for " << path << " hex=" << toHex(buf);
	::remove(path.c_str());
}

TEST(CrossLangFile, CsProducesCppConsumes) {
	const std::string path = makeTempPath("cs2cpp");
	const std::string strHex = utf8ToHex(std::string(kTestUtf8));
	const std::string bytesHex = bytesToHex(kTestBytes);
	const int rc = runDotnetVerifier(
		{"write", path, std::to_string(static_cast<long long>(kTestI32)), strHex, bytesHex});
	ASSERT_EQ(rc, 0) << "dotnet write failed path=" << path;

	std::vector<uint8_t> fileBytes;
	ASSERT_TRUE(readAll(path, fileBytes)) << "read file: " << path;
	ASSERT_FALSE(fileBytes.empty()) << "empty file: " << path;
	ProtocolBytesReader r(fileBytes);
	CrossLangPayload decoded;
	ASSERT_TRUE(decoded.deserialize(&r))
		<< "C++ deserialize failed file=" << path << " hex=" << toHex(fileBytes);
	expectPayload(decoded);
	::remove(path.c_str());
}
#else
TEST(CrossLangFile, SkippedNoDotnet) {
	GTEST_SKIP() << "dotnet was not found at CMake configure time; cross-language file tests disabled.";
}
#endif
