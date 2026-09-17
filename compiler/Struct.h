
#ifndef __Struct_h__
#define __Struct_h__

#include <vector>

#include "Definition.h"
#include "Field.h"

/** User-defined aggregate type (like a C++ struct; supports inheritance). */
class Struct : 
	public Definition,
	public FieldContainer
{
public:
	Struct():
	Definition(),
	super_(NULL)
	{}
	Struct(const std::string& f, const std::string& n):
	Definition(f, n),
	super_(NULL)
	{}

	virtual Struct*	getStruct()	{ return this; }

	/** Return whether a member field exists. */
	bool findField( const std::string& name );

	/** Number of fields. */
	size_t getFieldNum();

	Struct*				super_;		///< Base struct, if any.
	std::string					cppcode_;	///< Embedded C++ snippet.
};


#endif//__Struct_h__