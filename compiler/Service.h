#ifndef __Service_h__
#define __Service_h__

#include "Definition.h"
#include "Field.h"

/** Service method (like a C function declaration). */
class Method : public FieldContainer
{
public:
	Method()
	{}

	const std::string& getName()	{ return name_; }
	const char* getNameC()			{ return name_.c_str(); }
	void setName(const std::string& n) { name_ = n; }
private:
	std::string				name_;				///< Method name.
};


class Service : public Definition
{
public:
	Service():
	Definition(),
	super_(NULL)
	{}
	Service(const std::string&f, const std::string& n):
	Definition(f, n),
	super_(NULL)
	{}

	virtual Service* getService()	{ return this; }

	/** Return whether a method name exists. */
	bool findMethod( const std::string& name );

	/** Collect base services in inheritance order. */
	void getParents(std::vector<Service*>& parents);

	/** Method count. */
	size_t getMethodNum();

	/** Fill vector with all methods. */
	void getAllMethods(std::vector<Method*>& methods);

	Service*				super_;
	std::vector<Method>	methods_;
};


#endif//__Service_h__