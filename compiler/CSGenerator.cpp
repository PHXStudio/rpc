#include "CSGenerator.h"
#include "Compiler.h"
#include "CodeFile.h"



static const char* getFieldTypeName(Field& f)
{
	switch(f.getType())
	{
	case FT_INT64:
		return "long";
	case FT_UINT64:
		return "ulong";
	case FT_DOUBLE:
		return "double";
	case FT_FLOAT:
		return "float";
	case FT_INT32:
		return "int";
	case FT_UINT32:
		return "uint";
	case FT_INT16:
		return "short";
	case FT_UINT16:
		return "ushort";
	case FT_INT8:
		return "sbyte";
	case FT_UINT8:
		return "byte";
	case FT_BOOL:
		return "bool";
	case FT_STRING:
		return "string";
	case FT_USER:
	case FT_ENUM:
		return f.getUserType()->getNameC();
	default:
		throw "Invalid field type.";
	}
}

static void generateEnum(CodeFile& f, Enum* e)
{
	f.output("public enum %s : %s", e->getNameC(),getFieldTypeName(e->getSuperType()));
	f.output("{");
	f.indent();
	for(size_t i = 0; i < e->items_.size(); i++)
		f.output("%s,", e->items_[i].c_str());
	f.recover();
	f.output("}");
}

/* useFieldMask selects the encoding context, not a schema property:
     true  - a struct/method container supplies __fm__, so a field equal to its
             default is omitted and a bool lives in its mask bit alone.
     false - the standalone per-field API (serializeField/deserializeField),
             which has no __fm__ in scope and must emit the value verbatim. */
static void generateSingleFieldSerializeCode(CodeFile& f, Field& field, const char* wn, bool useFieldMask)
{
	if(field.getType() == FT_USER)
		f.output("%s.serialize(%s);", field.getNameC(), wn);
	else if(field.getType() == FT_STRING)
	{
		if(useFieldMask)
		{
			f.output("if(%s.Length > 0)", field.getNameC());
			f.indent();
		}
		f.output("rpc.ProtocolWriter.writeType(%s, %s);", wn, field.getNameC());
		if(useFieldMask)
			f.recover();
	}
	else if(field.getType() == FT_BOOL)
	{
		/* boolfieldmask: under a container a bool has no payload byte -- its
		   value is the mask bit written by the container, so this branch stays
		   empty. Standalone it has no mask to live in and must be written.
		   Either way it must not fall through to the scalar branch below. */
		if(!useFieldMask)
			f.output("rpc.ProtocolWriter.writeType(%s, %s);", wn, field.getNameC());
	}
	else if(field.getType() == FT_ENUM)
	{
		f.output("byte __e__ = (byte)%s;", field.getNameC());
		if(useFieldMask)
		{
			f.output("if(__e__ != 0)");
			f.indent();
		}
		f.output("rpc.ProtocolWriter.writeType(%s, __e__);", wn);
		if(useFieldMask)
			f.recover();
	}
	else
	{
		if(useFieldMask)
		{
			f.output("if(%s != 0)", field.getNameC());
			f.indent();
		}
		f.output("rpc.ProtocolWriter.writeType(%s, %s);", wn, field.getNameC());
		if(useFieldMask)
			f.recover();
	}
}

static void generateArrayFieldSerializeCode(CodeFile& f, Field& field, const char* wn, bool useFieldMask)
{
	// Array size.
	if(useFieldMask)
	{
		f.output("if(%s != null && %s.Length > 0)", field.getNameC(), field.getNameC());
		f.output("{");
		f.indent();
	}
	f.output("uint __len__ = (%s == null)?0:(uint)%s.Length;", field.getNameC(), field.getNameC());
	f.output("rpc.ProtocolWriter.writeDynSize(%s, __len__);", wn);
	f.output("for(uint i = 0; i < __len__; i++)");
	f.output("{");
	f.indent();
	if(field.getType() == FT_USER)
		f.output("%s[i].serialize(%s);", field.getNameC(), wn);
	else if(field.getType() == FT_ENUM)
		f.output("rpc.ProtocolWriter.writeType(%s, (byte)%s[i]);", wn, field.getNameC());
	else
		f.output("rpc.ProtocolWriter.writeType(%s, %s[i]);", wn, field.getNameC());
	f.recover();
	f.output("}");
	if(useFieldMask)
	{
		f.recover();
		f.output("}");
	}
}

