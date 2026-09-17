#ifndef __Enum_h__
#define __Enum_h__

#include <vector>
#include "Definition.h"
#include "Field.h"

/** Enum definition (similar to a C++ enum). */
class Enum : public Definition
{
public:
	Enum():Definition(){}
	Enum(const std::string& f, const std::string& n)
		:Definition(f, n)
	{
	}

	/** Return whether an enumerator label exists. */
	bool findItem( const std::string& item );
	virtual Enum* getEnum() { return this; }

	/* There is no "underlying type" member. An enum travels as a single uint8
	   (at most 256 enumerators), so a declared width could never widen the
	   representable range; the backends emit their own fixed 32-bit declaration
	   instead. See the note in rpc.y. */
	std::vector< std::string >	items_;	///< Enumerator names.
};


#endif//__Enum_h__
