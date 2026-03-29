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
		super_.setType(FT_INT32);
	}

	/** Return whether an enumerator label exists. */
	bool findItem( const std::string& item );
	Field &getSuperType(){return super_;}
	virtual Enum* getEnum() { return this; }

	Field						super_;
	std::vector< std::string >	items_;	///< Enumerator names.
};


#endif//__Enum_h__