static void generateFieldSerializeCode(CodeFile& f, Field& field, const char* wn, bool useFieldMask = true)
{
	f.output("{");
	f.indent();
	if(field.getArray())
		generateArrayFieldSerializeCode(f, field, wn, useFieldMask);
	else
		generateSingleFieldSerializeCode(f, field, wn, useFieldMask);
	f.recover();
	f.output("}");
}

static void generateFieldContainerSerializeCode(CodeFile& f, FieldContainer* fc, const char* wn)
{
	if(!fc->fields_.size())
		return;

	// Always write field mask for version compatibility.
	{
		// Write field mask length prefix for version compatibility
		f.output("// Write field mask length");
		f.output("byte __fm_len__ = (byte)%d;", fc->getFMByteNum());
		f.output("rpc.ProtocolWriter.writeType(%s, __fm_len__);", wn);

		f.output("rpc.FieldMask __fm__ = new rpc.FieldMask(new byte[%d]);", fc->getFMByteNum());
		for(size_t i = 0; i < fc->fields_.size(); i++)
		{
			Field& field = fc->fields_[i];
			if(field.getArray())
				f.output("__fm__.writeBit((%s==null)?false:(%s.Length>0?true:false));",
				field.getNameC(), field.getNameC());
			else
			{
				if(field.getType() == FT_USER)
					f.output("__fm__.writeBit(true);");
				else if(field.getType() == FT_STRING)
					f.output("__fm__.writeBit(%s.Length>0?true:false);", field.getNameC());
				else if(field.getType() == FT_BOOL)
					f.output("__fm__.writeBit(%s);", field.getNameC());
				else
					f.output("__fm__.writeBit(%s==0?false:true);", field.getNameC());
			}
		}
		f.output("rpc.ProtocolWriter.writeType(%s, __fm__.getBits());", wn);
	}

	for(size_t i = 0; i < fc->fields_.size(); i++)
		generateFieldSerializeCode(f, fc->fields_[i], wn);
}

static void generateArrayFieldDeserializeCode(CodeFile& f, Field& field, const char* rn, bool useFieldMask)
{
	if(useFieldMask)
	{
		f.output("if(__fm__.readBit())");
		f.output("{");
		f.indent();
	}
	f.output("uint __len__;");
	f.output("if(!rpc.ProtocolReader.readDynSize(%s, out __len__) || __len__ > %d) return false;", rn, field.getMaxArrLength());
	f.output("%s = new %s[__len__];", field.getNameC(), getFieldTypeName(field));
	f.output("for(uint i = 0; i < __len__; i++)");
	f.output("{");
	f.indent();
	if(field.getType() == FT_USER)
	{
		f.output("%s[i] = new %s();", field.getNameC(), getFieldTypeName(field));
		f.output("if(!%s[i].deserialize(%s)) return false;", field.getNameC(), rn);
	}
	else if(field.getType() == FT_STRING)
	{
		f.output("if(!rpc.ProtocolReader.readType(%s, out %s[i], %d)) return false;", rn, field.getNameC(), field.getMaxStrLength());
	}
	else if(field.getType() == FT_ENUM)
	{
		f.output("byte __e__;");
		f.output("if(!rpc.ProtocolReader.readType(%s, out __e__) || __e__ >= %d) return false;", rn, field.getUserType()->getEnum()->items_.size());
		f.output("%s[i] = (%s)__e__;", field.getNameC(), getFieldTypeName(field));
	}
	else
	{	
		f.output("if(!rpc.ProtocolReader.readType(%s, out %s[i])) return false;", rn, field.getNameC());
	}
	f.recover();
	f.output("}");
	if(useFieldMask)
	{
		f.recover();
		f.output("}");
	}
}

