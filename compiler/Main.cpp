#include "Compiler.h"
#include <string>
#include <iostream>
#include <iterator>
#include "Args.h"

/** rpc entry point. */
int main( int argc, char *argv[] )
{

	try 
	{
		Args args(argc, argv, ";i;o;g;");

		Compiler::inst().generator_ = args.GetCString('g');
		Compiler::inst().inputFileName_ = args.GetCString('i');
		std::string outDir = args.GetCString('o');
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
