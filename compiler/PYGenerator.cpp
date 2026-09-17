#include "PYGenerator.h"
#include "Compiler.h"
#include "CodeFile.h"

static void generateEnum(CodeFile& f, Enum* e)
{
	f.output("# %s", e->getNameC());
	f.output("class %s(object):", e->getNameC());
	f.indent();
	for(size_t i = 0; i < e->items_.size(); i++)
		f.output("%s = %d", e->items_[i].c_str(), i);
	f.recover();
}
static const char* getFieldDefault(Field& f)
{
	if(f.getArray())
		return "[]";
	if(f.getType() == FT_BOOL)
		return "False";
	else if(f.getType() == FT_STRING)
		return "\"\"";
	else if(f.getType() == FT_USER)
	{
		static std::string un;
		un = f.getUserType()->getName() + "()";
		return un.c_str();
	}
	else
		return "0";
}
static const char* getFieldTypeName(Field& f)
{
	switch(f.getType())
	{
	case FT_INT64:	return "int64";
	case FT_UINT64: return "uint64";
	case FT_DOUBLE: return "double";
	case FT_FLOAT:	return "float";
	case FT_INT32:	return "int32";
	case FT_UINT32: return "uint32";
	case FT_INT16:	return "int16";
	case FT_UINT16: return "uint16";
	case FT_INT8:	return "int8";
	case FT_UINT8:	return "uint8";
	case FT_BOOL:	return "bool";
	case FT_STRING:	return "string";
	case FT_USER:	return f.getUserType()->getNameC();
	case FT_ENUM:	return "enum";
	}
	return "";
}

static size_t getFieldValMax(Field& f)
{
	if(f.getType() == FT_STRING)
		return f.getMaxStrLength();
	else if(f.getType() == FT_ENUM)
		return f.getUserType()->getEnum()->items_.size();
	return 0;
}

