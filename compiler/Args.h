#ifndef __OPTIONS_H__
#define __OPTIONS_H__
#include <string>
#include <vector>

class Args
{
	struct Token
	{
		char					 ShortKey;
		std::string				 LongKey;
		std::vector<std::string> Values;
		/** The option appeared on the command line, with or without a value. */
		bool					 Seen;
	};
public:
	Args(int argc, char* argv[], const char* shortopts, const char* longopts = NULL);

	void SetShortKey(const char* shortopts);

	void SetLongKey(char shortkey, const char* longkey);

	/** Register long option names. Expected form: ";v=version;i=input;". */
	void SetLongKey(const char* longopts);

	void Scan(int argc, char* argv[]);

	/**
	 * Whether an option was actually given on the command line.
	 *
	 * Neither existing accessor can answer this. GetToken() cannot: SetShortKey()
	 * creates a token for every character in shortopts, so GetToken('v') is never
	 * NULL. GetCString() cannot either: a flag-style option such as --version
	 * takes no value, so Values stays empty and GetCString() always returns NULL.
	 * Hence the Seen flag, set by Scan() when it matches an option.
	 */
	bool Has(char shortkey);
	bool Has(const char* longkey);

	Token* GetToken(char shortkey);
	Token* GetToken(const char* longkey);

	const char* GetCString(char shortkey, int index = 0);
	const char* GetCString(const char* longkey, int index = 0);

	int GetInteger(char shortkey, int index = 0);
	int GetInteger(const char* longkey, int index = 0);

	float GetFloat(char shortkey, int index = 0);
	float GetFloat(const char* longkey, int index = 0);

private:
	std::vector<Token> Tokens;
};

#endif