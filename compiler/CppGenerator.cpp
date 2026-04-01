#include "CppGenerator.h"
#include "Compiler.h"
#include "CodeFile.h"
#include <limits>


static const char* getFieldCppType(Field& f, bool withArray = true)
{
	static std::string name;
	if(f.getArray() && withArray)
		name = "std::vector< ";
	else
		name = "";
	switch(f.getType())
	{
	case FT_INT64:
		name += "int64_t";
		break;
	case FT_UINT64:
		name += "uint64_t";
		break;
	case FT_DOUBLE:
		name += "double";
		break;
	case FT_FLOAT:
		name += "float";
		break;
	case FT_INT32:
		name += "int32_t";
		break;
	case FT_UINT32:
		name += "uint32_t";
		break;
	case FT_INT16:
		name += "int16_t";
		break;
	case FT_UINT16:
		name += "uint16_t";
		break;
	case FT_INT8:
		name += "int8_t";
		break;
	case FT_UINT8:
		name += "uint8_t";
		break;
	case FT_BOOL:
		name += "bool";
		break;
	case FT_STRING:
		name += "std::string";
		break;
	case FT_USER:
	case FT_ENUM:
		name += f.getUserType()->getName();
		break;
	default:
		throw "Invalid field type.";
	}

	if(f.getArray() && withArray)
		name += " >";
	return name.c_str();
}


static void generateEnumDecl(CodeFile& f, Enum* e)
{
	f.output("// enum %s", e->getNameC());
	f.output("enum %s : %s", e->getNameC(),getFieldCppType(e->getSuperType()));
	f.output("{");
	f.indent();
	for(size_t i = 0; i < e->items_.size(); i++)
		f.output("%s,", e->items_[i].c_str());
	f.recover();
	f.output("};");
	f.output("extern EnumInfo enum%s;", e->getNameC());
}

static const char* getFieldCppDefault(Field& f)
{
	static std::string name;
	if( f.getArray() )
		return NULL;
	name = "";
	switch( f.getType() )
	{
	case FT_INT64:
	case FT_UINT64:
	case FT_DOUBLE:
	case FT_FLOAT:
	case FT_INT32:
	case FT_UINT32:
	case FT_INT16:
	case FT_UINT16:
	case FT_INT8:
	case FT_UINT8:
		name = "0";
		break;
	case FT_BOOL:
		name = "false";
		break;
	case FT_ENUM:
		name = "(" + f.getUserType()->getName() + ")(0)";
		break;
	case FT_STRING:
		return NULL;
	case FT_USER:
		return NULL;
	default:
		throw "Invalid field type.";
	}

	return name.c_str();
}

static bool useParamReference(Field& f)
{
	if(f.getArray() || f.getType() == FT_STRING || f.getType() == FT_USER)
		return true;
	return false;
}

static void generateStubMethodDecl(CodeFile& f, Method& m)
{
	f.begin();
	f.append("void %s(", m.getNameC());
	for(size_t i = 0; i < m.fields_.size(); i++)
	{
		Field& field = m.fields_[i];
		f.append("%s%s%s %s%s", 
			useParamReference(field)?"const ":"",
			getFieldCppType(field),
			useParamReference(field)?"&":"",
			field.getNameC(),
			(i == m.fields_.size()-1)?"":",");
	}
	f.append(");");
	f.end();
}

static void generateProxyMethodDecl(CodeFile& f, Method& m, bool pureVirtual = true)
{
	f.begin();
	f.append("virtual bool %s(", m.getNameC());
	for(size_t i = 0; i < m.fields_.size(); i++)
	{
		Field& field = m.fields_[i];
		f.append("%s%s %s%s", 
			getFieldCppType(field),
			useParamReference(field)?"&":"",
			field.getNameC(),
			(i == m.fields_.size()-1)?"":", ");
	}
	f.append(")%s;", pureVirtual?" = 0":"");
	f.end();
}