static void generateStruct(CodeFile& f, Struct* s)
{
	f.output("# %s", s->getNameC());
	f.output("class %s(%s):", s->getNameC(), s->super_?s->super_->getNameC():"object");
	f.indent();
	// __init__
	f.output("def __init__(self):");
	f.indent();
	if(s->super_)
		f.output("%s.__init__(self)", s->super_->getNameC());
	for(size_t i = 0; i < s->fields_.size(); i++)
	{
		Field& field = s->fields_[i];
		f.output("self.%s = %s", field.getNameC(), getFieldDefault(field));
	}
	// Python rejects an empty body, so a struct with neither a superclass nor
	// fields still needs an explicit pass statement.
	if(!s->super_ && s->fields_.size() == 0)
		f.output("pass");
	f.recover();
	// serialize
	f.output("def serialize(self, _b_):");
	f.indent();
	if(s->super_)
		f.output("%s.serialize(self, _b_)", s->super_->getNameC());

	/* A struct with no fields carries no field mask at all: the mask segment
	   only exists to describe the fields that follow it. This matches the C++,
	   C# and Go backends, which all skip the segment when the container is
	   empty. */
	if(s->fields_.size())
	{
		// Write field mask length prefix for version compatibility
		f.output("# Write field mask length");
		f.output("_b_.append(struct.pack('B', %d))", s->getFMByteNum());

		f.output("_fm_ = FieldMaskWriter(%d)", s->getFMByteNum());
		f.output("_pfm_ = len(_b_)");
		f.output("_b_.append(b'')");
	}
	else if(!s->super_)
	{
		f.output("pass");
	}

	for(size_t i = 0; i < s->fields_.size(); i++)
	{
		Field& field = s->fields_[i];
		f.output("write(%sWriter, %s, _b_, self.%s, %s)",
			getFieldTypeName(field),
			field.getArray()?"True":"False",
			field.getNameC(),
			s->fields_.size()?"_fm_":"None"
			);
	}
	if(s->fields_.size())
		f.output("_b_[_pfm_] = _fm_.write()");
	f.recover();
	// deserialize
	f.output("def deserialize(self, _b_, _p_):");
	f.indent();
	if(s->super_)
		f.output("_p_ = %s.deserialize(self, _b_, _p_)", s->super_->getNameC());

	// Mirror of the write path: no fields means no mask segment to read.
	if(s->fields_.size())
	{
		// Read field mask length prefix for version compatibility
		f.output("# Read field mask length");
		f.output("_actual_fm_len_ = struct.unpack('B', _b_[_p_:_p_+1])[0]");
		f.output("_p_ += 1");
		f.output("_read_fm_len_ = min(_actual_fm_len_, %d)", s->getFMByteNum());

		f.output("_fm_ = FieldMaskReader(_b_, _p_, _read_fm_len_)");
		f.output("_p_ += _read_fm_len_");

		// Skip remaining field mask bytes
		f.output("# Skip remaining field mask bytes");
		f.output("if _actual_fm_len_ > _read_fm_len_:");
		f.indent();
		f.output("_p_ = skipReader(_b_, _p_, _actual_fm_len_ - _read_fm_len_)");
		f.recover();
	}

	for(size_t i = 0; i < s->fields_.size(); i++)
	{
		Field& field = s->fields_[i];
		f.output("self.%s, _p_= read(%sReader, _b_, _p_, %d, %d, %s)",
			field.getNameC(),
			getFieldTypeName(field),
			field.getArray()?field.getMaxArrLength():0,
			getFieldValMax(field),
			s->fields_.size()?"_fm_":"None"
			);
	}
	f.output("return _p_");
	f.recover();
	f.recover();
	/* Writer & Reader.
	   A nested struct occupies exactly one bit of its parent's field mask, and
	   that bit is always set -- the value is never "absent", the sub-struct is
	   simply serialized in place. Leaving the bit untouched used to shift every
	   following field down by one position.
	   fm is None for array elements: their presence is already carried by the
	   array's own bit, so nothing must be consumed there. */
	f.output("def %sWriter(b, v, fm):", s->getNameC());
	f.indent();
	f.output("if fm != None:");
	f.indent();
	f.output("fm.set(True)");
	f.recover();
	f.output("v.serialize(b)");
	f.recover();
	f.output("def %sReader(b, p, valMax, fm):", s->getNameC());
	f.indent();
	f.output("if fm != None and not fm.get():");
	f.indent();
	f.output("return %s(), p", s->getNameC());
	f.recover();
	f.output("v = %s()", s->getNameC());
	f.output("p = v.deserialize(b, p)");
	f.output("return v, p");
	f.recover();
}

static void generateServiceStubMethod(CodeFile& f, size_t id, Method& m)
{
	f.begin();
	f.append("def %s(self", m.getNameC());
	for(size_t i = 0; i < m.fields_.size(); i++)
	{
		Field& field = m.fields_[i];
		f.append(",%s", field.getNameC());
	}
	f.append("):");
	f.end();
	f.indent();
	f.output("_b_ = []", id);
	f.output("uint16Writer(_b_, %d, None)", id);

	/* The parameter list is encoded exactly like a struct body: a method with no
	   parameters carries no field mask at all, matching C++ and C#. */
	if(m.fields_.size())
	{
		f.output("# Write field mask length");
		f.output("_b_.append(struct.pack('B', %d))", m.getFMByteNum());
		f.output("_fm_ = FieldMaskWriter(%d)", m.getFMByteNum());
		f.output("_pfm_ = len(_b_)");
		f.output("_b_.append(b'')");
	}

	for(size_t i = 0; i < m.fields_.size(); i++)
	{
		Field& field = m.fields_[i];
		f.output("write(%sWriter, %s, _b_, %s, %s)",
			getFieldTypeName(field),
			field.getArray()?"True":"False",
			field.getNameC(),
			m.fields_.size()?"_fm_":"None"
			);
	}
	if(m.fields_.size())
		f.output("_b_[_pfm_] = _fm_.write()");
	f.output("self.call(_b_)");
	f.recover();
}

static void generateServiceStub(CodeFile& f, Service* s)
{
	f.output("# %s stub", s->getNameC());
	if(s->super_)
		f.output("class %sStub(%sStub):", s->getNameC(), s->super_->getNameC());
	else
		f.output("class %sStub(object):", s->getNameC());
	f.indent();
	size_t mid = s->super_?s->super_->getMethodNum():0;
	for(size_t i = 0; i < s->methods_.size(); i++, mid++)
		generateServiceStubMethod(f, mid, s->methods_[i]);
	f.recover();
}

