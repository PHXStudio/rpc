#ifndef __EnumInfo_h__
#define __EnumInfo_h__

#include "Common.h"

/** Runtime enum metadata (name <-> id). */
class EnumInfo
{
public:
	EnumInfo(const std::string& name, void (*initFunc)(EnumInfo* einfo)):
	name_(name)
	{
		initFunc(this);
	}

	/** Map a string label to an enumerator index.
		@param item Enumerator name.
		@return -1 if not found.
	*/
	int getItemId(const std::string& item)
	{
		std::vector<std::string>::iterator r;
		r = std::find(items_.begin(), items_.end(), item);
		if(r == items_.end())
			return -1;
		return (int)(r - items_.begin());
	}

	/** Map an enumerator index to its name.
		@param item Enumerator index.
		@return NULL if out of range.
	*/
	const char* getItemName(int item)
	{
		if(item < 0 || item >= (int)items_.size())
			return NULL;
		return items_[item].c_str();
	}

	std::string						name_;
	std::vector<std::string>		items_;
};

/** Macro to access generated enum runtime info. */
#define ENUM(E)	enum##E


#endif//__EnumInfo_h__