static void generateStructDecl(CodeFile& f, Struct* s)
{
	/** struct.struct.
	*/
	f.output("// struct %s", s->getNameC());
	if(s->super_)
		f.output("struct %s : public %s", s->getNameC(), s->super_->getNameC());
	else
		f.output("struct %s", s->getNameC());
	f.output("{");
	f.indent();

	/** struct.
	*/
	f.output("// member list.");
	for(size_t i = 0; i < s->fields_.size(); i++)
	{
		Field& field = s->fields_[i];
		f.output("%s %s;", getFieldCppType(field), field.getNameC());
	}

	/** field id. 
		enum
		{
			FID_...,
			FID_...,
			FIDMAX,
		}
	*/
	f.output("// field ids.");
	f.output("enum");
	f.output("{");
	f.indent();
	size_t fid = s->super_?s->super_->getFieldNum():0;
	for(size_t i = 0; i < s->fields_.size(); i++)
	{
		Field& field = s->fields_[i];
		f.output("FID_%s = %d,", field.getNameC(), fid);
		fid++;
	}
	f.output("FIDMAX = %d,", fid);
	f.recover();
	f.output("};");

	/** struct.
		constructor.default valuefieldconstructor.
	*/
	bool needCtor = false;
	for(size_t i = 0; i < s->fields_.size(); i++)
	{
		Field& field = s->fields_[i];
		if(getFieldCppDefault(field))
		{
			needCtor = true;
			break;
		}
	}
	if(needCtor)
	{
		f.output("// constructor.");
		f.output("%s();", s->getNameC());
	}

	/** .
		void serialize(ProtocolWriter* s) const;
		bool deserialize(ProtocolReader* r);
	*/
	f.output("// serialization.");
	f.output("void serialize(ProtocolWriter* s) const;");
	f.output("// deserialization.");
	f.output("bool deserialize(ProtocolReader* r);");
	f.output("void toJson(std::ostream& ss, bool needBracket = true)const;");
	f.output("bool loadJson(const char* json, size_t len);");
	f.output("bool loadJson(const std::string& json);");
	f.output("bool loadJson(JsonReader& __rd__);");


	/** cppcode. */
	if(s->cppcode_.length())
	{
		f.output("// cppcode.");
		f.output("%s", s->cppcode_.c_str());
	}
	/** .
	*/
	f.recover();
	f.output("};");
}

static void generateStubDecl(CodeFile& f, Service* s)
{
	/** service stub class.
	*/
	f.output("// service stub %s", s->getNameC());
	if(s->super_)
		f.output("class %sStub : public %sStub", s->getNameC(), s->super_->getNameC());
	else
		f.output("class %sStub", s->getNameC());
	f.output("{");
	f.output("public:");
	f.indent();

	/** Methods */
	f.output("// methods.");
	for(size_t i = 0; i < s->methods_.size(); i++)
		generateStubMethodDecl(f, s->methods_[i]);

	if(!s->super_)
	{
		f.recover();
		f.output("protected:");
		f.indent();
		f.output("// events to be processed.");
		f.output("virtual ProtocolWriter* methodBegin() = 0;");
		f.output("virtual void methodEnd() = 0;");
	}
	f.recover();
	f.output("};");
}

static void generateProxyDecl(CodeFile& f, Service* s)
{
	/** service Proxy class.
	*/
	f.output("// service proxy %s", s->getNameC());
	if(s->super_)
		f.output("class %sProxy : public %sProxy", s->getNameC(), s->super_->getNameC());
	else
		f.output("class %sProxy", s->getNameC());
	f.output("{");
	f.output("public:");
	f.indent();

	// methods.
	f.output("// methods.");
	for(size_t i = 0; i < s->methods_.size(); i++)	
	{
		Method& method = s->methods_[i];
		generateProxyMethodDecl(f, method);
	}
	f.output("");
	
	// dispatch 
	f.output("// dispatch.");
	f.output("bool dispatch(ProtocolReader* reader);");

	// deserialization.
	f.recover();
	f.output("protected:");
	f.indent();

	f.output("// deserialization.");
	for(size_t i = 0; i < s->methods_.size(); i++)
		f.output("bool %s(ProtocolReader* r);", s->methods_[i].getNameC());

	f.recover();
	f.output("};");

}

static void generateEnumDef(CodeFile& f, Enum* e)
{
	f.output("static void initFunc%s(EnumInfo* e)", e->getNameC());
	f.output("{");
	f.indent();
	for(size_t i = 0; i < e->items_.size(); i++)
		f.output("	e->items_.push_back(\"%s\");", e->items_[i].c_str());
	f.recover();
	f.output("}");
	f.output("EnumInfo enum%s(\"%s\", initFunc%s);", 
		e->getNameC(),
		e->getNameC(),
		e->getNameC());
}

