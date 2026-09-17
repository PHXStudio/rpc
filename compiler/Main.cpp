#include "Compiler.h"
#include <string>
#include <iostream>
#include <iterator>
#include "Args.h"
#include "Config.h"

/** rpc entry point. */
int main( int argc, char *argv[] )
{

	try
	{
		Args args(argc, argv, ";i;o;g;v;", ";v=version;");

		// --version is handled first, on purpose: it has to work with no schema
		// and no output directory, and it is what CI uses to check that a
		// released binary names the commit it was built from.
		if (args.Has('v'))
		{
			std::cout << "rpc " << RPC_VERSION_STRING << std::endl;
			return 0;
		}

		// GetCString() returns NULL for an option that was never given, and
		// building a std::string from NULL is undefined behaviour. All three of
		// these used to assign unconditionally, so a bare `rpc` with no
		// arguments segfaulted instead of reporting the missing input file.
		// Staying with the constructed empties lets compile() report it.
		const char* generator = args.GetCString('g');
		if (NULL != generator)
			Compiler::inst().generator_ = generator;

		const char* input = args.GetCString('i');
		if (NULL != input)
			Compiler::inst().inputFileName_ = input;

		std::string outDir;
		const char* output = args.GetCString('o');
		if (NULL != output)
			outDir = output;

		if (!outDir.empty()) {
			const char last = outDir[outDir.size() - 1];
			if (last != '/' && last != '\\')
				outDir.push_back('/');
		}
		Compiler::inst().outputDir_ = outDir;
		
	}
	catch(...) 
	{
		return 1;
	}

	// Run compile.
	return Compiler::inst().compile();
}