static void generateSingleFieldDeserializeCode(CodeFile& f, Field& field, const char* rn, bool useFieldMask)
{
	if(field.getType() == FT_USER)
	{
		if(useFieldMask)
			f.output("__fm__.readBit();");
		f.output("if(!%s.deserialize(%s)) return false;", field.getNameC(), rn);
	}
	else if(field.getType() == FT_STRING)
	{
		if(useFieldMask)
		{
			f.output("if(__fm__.readBit())");
			f.output("{");
			f.indent();
		}
		f.output("if(!rpc.ProtocolReader.readType(%s, out %s, %d)) return false;", rn, field.getNameC(), field.getMaxStrLength());
		if(useFieldMask)
		{
			f.recover();
			f.output("}");
		}
	}
	else if(field.getType() == FT_BOOL)
	{
		/* boolfieldmask: under a container a bool has no payload byte -- the
		   mask bit IS the value, so it must be read directly rather than used
		   as a guard (guarding would skip the assignment whenever the value is
		   false). Standalone there is no mask, so the value is a real byte. */
		if(useFieldMask)
			f.output("%s = __fm__.readBit();", field.getNameC());
		else
			f.output("if(!rpc.ProtocolReader.readType(%s, out %s)) return false;", rn, field.getNameC());
	}
	else if(field.getType() == FT_ENUM)
	{
		if(useFieldMask)
		{
			f.output("if(__fm__.readBit())");
			f.output("{");
			f.indent();
		}
		f.output("byte __e__ = 0;");
		f.output("if(!rpc.ProtocolReader.readType(%s, out __e__) || __e__ >= %d) return false;", rn, field.getUserType()->getEnum()->items_.size());
		f.output("%s = (%s)__e__;", field.getNameC(), getFieldTypeName(field));
		if(useFieldMask)
		{
			f.recover();
			f.output("}");
		}
	}
	else
	{
		if(useFieldMask)
		{
			f.output("if(__fm__.readBit())");
			f.output("{");
			f.indent();
		}
		f.output("if(!rpc.ProtocolReader.readType(%s, out %s)) return false;", rn, field.getNameC());
		if(useFieldMask)
		{
			f.recover();
			f.output("}");
		}
	}
}

static void generateFieldDeserializeCode(CodeFile& f, Field& field, const char* rn, bool useFieldMask = true)
{
	f.output("{");
	f.indent();
	if(field.getArray())
		generateArrayFieldDeserializeCode(f, field, rn, useFieldMask);
	else
		generateSingleFieldDeserializeCode(f, field, rn, useFieldMask);
	f.recover();
	f.output("}");
}

static void generateFieldContainerDeserializeCode(CodeFile& f, FieldContainer* fc, const char* rn)
{
	if(!fc->fields_.size())
		return;

	// Always read field mask for version compatibility.
	{
		// Read field mask length prefix for version compatibility
		f.output("// Read field mask length");
		f.output("byte __actual_fm_len__;");
		f.output("if(!rpc.ProtocolReader.readType(%s, out __actual_fm_len__)) return false;", rn);
		f.output("byte __read_fm_len__ = (byte)Math.Min((int)__actual_fm_len__, %d);", fc->getFMByteNum());

		f.output("byte[] __fmbits__;");
		f.output("if(!rpc.ProtocolReader.readType(%s, out __fmbits__, __read_fm_len__)) return false;", rn);

		// Skip remaining field mask bytes
		f.output("// Skip remaining field mask bytes");
		f.output("if(__actual_fm_len__ > __read_fm_len__){");
		f.indent();
		f.output("if(!rpc.ProtocolReader.Skip(%s, (uint)(__actual_fm_len__ - __read_fm_len__))) return false;", rn);
		f.recover();
		f.output("}");

		f.output("rpc.FieldMask __fm__ = new rpc.FieldMask(__fmbits__);");
	}

	for(size_t i = 0; i < fc->fields_.size(); i++)
		generateFieldDeserializeCode(f, fc->fields_[i], rn);
}