static void generateFieldSerialize(CodeFile& f, Field& field, const char* senderName, bool skipComp)
{
	f.output("// serialize %s",field.getNameC());
	if(field.getArray())
	{
		// 
		if(!skipComp)		f.output("if(%s.size())", field.getNameC());
							f.output("{");
							f.indent();
							f.output("uint32_t __len__ = (uint32_t)%s.size();", field.getNameC());
							f.output("%s->writeDynSize(__len__); ", senderName);
							f.output("for(uint32_t i = 0; i < __len__; i++)");
							f.output("{");
							f.indent();
		// .
		if(field.getType() == FT_USER)
		{
							f.output("%s[i].serialize(%s);", field.getNameC(), senderName);
		}
		else if(field.getType() == FT_ENUM)
		{
							f.output("uint8_t __e__ = (uint8_t)%s[i];", field.getNameC());
							f.output("%s->writeType(__e__);", senderName );
		}
		else if(field.getType() == FT_STRING)
		{
							f.output("%s->writeType(%s[i]);", senderName, field.getNameC());
		}
		else
		{
							f.output("%s->writeType(%s[i]);", senderName, field.getNameC());
		}
							f.recover();
							f.output("}");
							f.recover();
							f.output("}");
	}
	else
	{
							f.output("{");
							f.indent();
		if(field.getType() == FT_USER)
		{
							f.output("%s.serialize(%s);", field.getNameC(), senderName);
		}
		else if(field.getType() == FT_STRING)
		{
			if(!skipComp)	f.output("if(%s.length()){", field.getNameC());
							f.output("%s->writeType(%s);", senderName, field.getNameC());
			if(!skipComp)	f.output("}");
		}
		else if(field.getType() == FT_BOOL)
		{ 
			/* boolfieldmask.*/ 
			if(skipComp) 
			{
							f.output("%s->writeType(%s);", senderName, field.getNameC());
			}
		}
		else if(field.getType() == FT_ENUM)
		{
							f.output("uint8_t __e__ = (uint8_t)%s;", field.getNameC());
			if(!skipComp)	f.output("if(__e__){");
							f.output("%s->writeType(__e__);", senderName );
			if(!skipComp)	f.output("}");
		}
		else
		{
			if(!skipComp)	f.output("if(%s != %s){", field.getNameC(), getFieldCppDefault(field));
							f.output("%s->writeType(%s);", senderName, field.getNameC());
			if(!skipComp)	f.output("}");
		}
							f.recover();
							f.output("}");
	}
}

static void generateFieldSerializeJson(CodeFile& f, Field& field)
{
	f.output("// serialize %s", field.getNameC());
	f.output("ss << \"\\\"%s\\\":\";", field.getNameC());
	if (field.getArray())
	{
		// 
		f.output("{");
		f.indent();
		f.output("ss << \"[\";");
		f.output("uint32_t __len__ = (uint32_t)%s.size();", field.getNameC());
		f.output("for(uint32_t i = 0; i < __len__; i++)");
		f.output("{");
		f.indent();
		// .
		if (field.getType() == FT_USER)
		{
			f.output("%s[i].toJson(ss);", field.getNameC());
		}
		else if (field.getType() == FT_ENUM)
		{
			f.output("ss << \"\\\"\" << ENUM(%s).getItemName(%s[i]) << \"\\\"\";", field.getUserType()->getNameC(), field.getNameC());
		}
		else if (field.getType() == FT_STRING)
		{
			f.output("ss << \"\\\"\" << escapeJsonString(%s[i]) << \"\\\"\";", field.getNameC());
		}
		else if (field.getType() == FT_BOOL)
		{
			f.output("ss << (%s[i] ? \"true\" : \"false\");", field.getNameC());
		}
		else if (field.getType() == FT_FLOAT || field.getType() == FT_DOUBLE)
		{
			f.output("ss << std::setprecision(std::numeric_limits<double>::max_digits10) << (double)%s[i];", field.getNameC());
		}
		else
		{
			f.output("ss << (int64_t)%s[i];", field.getNameC());
		}
		
		f.output("ss <<(((i+1) == __len__) ? \"\":\",\")<<\"\";");
		f.recover();
		f.output("}");
		f.output("ss << \"]\";");
		f.recover();
		f.output("}");
	}
	else
	{
		f.output("{");
		f.indent();
		if (field.getType() == FT_USER)
		{
			f.output("%s.toJson(ss);", field.getNameC());
		}
		else if (field.getType() == FT_STRING)
		{
			f.output("ss << \"\\\"\" << escapeJsonString(%s) << \"\\\"\";", field.getNameC());
		}
		else if (field.getType() == FT_BOOL)
		{
			f.output("ss << (%s ? \"true\" : \"false\");", field.getNameC());
			
		}
		else if (field.getType() == FT_ENUM)
		{
			f.output("ss << \"\\\"\" << ENUM(%s).getItemName(%s) << \"\\\"\";",field.getUserType()->getNameC() ,field.getNameC());
		}
		else if (field.getType() == FT_FLOAT || field.getType() == FT_DOUBLE)
		{
			f.output("ss << std::setprecision(std::numeric_limits<double>::max_digits10) << (double)%s;", field.getNameC());
		}
		else 
		{
			f.output("ss << (int64_t)%s;", field.getNameC());
		}
		f.recover();
		f.output("}");
	}
}