static void generateServiceProxy(CodeFile& f, Service* s)
{
	f.output("# %s proxy", s->getNameC());
	if(s->super_)
		f.output("class %sProxy(%sProxy):", s->getNameC(), s->super_->getNameC());
	else
		f.output("class %sProxy(object):", s->getNameC());
	f.indent();
	f.output("def dispatch(self, _b_):");
	f.indent();
	f.output("_id_, _p_ = uint16Reader(_b_, 0, 0, None)");
	f.output("self.dispatchID(_id_, _b_, _p_)");
	f.recover();
	f.output("def dispatchID(self, _id_, _b_, _p_):");
	f.indent();
	size_t mid = s->super_?s->super_->getMethodNum():0;
	for(size_t m = 0; m < s->methods_.size(); m++, mid++)
	{
		Method& method = s->methods_[m];
		f.output("if _id_ == %d:", mid);
		f.indent();

		/* Mirrors the stub: a parameterless method carries no field mask. */
		if(method.fields_.size())
		{
			f.output("# Read field mask length");
			f.output("_actual_fm_len_ = struct.unpack('B', _b_[_p_:_p_+1])[0]");
			f.output("_p_ += 1");
			f.output("_read_fm_len_ = min(_actual_fm_len_, %d)", method.getFMByteNum());
			f.output("_fm_ = FieldMaskReader(_b_, _p_, _read_fm_len_)");
			f.output("_p_ += _read_fm_len_");
			f.output("# Skip remaining field mask bytes");
			f.output("if _actual_fm_len_ > _read_fm_len_:");
			f.indent();
			f.output("_p_ = skipReader(_b_, _p_, _actual_fm_len_ - _read_fm_len_)");
			f.recover();
		}

		for(size_t fid = 0; fid < method.fields_.size(); fid++)
		{
			Field& field = method.fields_[fid];
			f.output("%s, _p_= read(%sReader, _b_, _p_, %d, %d, %s)",
				field.getNameC(),
				getFieldTypeName(field),
				field.getArray()?field.getMaxArrLength():0,
				getFieldValMax(field),
				method.fields_.size()?"_fm_":"None"
				);
		}
		f.begin();
		f.append("self.%s(", method.getNameC());
		bool needComma = false;
		for(size_t fid = 0; fid < method.fields_.size(); fid++)
		{
			Field& field = method.fields_[fid];
			f.append("%s%s", needComma?",":"", field.getNameC());
			needComma = true;
		}
		f.append(")");
		f.end();
		f.output("return");
		f.recover();
	}
	if(s->super_)
		f.output("%sProxy.dispatchID(self, _id_, _b_, _p_)", s->super_->getNameC());
	f.recover();
	f.recover();
}

static void generateService(CodeFile& f, Service* s)
{
	generateServiceStub(f, s);
	generateServiceProxy(f, s);
}

void PYGenerator::generate()
{
	// Python File.
	std::string fn = Compiler::inst().outputDir_ + Compiler::inst().fileStem_ + ".py";
	CodeFile f(fn);

	// import.
	f.output("from rpc.writer import *");
	f.output("from rpc.reader import *");
	/* No "from <imported> import *" line: an imported schema does not get a
	   module of its own. Its definitions are flattened into this file by the
	   loop further down, so importing a separate module would raise
	   ModuleNotFoundError at import time. */

	// .
	for(size_t i = 0; i < Compiler::inst().definitions_.size(); i++)
	{
		Definition* definition = Compiler::inst().definitions_[i];
		/* #imported definitions are flattened into this output as well.
		   Skipping them (as this used to) left every type they define
		   undefined while still being referenced by the root file. */
		if (definition->getEnum())
			generateEnum(f, definition->getEnum());
		else if (definition->getStruct())
			generateStruct(f, definition->getStruct());
		else if (definition->getService())
			generateService(f, definition->getService());
	}
}