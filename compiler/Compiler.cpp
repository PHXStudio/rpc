#include "Compiler.h"
#include <stdio.h>
#include "CodeGenerator.h"
#include "CppGenerator.h"
#include "ASGenerator.h"
#include "CSGenerator.h"
#include "ERLGenerator.h"
#include "PYGenerator.h"

Compiler& Compiler::inst()
{
	static Compiler spec;
	return spec;
}

Compiler::Compiler():
curLineno_(1)
{}

Compiler::~Compiler()
{
	// Free parsed definitions.
	for(size_t i = 0; i < definitions_.size(); i++)
		delete definitions_[i];
}

Definition* Compiler::findDefinition(const std::string& name)
{
	for(size_t i = 0; i < definitions_.size(); i++)
	{
		if(definitions_[i]->getName() == name)
			return definitions_[i];
	}

	return NULL;
}

int yyparse();
extern FILE *yyin;

int Compiler::compile()
{
	// Open input.
	yyin = fopen(inputFileName_.c_str(), "r");
	if(yyin == NULL)
	{
		outputError("failed to open file \"%s\".", inputFileName_.c_str());
		return 1;
	}

	// Root file stem.
	filename_ = File::GetFileName(inputFileName_);
	fileStem_ = File::GetFileBaseName(inputFileName_);
	curFilename_ = filename_;

	// Add input file path to import paths.
	std::string curPath = File::GetParentPath(inputFileName_);
	curPath += "/";
	importPaths_.insert(importPaths_.begin(), curPath);

	// Parse.
	int r;
	if(r = yyparse())
	{
		fclose(yyin);
		return r;
	}

	fclose(yyin);

	// Pick generator backend.
	CppGenerator cppGen;
	//ASGenerator as3Gen;
	CSGenerator csGen;
	ERLGenerator erlGen;
	PYGenerator pyGen;
	CodeGenerator* gen = &cppGen;
	if(generator_ == "cpp")
		gen = &cppGen;
	/*else if(generator_ == "as3")
		gen = &as3Gen;*/
	else if(generator_ == "cs")
		gen = &csGen;
	else if(generator_ == "erl")
		gen = &erlGen;
	else if(generator_ == "py")
		gen = &pyGen;

	// Emit code.
	try
	{
		gen->generate();
	}
	catch (const char* e)
	{
		outputError(e);
		return 1;
	}

	return 0;
}

#include <cstdio>
#include <cstdarg>
#define GET_VARARGS( str, len, fmt ) char str[len];va_list argptr;	va_start( argptr, fmt );vsprintf( str, fmt, argptr );va_end( argptr );
void Compiler::outputError(const char* e, ...)
{
	GET_VARARGS(str, 1024, e);
	fprintf(stderr, "%s\n", str);
}

void Compiler::outputErrorFL(const char* e, ...)
{
	GET_VARARGS(str, 1024, e);
	fprintf(stderr, "%s(%d):%s\n", curFilename_.c_str(), curLineno_, str);
}