static void generateFieldContainerSerialize(CodeFile& f, FieldContainer* fc, const char* senderName, bool skipComp = false)
{
	if(!fc->fields_.size())
		return;

	if(!skipComp)
	{
		// fields FieldMask .
		f.output("//field mask");
		f.output("FieldMask<%d> __fm__;", fc->getFMByteNum());
		for(size_t i = 0; i <fc-> fields_.size(); i++)
		{
			Field& field = fc->fields_[i];
			if(field.getArray())
				f.output("__fm__.writeBit(%s.size()?true:false);", field.getNameC());
			else
			{
				if(field.getType() == FT_USER)
					f.output("__fm__.writeBit(true);");
				else if(field.getType() == FT_STRING)
					f.output("__fm__.writeBit(%s.length()?true:false);", field.getNameC());
				else if(field.getType() == FT_BOOL)
					f.output("__fm__.writeBit(%s);", field.getNameC());
				else
					f.output("__fm__.writeBit((%s==%s)?false:true);", field.getNameC(), getFieldCppDefault(field));
			}
		}
		f.output("%s->write(__fm__.masks_, %d);", senderName, fc->getFMByteNum());
	}

	// field.
	for(size_t i = 0; i < fc->fields_.size(); i++)
		generateFieldSerialize(f, fc->fields_[i], senderName, skipComp);
}

static void generateFieldContainerSerializeJson(CodeFile& f, FieldContainer* fc)
{
	if (!fc->fields_.size())
		return;

	// field.
	for (size_t i = 0; i < fc->fields_.size(); i++)
	{
		generateFieldSerializeJson(f, fc->fields_[i]);
		f.output(((i + 1) == fc->fields_.size()) ? "ss<<\"\\n\";" : " ss << \",\\n\";");
	}
}