static void generateStruct(CodeFile& f, Struct* s)
{
	if(s->super_)
		f.output("public class %s : %s", s->getNameC(), s->super_->getNameC());
	else
		f.output("public class %s", s->getNameC());
	f.output("{");
	f.indent();
	// fields.
	for(size_t i = 0; i < s->fields_.size(); i++)
	{
		Field& field = s->fields_[i];
		if(field.getArray())
			f.output("public %s[] %s = new %s[0];", 
			getFieldTypeName(field), 
			field.getNameC(),
			getFieldTypeName(field));
		else
		{
			if(field.getType() == FT_USER)
				f.output("public %s %s = new %s();", 
				getFieldTypeName(field), 
				field.getNameC(), 
				getFieldTypeName(field));
			else if(field.getType() == FT_STRING)
				f.output("public string %s = \"\";", field.getNameC());
			else
				f.output("public %s %s;", getFieldTypeName(field), field.getNameC());
		}
	}
	/** Field ids. */
	f.output("// member ids.");
	f.output("public enum FID");
	f.output("{");
	f.indent();
	size_t fid = s->super_?s->super_->getFieldNum():0;
	for(size_t i = 0; i < s->fields_.size(); i++)
	{
		Field& field = s->fields_[i];
		f.output("%s = %d,", field.getNameC(), fid);
		fid++;
	}
	f.output("MAX = %d,", fid);
	f.recover();
	f.output("}");

	// serialize code.
	f.output("public %s void serialize(rpc.IWriter w)", s->super_?"new":"");
	f.output("{");
	f.indent();
	if(s->super_)
		f.output("base.serialize(w);");
	generateFieldContainerSerializeCode(f, s, "w");
	f.recover();
	f.output("}");
	// deserialize code.
	f.output("public %s bool deserialize(rpc.IReader r)", s->super_?"new":"");
	f.output("{");
	f.indent();
	if(s->super_)
		f.output("base.deserialize(r);");
	generateFieldContainerDeserializeCode(f, s, "r");
	f.output("return true;");
	f.recover();
	f.output("}");
	// field serialize.
	f.output("public %s bool serializeField(uint fid, rpc.IWriter w)", s->super_?"new":"");
	f.output("{");
	f.indent();
	f.output("switch(fid)");
	f.output("{");
	f.indent();
	for(size_t i = 0; i < s->fields_.size(); i++)
	{
		Field& field = s->fields_[i];
		f.output("case (uint)FID.%s:", field.getNameC());
		f.output("{");
		f.indent();
		generateFieldSerializeCode(f, field, "w", false);
		f.recover();
		f.output("}");
		f.output("return true;");
	}
	f.recover();
	f.output("}");
	if(s->super_)
		f.output("return base.serializeField(fid, w);");
	else
		f.output("return false;");
	f.recover();
	f.output("}");
	// field deserialize.
	f.output("public %s bool deserializeField(uint fid, rpc.IReader r)", s->super_?"new":"");
	f.output("{");
	f.indent();
	f.output("switch(fid)");
	f.output("{");
	f.indent();
	for(size_t i = 0; i < s->fields_.size(); i++)
	{
		Field& field = s->fields_[i];
		f.output("case (uint)FID.%s:", field.getNameC());
		f.output("{");
		f.indent();
		generateFieldDeserializeCode(f, field, "r", false);
		f.recover();
		f.output("}");
		f.output("return true;");
	}
	f.recover();
	f.output("}");
	if(s->super_)
		f.output("return base.deserializeField(fid, r);");
	else
		f.output("return false;");
	f.recover();
	f.output("}");

	f.recover();
	f.output("}");
}

static void generateStubMethod(CodeFile& f, Service* s, Method& m, size_t mid)
{
	f.begin();
	f.append("public void %s(", m.getNameC());
	for(size_t i = 0; i < m.fields_.size(); i++)
	{
		Field& field = m.fields_[i];
		f.append("%s%s %s%s", 
			getFieldTypeName(field), 
			field.getArray()?"[]":"",
			field.getNameC(),
			(i == m.fields_.size()-1)?"":",");
	}
	f.append(")");
	f.end();
	f.output("{");
	f.indent();

	f.output("rpc.IWriter w = methodBegin();");
	f.output("if(w == null) return;");
	f.output("ushort __pid__ = %d;", mid);
	f.output("rpc.ProtocolWriter.writeType(w, __pid__);");
	generateFieldContainerSerializeCode(f, &m, "w");
	f.output("methodEnd();");

	f.recover();
	f.output("}");
}

static void generateServiceStub(CodeFile& f, Service* s)
{
	if(s->super_)
		f.output("public abstract class %sStub : %sStub", s->getNameC(), s->super_->getNameC());
	else
		f.output("public abstract class %sStub", s->getNameC());
	f.output("{");
	f.indent();
	if(!s->super_)
	{
		f.output("protected abstract rpc.IWriter methodBegin();");
		f.output("protected abstract void methodEnd();");
	}
	// methods.
	size_t methodStartId = s->super_?s->super_->getMethodNum():0;
	for(size_t i = 0; i < s->methods_.size(); i++)
		generateStubMethod(f, s, s->methods_[i], methodStartId + i);
	f.recover();
	f.output("}");
}

