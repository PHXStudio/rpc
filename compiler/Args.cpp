#include "Args.h"
#include <cstring>
#include<stdlib.h>
static const char Deli = ';';

Args::Args(int argc, char* argv[], const char* shortopts, const char* longopts)
{
	SetShortKey(shortopts);
	// Long names must be registered before Scan(): Scan matches long options by
	// name, and one with no token registered yet is dropped without a word.
	if (NULL != longopts)
		SetLongKey(longopts);
	Scan(argc, argv);
}

void Args::SetShortKey(const char* shortopts)
{
	for (size_t i = 0, l = strlen(shortopts); i < l; ++i)
	{
		if (shortopts[i] == Deli)
		{
			continue;
		}

		Token token;
		token.ShortKey = shortopts[i];
		token.Seen = false;
		Tokens.push_back(token);
	}
}

void Args::SetLongKey(char shortkey, const char* longkey)
{
	for (size_t i = 0; i < Tokens.size(); ++i)
	{
		if (Tokens[i].ShortKey == shortkey)
		{
			Tokens[i].LongKey = longkey;
			return;
		}
	}
}

void Args::SetLongKey(const char* longopts)
{
	if (NULL == longopts)
		return;

	const std::string spec(longopts);
	size_t pos = 0;
	while (pos < spec.size())
	{
		size_t end = spec.find(Deli, pos);
		if (end == std::string::npos)
			end = spec.size();

		const std::string item = spec.substr(pos, end - pos);
		pos = end + 1;

		const size_t eq = item.find('=');
		if (item.empty() || eq == std::string::npos || eq == 0)
			continue;

		SetLongKey(item[0], item.substr(eq + 1).c_str());
	}
}

bool Args::Has(char shortkey)
{
	Token* ptoken = GetToken(shortkey);
	return NULL != ptoken && ptoken->Seen;
}

bool Args::Has(const char* longkey)
{
	Token* ptoken = GetToken(longkey);
	return NULL != ptoken && ptoken->Seen;
}

void Args::Scan(int argc, char* argv[])
{
	char shortkey = '\0';
	std::string longkey = "  ";
	for (int i = 0; i < argc; ++i)
	{
		size_t argsize = strlen(argv[i]);
		if (argv[i][0] == '-')
		{
			shortkey = '\0';
			longkey = "  ";

			if (argv[i][1] == '-' && argsize > 2)
			{
				longkey = (argv[i] + 2);
			}
			else
			{
				shortkey = argv[i][1];
			}

			// Record the match here rather than when a value arrives: a
			// flag-style option never has one, so waiting for a value would
			// leave it indistinguishable from an option that was never given.
			Token* pseen = GetToken(shortkey);
			if (NULL == pseen)
				pseen = GetToken(longkey.c_str());
			if (NULL != pseen)
				pseen->Seen = true;
		}
		else
		{
			Token* ptoken = GetToken(shortkey);
			if (NULL == ptoken)
				ptoken = GetToken(longkey.c_str());
			if (ptoken != NULL)
			{
				ptoken->Values.push_back(argv[i]);
			}
		}
	}
}

Args::Token* Args::GetToken(char shortkey)
{
	for (size_t i = 0; i < Tokens.size(); ++i)
	{
		if (Tokens[i].ShortKey == shortkey)
		{
			return &Tokens[i];
		}
	}
	return NULL;
}

Args::Token* Args::GetToken(const char* longkey)
{
	for (size_t i = 0; i < Tokens.size(); ++i)
	{
		if (Tokens[i].LongKey == longkey)
		{
			return &Tokens[i];
		}
	}
	return NULL;
}

const char* Args::GetCString(char shortkey, int index)
{
	Token* ptoken = GetToken(shortkey);
	if (NULL == ptoken)
		return NULL;
	if (index >= ptoken->Values.size())
		return NULL;
	return ptoken->Values[index].c_str();
}

const char* Args::GetCString(const char* longkey, int index)
{
	Token* ptoken = GetToken(longkey);
	if (NULL == ptoken)
		return NULL;
	if (index >= ptoken->Values.size())
		return NULL;
	return ptoken->Values[index].c_str();
}

int Args::GetInteger(char shortkey, int index)
{
	const char* c = GetCString(shortkey, index);
	if (NULL == c)
		return 0;
	return atoi(c);
}

int Args::GetInteger(const char* longkey, int index)
{
	const char* c = GetCString(longkey, index);
	if (NULL == c)
		return 0;
	return atoi(c);
}

float Args::GetFloat(char shortkey, int index)
{
	const char* c = GetCString(shortkey, index);
	if (NULL == c)
		return 0.F;
	return (float)atof(c);
}

float Args::GetFloat(const char* longkey, int index)
{
	const char* c = GetCString(longkey, index);
	if (NULL == c)
		return 0.F;
	return (float)atof(c);
}