static void generateFieldLoadJson(CodeFile& f, Field& field)
{
	f.output("// loadJson %s", field.getNameC());
	f.output("if(__rd__.hasMember(\"%s\")){", field.getNameC());
	f.indent();

	if (field.getArray())
	{
		f.output("if(!__rd__.enterArray(\"%s\")) return false;", field.getNameC());
		f.output("size_t __len__ = __rd__.getArraySize();");
		f.output("%s.resize(__len__);", field.getNameC());
		f.output("for(size_t i = 0; i < __len__; i++)");
		f.output("{");
		f.indent();
		f.output("if(!__rd__.enterArrayItem(i)) return false;");

		if (field.getType() == FT_USER)
		{
			f.output("if(!%s[i].loadJson(__rd__)) return false;", field.getNameC());
		}
		else if (field.getType() == FT_ENUM)
		{
			f.output("int32_t __val__;");
			f.output("if(!__rd__.readValueEnum(__val__, &ENUM(%s))) return false;", field.getUserType()->getNameC());
			f.output("%s[i] = (%s)__val__;", field.getNameC(), getFieldCppType(field, false));
		}
		else if (field.getType() == FT_STRING)
		{
			f.output("if(!__rd__.readValueString(%s[i], %d)) return false;", field.getNameC(), field.getMaxStrLength());
		}
		else if (field.getType() == FT_BOOL)
		{
			f.output("bool __bv__;");
			f.output("if(!__rd__.readValueBool(__bv__)) return false;");
			f.output("%s[i] = __bv__;", field.getNameC());
		}
		else if (field.getType() == FT_FLOAT || field.getType() == FT_DOUBLE)
		{
			f.output("double __val__;");
			f.output("if(!__rd__.readValueDouble(__val__)) return false;");
			f.output("%s[i] = (%s)__val__;", field.getNameC(), getFieldCppType(field, false));
		}
		else
		{
			f.output("int64_t __val__;");
			f.output("if(!__rd__.readValueInt(__val__)) return false;");
			f.output("%s[i] = (%s)__val__;", field.getNameC(), getFieldCppType(field, false));
		}

		f.output("__rd__.leave();");
		f.recover();
		f.output("}");
		f.output("__rd__.leave();");
	}
	else
	{
		if (field.getType() == FT_USER)
		{
			f.output("if(!__rd__.enterObject(\"%s\")) return false;", field.getNameC());
			f.output("if(!%s.loadJson(__rd__)) return false;", field.getNameC());
			f.output("__rd__.leave();");
		}
		else if (field.getType() == FT_ENUM)
		{
			f.output("int32_t __val__;");
			f.output("if(!__rd__.readEnum(\"%s\", __val__, &ENUM(%s))) return false;", field.getNameC(), field.getUserType()->getNameC());
			f.output("%s = (%s)__val__;", field.getNameC(), getFieldCppType(field, false));
		}
		else if (field.getType() == FT_STRING)
		{
			f.output("if(!__rd__.readString(\"%s\", %s, %d)) return false;", field.getNameC(), field.getNameC(), field.getMaxStrLength());
		}
		else if (field.getType() == FT_BOOL)
		{
			f.output("if(!__rd__.readBool(\"%s\", %s)) return false;", field.getNameC(), field.getNameC());
		}
		else if (field.getType() == FT_FLOAT || field.getType() == FT_DOUBLE)
		{
			f.output("double __val__;");
			f.output("if(!__rd__.readDouble(\"%s\", __val__)) return false;", field.getNameC());
			f.output("%s = (%s)__val__;", field.getNameC(), getFieldCppType(field, false));
		}
		else
		{
			f.output("int64_t __val__;");
			f.output("if(!__rd__.readInt(\"%s\", __val__)) return false;", field.getNameC());
			f.output("%s = (%s)__val__;", field.getNameC(), getFieldCppType(field, false));
		}
	}

	f.recover();
	f.output("}");
}

static void generateFieldContainerLoadJson(CodeFile& f, FieldContainer* fc)
{
	if (!fc->fields_.size())
		return;

	for (size_t i = 0; i < fc->fields_.size(); i++)
	{
		generateFieldLoadJson(f, fc->fields_[i]);
	}
}

