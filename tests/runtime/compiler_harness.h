/* Shared helpers for tests that drive the rpc compiler as a subprocess.
   Extracted from compiler_test.cpp so the import and negative-path suites can
   reuse the same invocation and assertion primitives. */
#ifndef RPC_TESTS_COMPILER_HARNESS_H
#define RPC_TESTS_COMPILER_HARNESS_H

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#include "tests_config.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace rpc_test {

/** Normalise a std::system() status into a process exit code. */
inline int shellExitStatus(int status) {
#ifdef _WIN32
	return status;
#else
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	return -1;
#endif
}

inline std::string makeOutDir(const std::string& subdir) {
	return std::string(RPC_TEST_OUTPUT_DIR) + "/" + subdir;
}

inline void mkdirp(const std::string& dir) {
	std::string cmd =
#ifdef _WIN32
		"mkdir \"" + dir + "\" 2>NUL";
#else
		"mkdir -p \"" + dir + "\"";
#endif
	// The directory usually already exists, so a failure here is not fatal --
	// the compiler invocation that follows reports it more usefully. The
	// result still has to be consumed, though: casting to void is not enough
	// for GCC's -Wunused-result, only actually using the value is.
	if (std::system(cmd.c_str()) != 0)
		std::fprintf(stderr, "mkdirp: could not create '%s'\n", dir.c_str());
}

/** Run the rpc compiler and return its exit status. */
inline int runCompiler(const std::string& inputFile, const std::string& outputDir,
                       const std::string& generator) {
	std::ostringstream cmd;
	cmd << "\"" << RPC_TEST_COMPILER_EXE << "\"";
	cmd << " -i \"" << inputFile << "\"";
	cmd << " -o \"" << outputDir << "/\"";
	cmd << " -g " << generator;

	return shellExitStatus(std::system(cmd.str().c_str()));
}

/** Run the rpc compiler with a raw argument string, capturing its output.

    std::system() offers no way to read what the child wrote, so stdout and
    stderr are redirected to a file first and read back afterwards. */
inline int runCompilerArgs(const std::string& args, std::string* output) {
	const std::string dir = makeOutDir("args");
	mkdirp(dir);
	const std::string capture = dir + "/output.txt";

	std::ostringstream cmd;
	cmd << "\"" << RPC_TEST_COMPILER_EXE << "\" " << args;
	cmd << " > \"" << capture << "\" 2>&1";

	const int status = shellExitStatus(std::system(cmd.str().c_str()));

	if (output != NULL) {
		std::ifstream f(capture.c_str());
		if (!f) {
			output->clear();
		} else {
			output->assign(std::istreambuf_iterator<char>(f),
			               std::istreambuf_iterator<char>());
		}
	}
	return status;
}

inline bool fileContains(const std::string& path, const std::string& needle) {
	std::ifstream f(path);
	if (!f) return false;
	std::string content((std::istreambuf_iterator<char>(f)),
	                     std::istreambuf_iterator<char>());
	return content.find(needle) != std::string::npos;
}

inline bool fileExists(const std::string& path) {
	std::ifstream f(path);
	return static_cast<bool>(f);
}

inline std::string schemaPath(const std::string& name) {
	return std::string(RPC_TEST_SCHEMA_DIR) + "/" + name;
}

} // namespace rpc_test

#endif // RPC_TESTS_COMPILER_HARNESS_H