static void generateProxyAbstractMethod(CodeFile& f, Method& m)
{
	f.begin();
	f.append("bool %s(", m.getNameC());
	for(size_t i = 0; i < m.fields_.size(); i++)
	{
		Field& field = m.fields_[i];
		f.append("%s%s %s%s",
			getFieldTypeName(field),
			field.getArray()?"[]":"",
			field.getNameC(),
			(i == m.fields_.size()-1)?"":",");
	}
	f.append(");");
	f.end();
}

static void generateMethodDispatcher(CodeFile& f, Service* s, Method& m)
{
	f.output("public static bool %s(rpc.IReader __r__, %sProxy __p__)", m.getNameC(), s->getNameC());
	f.output("{");
	f.indent();
	for(size_t i = 0; i < m.fields_.size(); i++)
	{
		Field& field = m.fields_[i];
		if(field.getType() == FT_USER && !field.getArray())
			f.output("%s %s = new %s();", getFieldTypeName(field), field.getNameC(), getFieldTypeName(field));
		else
			f.output("%s%s %s;", getFieldTypeName(field), field.getArray()?"[]":"", field.getNameC());
	}
	generateFieldContainerDeserializeCode(f, &m, "__r__");
	f.begin();
	f.append("return __p__.%s(", m.getNameC());
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

static void generateServiceProxy(CodeFile& f, Service* s)
{
	if(s->super_)
		f.output("public interface %sProxy : %sProxy", s->getNameC(), s->super_->getNameC());
	else
		f.output("public interface %sProxy", s->getNameC());
	f.output("{");
	f.indent();

	// methods.
	for(size_t i = 0; i < s->methods_.size(); i++)
		generateProxyAbstractMethod(f, s->methods_[i]);
	f.recover();
	f.output("}");
}

static void generateServiceDispatcher(CodeFile& f, Service* s)
{
	f.output("public static class %sDispatcher", s->getNameC());
	f.output("{");
	f.indent();

	// deserializations.
	for(size_t i = 0; i < s->methods_.size(); i++)
		generateMethodDispatcher(f, s, s->methods_[i]);

	// dispatch function.
	f.output("public static bool dispatch(rpc.IReader r, %sProxy p)", s->getNameC());
	f.output("{");
	f.indent();
	f.output("ushort pid;");
	f.output("if(!rpc.ProtocolReader.readType(r, out pid)) return false;");
	f.output("switch(pid)");
	f.output("{");
	f.indent();
	std::vector<Service*> parents;
	s->getParents(parents);
	size_t mid = 0;
	for(size_t i = 0; i < parents.size(); i++)
	{
		Service* parent = parents[i];
		for(size_t m = 0; m < parent->methods_.size(); m++)
		{
			Method& method = parent->methods_[m];
			f.output("case %d:", mid++);
			f.output("{");
			f.indent();
			f.output("if(!%sDispatcher.%s(r, p)) return false;", parent->getNameC(), method.getNameC());
			f.recover();
			f.output("}");
			f.output("break;");
		}
	}
	f.output("default: return false;");
	f.recover();
	f.output("}");
	f.output("return true;");
	f.recover();
	f.output("}");
	f.recover();
	f.output("}");
}

static void generateService(CodeFile& f, Service* s)
{
	generateServiceStub(f, s);
	f.output("//=============================================================");
	generateServiceProxy(f, s);
	f.output("//=============================================================");
	generateServiceDispatcher(f, s);
}

void CSGenerator::generate()
{
	std::string fn = 
		Compiler::inst().outputDir_ + 
		Compiler::inst().fileStem_ + ".cs";
	CodeFile f(fn);

	f.output("/* This file is generated by rpc, do not change manually! */");
	f.output("");
	f.output("using System;");
	f.output("");

	// iterate all definations.
	for(size_t i = 0; i < Compiler::inst().definitions_.size(); i++)
	{
		Definition* definition = Compiler::inst().definitions_[i];
		if(definition->getFile() != Compiler::inst().filename_)
			continue;
		f.output("//=============================================================");
		if (definition->getEnum())
			generateEnum(f, definition->getEnum());
		else if (definition->getStruct())
			generateStruct(f, definition->getStruct());
		else if (definition->getService())
			generateService(f, definition->getService());
	}
}