static void generateFieldDeserialize(CodeFile& f, Field& field, const char* recvName, bool skipComp)
{
	f.output("// deserialize %s", field.getNameC());
	if(field.getArray())
	{
		// 
		if(!skipComp)		f.output("if(__fm__.readBit())");
							f.output("{");
							f.indent();
							f.output("uint32_t __len__;");
							f.output("if(!%s->readDynSize(__len__) || __len__ > %d) return false;", recvName, field.getMaxArrLength());
							f.output("%s.resize(__len__);", field.getNameC());
							// .
							f.output("for(uint32_t i = 0; i < __len__; i++)");
							f.output("{");
							f.indent();
		if(field.getType() == FT_USER)
		{
							f.output("if(!%s[i].deserialize(%s)) return false;", field.getNameC(), recvName);
		}
		else if(field.getType() == FT_STRING)
		{
							f.output("if(!%s->readType(%s[i], %d)) return false;", recvName, field.getNameC(), field.getMaxStrLength());
		}
		else if(field.getType() == FT_ENUM)
		{
							f.output("uint8_t __e__;");
							f.output("if(!%s->readType(__e__) || __e__ >= %d) return false;", recvName, field.getUserType()->getEnum()->items_.size());
							f.output("%s[i] = (%s)__e__;", field.getNameC(), getFieldCppType(field, false));
		}
		else if(field.getType() == FT_BOOL)
		{
							f.output("uint8_t __bv__;");
							f.output("if(!%s->readType(__bv__)) return false;", recvName);
							f.output("%s[i] = __bv__?true:false;", field.getNameC());
		}
		else
		{
							f.output("if(!%s->readType(%s[i])) return false;", recvName, field.getNameC());
		}
							f.recover();
							f.output("}");
							f.recover();
							f.output("}");
	}
	else
	{
		f.output("{");
		f.indent();
		if(field.getType() == FT_USER)
		{
			if(!skipComp)	f.output("if(__fm__.readBit()){");
							f.output("if(!%s.deserialize(%s)) return false;", field.getNameC(), recvName);
			if(!skipComp)	f.output("}");
		}
		else if(field.getType() == FT_STRING)
		{
			if(!skipComp)	f.output("if(__fm__.readBit()){");
							f.output("if(!%s->readType(%s, %d)) return false;", recvName, field.getNameC(), field.getMaxStrLength());
			if(!skipComp)	f.output("}");
		}
		else if(field.getType() == FT_BOOL)
		{
			if(!skipComp)
			{
							f.output("%s = __fm__.readBit();", field.getNameC());
			}
			else
			{
							f.output("if(!%s->readType(%s)) return false;", recvName, field.getNameC());
			}
		}
		else if(field.getType() == FT_ENUM)
		{
							f.output("uint8_t __e__ = 0;");	//readBit0__e__0
			if(!skipComp)	f.output("if(__fm__.readBit()){");
							f.output("if(!%s->readType(__e__) || __e__ >= %d) return false;", recvName, field.getUserType()->getEnum()->items_.size()); 
							f.output("%s = (%s)__e__;", field.getNameC(), getFieldCppType(field, false));
			if(!skipComp)	f.output("}");
		}
		else
		{
			if(!skipComp)	f.output("if(__fm__.readBit()){");
							f.output("if(!%s->readType(%s)) return false;", recvName, field.getNameC());
			if(!skipComp)	f.output("}");
		}
		f.recover();
		f.output("}");
	}

}

static void generateFieldContainerDeserialize(CodeFile& f, FieldContainer* fc, const char* recvName, bool skipComp = false)
{
	if(!fc->fields_.size())
		return;

	if(!skipComp)
	{
		// field field mask.
		f.output("//field mask");
		f.output("FieldMask<%d> __fm__;", fc->getFMByteNum());
		f.output("if(!%s->read(__fm__.masks_, %d)) return false;", recvName, fc->getFMByteNum());
	}

	// field.
	for(size_t i = 0; i < fc->fields_.size(); i++)
		generateFieldDeserialize(f, fc->fields_[i], recvName, skipComp);
}

static void generateStructDef(CodeFile& f, Struct* s)
{
	/** struct.
		constructor.default valuefieldconstructor.
	*/
	bool needCtor = false;
	for(size_t i = 0; i < s->fields_.size(); i++)
	{
		Field& field = s->fields_[i];
		if(getFieldCppDefault(field))
		{
			needCtor = true;
			break;
		}
	}
	if(needCtor)
	{
		f.output("%s::%s():", s->getNameC(), s->getNameC());
		// defaults.
		bool needComma = false;
		for(size_t i = 0; i < s->fields_.size(); i++)
		{
			Field& field = s->fields_[i];
			if(!getFieldCppDefault(field))
				continue;
			f.output("%s%s(%s)", needComma?",":"", field.getNameC(), getFieldCppDefault(field));
			needComma = true;
		}
		f.output("{}");
	}

	/** .
		void serialize(ProtocolWriter* s) const;
	*/
	f.output("void %s::serialize(ProtocolWriter* __s__) const", s->getNameC());
	f.output("{");
	f.indent();
	if(s->super_)
		f.output("%s::serialize(__s__);", s->super_->getNameC());
	generateFieldContainerSerialize(f, s, "__s__", s->skipComp_);
	f.recover();
	f.output("}");

	/** .
		bool deserialize(ProtocolReader* r);
	*/
	f.output("bool %s::deserialize(ProtocolReader* __r__)", s->getNameC());
	f.output("{");
	f.indent();
	if(s->super_)
		f.output("if(!%s::deserialize(__r__)) return false;",s-> super_->getNameC());
	generateFieldContainerDeserialize(f, s, "__r__", s->skipComp_);
	f.output("	return true;");
	f.recover();
	f.output("}");

	f.output("void %s::toJson(std::ostream& ss, bool needBracket)const", s->getNameC());
	f.output("{");
	f.indent();
	f.output("if(needBracket){ ss << \"{\"; }");
	if (s->super_) {
		f.output("%s::toJson(ss,false);", s->super_->getNameC());
		// Add comma before first derived field if base has fields and derived has fields
		if (s->fields_.size() > 0) {
			f.output("if(%s::FIDMAX > 0) ss << \",\\n\";", s->super_->getNameC());
		}
	}
	generateFieldContainerSerializeJson(f, s);
	f.output("if(needBracket){ ss << \"}\"; }");
	f.recover();
	f.output("}");

	/** .
		bool loadJson(JsonReader& __rd__);
	*/
	f.output("bool %s::loadJson(JsonReader& __rd__)", s->getNameC());
	f.output("{");
	f.indent();
	if (s->super_)
		f.output("if(!%s::loadJson(__rd__)) return false;", s->super_->getNameC());
	generateFieldContainerLoadJson(f, s);
	f.output("return true;");
	f.recover();
	f.output("}");

	/** .
		bool loadJson(const char* json, size_t len);
	*/
	f.output("bool %s::loadJson(const char* json, size_t len)", s->getNameC());
	f.output("{");
	f.indent();
	f.output("JsonReader __rd__(json, len);");
	f.output("if(!__rd__.isValid()) return false;");
	f.output("return loadJson(__rd__);");
	f.recover();
	f.output("}");

	/** .
		bool loadJson(const std::string& json);
	*/
	f.output("bool %s::loadJson(const std::string& json)", s->getNameC());
	f.output("{");
	f.indent();
	f.output("return loadJson(json.c_str(), json.length());");
	f.recover();
	f.output("}");
}

static void generateStubMethodDef(CodeFile& f, Service*s, Method& m, size_t pid)
{
	f.begin();
	f.append("void %sStub::%s(", s->getNameC(), m.getNameC());
	for(size_t i = 0; i < m.fields_.size(); i++)
	{
		Field& field = m.fields_[i];
		f.append("%s%s%s %s%s", 
			useParamReference(field)?"const ":"",
			getFieldCppType(field),
			useParamReference(field)?"&":"",
			field.getNameC(),
			(i == m.fields_.size()-1)?"":",");
	}
	f.append(")");
	f.end();
	f.output("{");
	f.indent();
	f.output("ProtocolWriter* w = methodBegin();");
	f.output("if(!w) return;");
	f.output("uint16_t pid = %d;", pid);
	f.output("w->writeType(pid);");
	generateFieldContainerSerialize(f, &m, "w", true);
	f.output("methodEnd();");
	f.recover();
	f.output("}");
}

static void generateStubDef(CodeFile& f, Service* s)
{
	/** Methods */
	size_t methodStartId = s->super_?s->super_->getMethodNum():0;
	for(size_t i = 0; i < s->methods_.size(); i++)
		generateStubMethodDef(f, s, s->methods_[i], methodStartId+i);
}

static void generateProxyMethodDef(CodeFile&f, Method& m, const std::string& svcName)
{
	f.output("bool %sProxy::%s(ProtocolReader* __r__)", svcName.c_str(), m.getNameC());
	f.output("{");
	f.indent();
	for(size_t i = 0; i < m.fields_.size(); i++)
	{
		Field& field = m.fields_[i];
		if(getFieldCppDefault(field))
			f.output("%s %s=%s;", getFieldCppType(field), field.getNameC(), getFieldCppDefault(field));
		else
			f.output("%s %s;", getFieldCppType(field), field.getNameC());
	}
	generateFieldContainerDeserialize(f, &m, "__r__", true);
	f.begin();
	f.append("return %s(", m.getNameC());
	for(size_t i = 0; i < m.fields_.size(); i++)
	{
		Field& field = m.fields_[i];
		f.append("%s%s", field.getNameC(), (i == m.fields_.size()-1)?"":",");
	}
	f.append(");");
	f.end();
	f.recover();
	f.output("}");
}

static void generateProxyDef(CodeFile& f, Service* s)
{

	// methods.
	for(size_t i = 0; i < s->methods_.size(); i++)
		generateProxyMethodDef(f, s->methods_[i], s->getName());

	// Dispatch .
				f.output("bool %sProxy::dispatch(ProtocolReader* r)", s->getNameC());
				f.output("{");
				f.indent();
				f.output("uint16_t pid;");
				f.output("if(!r->readType(pid)) return false;");
				f.output("switch(pid)");
				f.output("{");
				f.indent();
	std::vector<Method*> methods;
	s->getAllMethods(methods);
	for(size_t i = 0; i < methods.size(); i++)
	{
		Method* method = methods[i];
			f.output("case %d:", i);
			f.output("{");
			f.indent();
			f.output("if(!%s(r)) return false;", method->getNameC());
			f.recover();
			f.output("}");
			f.output("break;");
	}
				f.output("default: return false;");
				f.recover();
				f.output("}");
				f.output("return true;");
				f.recover();
				f.output("}");
}

void CppGenerator::generate()
{
	// H File.
	{
		// .
		std::string fn = 
			Compiler::inst().outputDir_ + 
			Compiler::inst().fileStem_ + ".h";
		CodeFile f(fn);

		// .
		f.output("/* This file is generated by arpcc, do not change manually! */");
		f.output("#ifndef __%s_h__", Compiler::inst().fileStem_.c_str());
		f.output("#define __%s_h__", Compiler::inst().fileStem_.c_str());
		f.output("#include \"ProtocolWriter.h\"");
		f.output("#include \"ProtocolReader.h\"");
		f.output("#include \"EnumInfo.h\"");
		f.output("#include \"JsonHelper.h\"");
		f.output("#include <sstream>");
		f.output("#include <iomanip>");
		for(size_t i = 0; i < Compiler::inst().imports_.size(); i++)
		{
			std::string incFilename = Compiler::inst().imports_[i];
			incFilename = incFilename.substr(0,incFilename.find('.'));
			f.output("#include \"%s.h\"", incFilename.c_str());

		}

		// cppcode.
		if(Compiler::inst().cppcode_.length())
			f.output("%s", Compiler::inst().cppcode_.c_str());

		// .
		for(size_t i = 0; i < Compiler::inst().definitions_.size(); i++)
		{
			Definition* definition = Compiler::inst().definitions_[i];
			if(definition->getFile() != Compiler::inst().filename_)
				continue;
			f.output("//=============================================================");
			if (definition->getEnum())
				generateEnumDecl(f, definition->getEnum());
			else if (definition->getStruct())
				generateStructDecl(f, definition->getStruct());
			else if (definition->getService())
			{
				Service* s = definition->getService();
				generateStubDecl(f, s);
				generateProxyDecl(f, s);

				/* Methods file.
				methodsservice
				service
				methodsmethods
				
				*/
				std::string mfn = Compiler::inst().outputDir_ + s->getName() + "Methods.h";
				CodeFile mf(mfn);
				if(s->super_)
					mf.output("#include \"%sMethods.h\"", s->super_->getNameC());
				for(size_t i = 0; i < s->methods_.size(); i++)
					generateProxyMethodDecl(mf, s->methods_[i], false);
			}

		}

		f.output("#endif");
	}
	// Cpp File.
	{
		// .
		std::string fn = 
			Compiler::inst().outputDir_ + 
			Compiler::inst().fileStem_ + ".cpp";
		CodeFile f(fn);

		// .
		f.output("/* arpcc auto generated cpp file. */");
		f.output("#include \"FieldMask.h\"");
		f.output("#include \"%s.h\"", Compiler::inst().fileStem_.c_str());

		// .
		for(size_t i = 0; i < Compiler::inst().definitions_.size(); i++)
		{
			Definition* definition = Compiler::inst().definitions_[i];
			if(definition->getFile() != Compiler::inst().filename_)
				continue;
			f.output("//=============================================================");
			if (definition->getEnum())
				generateEnumDef(f, definition->getEnum());
			else if (definition->getStruct())
				generateStructDef(f, definition->getStruct());
			else if (definition->getService())
			{
				generateStubDef(f, definition->getService());
				generateProxyDef(f, definition->getService());
			}
		}
	}
}
