#include "GoGenerator.h"
#include "Compiler.h"
#include "CodeFile.h"
#include <limits>
#include <algorithm>
#include <cctype>

// ============================================================================
// Type Mapping Helpers
// ============================================================================

static const char* getFieldGoType(Field& f, bool withArray = true)
{
    static std::string name;
    if(f.getArray() && withArray)
        name = "[]";
    else
        name = "";

    switch(f.getType())
    {
    case FT_INT64:   name += "int64"; break;
    case FT_UINT64:  name += "uint64"; break;
    case FT_DOUBLE:  name += "float64"; break;
    case FT_FLOAT:   name += "float32"; break;
    case FT_INT32:   name += "int32"; break;
    case FT_UINT32:  name += "uint32"; break;
    case FT_INT16:   name += "int16"; break;
    case FT_UINT16:  name += "uint16"; break;
    case FT_INT8:    name += "int8"; break;
    case FT_UINT8:   name += "uint8"; break;
    case FT_BOOL:    name += "bool"; break;
    case FT_STRING:  name += "string"; break;
    case FT_USER:
    case FT_ENUM:    name += f.getUserType()->getName(); break;
    default:
        throw "Invalid field type.";
    }
    return name.c_str();
}

static const char* getFieldGoDefault(Field& f)
{
    if(f.getArray())
        return "nil";

    switch(f.getType())
    {
    case FT_INT64:
    case FT_UINT64:
    case FT_INT32:
    case FT_UINT32:
    case FT_INT16:
    case FT_UINT16:
    case FT_INT8:
    case FT_UINT8:
        return "0";
    case FT_DOUBLE:
    case FT_FLOAT:
        return "0.0";
    case FT_BOOL:
        return "false";
    case FT_STRING:
        return "\"\"";
    case FT_ENUM:
        {
            Enum* e = f.getUserType()->getEnum();
            if(e && !e->items_.empty())
                return e->items_[0].c_str();
        }
        return "0";
    case FT_USER:
        {
            static std::string init;
            init = f.getUserType()->getName() + "{}";
            return init.c_str();
        }
    default:
        return "";
    }
}

// Convert snake_case to PascalCase for Go field names
static std::string toGoFieldName(const std::string& idlName)
{
    std::string goName;
    bool capitalize = true;

    for(size_t i = 0; i < idlName.length(); i++)
    {
        char c = idlName[i];
        if(c == '_')
        {
            capitalize = true;
        }
        else if(capitalize)
        {
            goName += toupper(c);
            capitalize = false;
        }
        else
        {
            goName += c;
        }
    }

    return goName;
}

// Convert package name (filename to lowercase)
static std::string toPackageName(const std::string& filename)
{
    std::string pkg = filename;
    // Remove directory path
    size_t lastSlash = pkg.find_last_of("/\\");
    if(lastSlash != std::string::npos)
        pkg = pkg.substr(lastSlash + 1);
    // Remove extension
    size_t lastDot = pkg.find_last_of('.');
    if(lastDot != std::string::npos)
        pkg = pkg.substr(0, lastDot);
    // Convert to lowercase
    std::transform(pkg.begin(), pkg.end(), pkg.begin(), ::tolower);
    return pkg;
}

static bool isGoBuiltInType(const std::string& type)
{
    return type == "int8" || type == "uint8" ||
           type == "int16" || type == "uint16" ||
           type == "int32" || type == "uint32" ||
           type == "int64" || type == "uint64" ||
           type == "float32" || type == "float64" ||
           type == "bool" || type == "string";
}

// Get max length for strings/arrays
static uint32_t getFieldMaxLength(Field& f)
{
    if(f.getType() == FT_STRING)
        return f.getMaxStrLength();
    if(f.getArray())
        return f.getMaxArrLength();
    return 0xFFFFFFFF;
}

// ============================================================================
// Enum Generation
// ============================================================================

static void generateEnumDecl(CodeFile& f, Enum* e)
{
    f.output("// enum %s", e->getNameC());
    f.output("type %s int32", e->getNameC());
    f.output("");
    f.output("const (");
    f.indent();
    f.output("%s %s = iota", e->items_[0].c_str(), e->getNameC());
    for(size_t i = 1; i < e->items_.size(); i++)
        f.output("%s", e->items_[i].c_str());
    f.recover();
    f.output(")");
    f.output("");
    f.output("// String returns the string representation of the enum");
    f.output("func (e %s) String() string {", e->getNameC());
    f.indent();
    f.output("switch e {");
    f.indent();
    for(size_t i = 0; i < e->items_.size(); i++)
        f.output("case %s: return \"%s\"", e->items_[i].c_str(), e->items_[i].c_str());
    f.output("default: return \"Unknown\"");
    f.recover();
    f.output("}");
    f.recover();
    f.output("}");
    f.output("");
    f.output("// MarshalJSON implements json.Marshaler interface");
    f.output("func (e %s) MarshalJSON() ([]byte, error) {", e->getNameC());
    f.indent();
    f.output("return json.Marshal(e.String())");
    f.recover();
    f.output("}");
    f.output("");
    f.output("// UnmarshalJSON implements json.Unmarshaler interface");
    f.output("func (e *%s) UnmarshalJSON(data []byte) error {", e->getNameC());
    f.indent();
    f.output("var s string");
    f.output("if err := json.Unmarshal(data, &s); err != nil {");
    f.indent();
    f.output("return err");
    f.recover();
    f.output("}");
    f.output("switch s {");
    f.indent();
    for(size_t i = 0; i < e->items_.size(); i++)
        f.output("case \"%s\": *e = %s", e->items_[i].c_str(), e->items_[i].c_str());
    f.output("default: return errors.New(\"invalid enum value\")");
    f.recover();
    f.output("}");
    f.output("return nil");
    f.recover();
    f.output("}");
}

static void generateEnumInfo(CodeFile& f, Enum* e)
{
    f.output("");
    f.output("// Enum info for %s", e->getNameC());
    f.output("var %sInfo = NewEnumInfo(\"%s\", []string{", e->getNameC(), e->getNameC());
    f.indent();
    for(size_t i = 0; i < e->items_.size(); i++)
    {
        if(i > 0) f.output(", ");
        f.output("\"%s\"", e->items_[i].c_str());
    }
    f.recover();
    f.output("})");
    f.output("");
    f.output("func init() {");
    f.indent();
    f.output("RegisterEnum(\"%s\", %sInfo)", e->getNameC(), e->getNameC());
    f.recover();
    f.output("}");
}

// ============================================================================
// Field Serialization/Deserialization
// ============================================================================

static void generateFieldSerialize(CodeFile& f, Field& field, const std::string& recvName, bool isMethod)
{
    std::string goName = toGoFieldName(field.getNameC());

    if(field.getArray())
    {
        // Array serialization
        if(!isMethod) {
            f.output("// serialize %s", field.getNameC());
            f.output("if len(s.%s) > 0 {", goName.c_str());
            f.indent();
        }
        f.output("if err := %s.WriteDynSize(uint32(len(s.%s))); err != nil {", recvName.c_str(), goName.c_str());
        f.indent();
        f.output("return err");
        f.recover();
        f.output("}");
        f.output("for _, v := range s.%s {", goName.c_str());
        f.indent();

        if(field.getType() == FT_USER)
        {
            f.output("if err := v.Serialize(%s); err != nil {", recvName.c_str());
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
        }
        else if(field.getType() == FT_ENUM)
        {
            f.output("if err := %s.WriteUint8(uint8(v)); err != nil {", recvName.c_str());
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
        }
        else if(field.getType() == FT_STRING)
        {
            f.output("if err := %s.WriteString(v); err != nil {", recvName.c_str());
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
        }
        else if(field.getType() == FT_BOOL)
        {
            f.output("if err := %s.WriteBool(v); err != nil {", recvName.c_str());
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
        }
        else if(field.getType() == FT_FLOAT || field.getType() == FT_DOUBLE)
        {
            std::string method = field.getType() == FT_FLOAT ? "WriteFloat32" : "WriteFloat64";
            f.output("if err := %s.%s(v); err != nil {", recvName.c_str(), method.c_str());
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
        }
        else
        {
            // Integer types
            f.output("if err := %s.WriteInt64(int64(v)); err != nil {", recvName.c_str());
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
        }

        f.recover();
        f.output("}");

        if(!isMethod) {
            f.recover();
            f.output("}");
        }
    }
    else
    {
        // Single field serialization
        if(!isMethod) {
            f.output("// serialize %s", field.getNameC());
        }

        if(field.getType() == FT_USER)
        {
            if(!isMethod) {
                f.output("if err := s.%s.Serialize(%s); err != nil {", goName.c_str(), recvName.c_str());
                f.indent();
                f.output("return err");
                f.recover();
                f.output("}");
            } else {
                f.output("if err := %s.Serialize(%s); err != nil {", goName.c_str(), recvName.c_str());
                f.indent();
                f.output("return err");
                f.recover();
                f.output("}");
            }
        }
        else if(field.getType() == FT_STRING)
        {
            if(!isMethod) {
                f.output("if s.%s != \"\" {", goName.c_str());
                f.indent();
            }
            f.output("if err := %s.WriteString(s.%s); err != nil {", recvName.c_str(), goName.c_str());
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(!isMethod) {
                f.recover();
                f.output("}");
            }
        }
        else if(field.getType() == FT_BOOL)
        {
            // Bool is always written in methods, optional in structs with fieldmask
            if(!isMethod) {
                f.output("if s.%s {", goName.c_str());
                f.indent();
            }
            f.output("if err := %s.WriteBool(s.%s); err != nil {", recvName.c_str(), goName.c_str());
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(!isMethod) {
                f.recover();
                f.output("}");
            }
        }
        else if(field.getType() == FT_ENUM)
        {
            if(!isMethod) {
                f.output("if s.%s != %s {", goName.c_str(), getFieldGoDefault(field));
                f.indent();
            }
            f.output("if err := %s.WriteUint8(uint8(s.%s)); err != nil {", recvName.c_str(), goName.c_str());
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(!isMethod) {
                f.recover();
                f.output("}");
            }
        }
        else
        {
            // Numeric types
            if(!isMethod) {
                f.output("if s.%s != %s {", goName.c_str(), getFieldGoDefault(field));
                f.indent();
            }

            if(field.getType() == FT_FLOAT || field.getType() == FT_DOUBLE)
            {
                std::string method = field.getType() == FT_FLOAT ? "WriteFloat32" : "WriteFloat64";
                f.output("if err := %s.%s(s.%s); err != nil {", recvName.c_str(), method.c_str(), goName.c_str());
                f.indent();
                f.output("return err");
                f.recover();
                f.output("}");
            }
            else
            {
                f.output("if err := %s.WriteInt64(int64(s.%s)); err != nil {", recvName.c_str(), goName.c_str());
                f.indent();
                f.output("return err");
                f.recover();
                f.output("}");
            }

            if(!isMethod) {
                f.recover();
                f.output("}");
            }
        }
    }
}

static void generateFieldDeserialize(CodeFile& f, Field& field, const std::string& recvName, bool isMethod)
{
    std::string goName = toGoFieldName(field.getNameC());
    uint32_t maxLen = getFieldMaxLength(field);

    if(field.getArray())
    {
        // Array deserialization
        if(!isMethod) {
            f.output("// deserialize %s", field.getNameC());
        }

        f.output("size, err := %s.ReadDynSize()", recvName.c_str());
        f.indent();
        f.output("if err != nil {");
        f.indent();
        f.output("return err");
        f.recover();
        f.output("}");
        f.output("if size > %u {", maxLen);
        f.indent();
        f.output("return arpc.ErrArrayTooLong");
        f.recover();
        f.output("}");

        if(isMethod) {
            f.output("%s := make(%s, size)", goName.c_str(), getFieldGoType(field));
        } else {
            f.output("s.%s = make(%s, size)", goName.c_str(), getFieldGoType(field));
        }

        f.recover();

        f.output("for i := uint32(0); i < size; i++ {");
        f.indent();

        if(field.getType() == FT_USER)
        {
            if(isMethod) {
                f.output("if err := %s[i].Deserialize(%s); err != nil {", goName.c_str(), recvName.c_str());
            } else {
                f.output("if err := s.%s[i].Deserialize(%s); err != nil {", goName.c_str(), recvName.c_str());
            }
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
        }
        else if(field.getType() == FT_ENUM)
        {
            f.output("eb, err := %s.ReadUint8()", recvName.c_str());
            f.indent();
            f.output("if err != nil {");
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(isMethod) {
                f.output("%s[i] = %s(eb)", goName.c_str(), field.getUserType()->getNameC());
            } else {
                f.output("s.%s[i] = %s(eb)", goName.c_str(), field.getUserType()->getNameC());
            }
            f.recover();
        }
        else if(field.getType() == FT_STRING)
        {
            f.output("str, err := %s.ReadString(%u)", recvName.c_str(), maxLen);
            f.indent();
            f.output("if err != nil {");
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(isMethod) {
                f.output("%s[i] = str", goName.c_str());
            } else {
                f.output("s.%s[i] = str", goName.c_str());
            }
            f.recover();
        }
        else if(field.getType() == FT_BOOL)
        {
            f.output("b, err := %s.ReadBool()", recvName.c_str());
            f.indent();
            f.output("if err != nil {");
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(isMethod) {
                f.output("%s[i] = b", goName.c_str());
            } else {
                f.output("s.%s[i] = b", goName.c_str());
            }
            f.recover();
        }
        else if(field.getType() == FT_FLOAT || field.getType() == FT_DOUBLE)
        {
            std::string method = field.getType() == FT_FLOAT ? "ReadFloat32" : "ReadFloat64";
            f.output("v, err := %s.%s()", recvName.c_str(), method.c_str());
            f.indent();
            f.output("if err != nil {");
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(isMethod) {
                f.output("%s[i] = v", goName.c_str());
            } else {
                f.output("s.%s[i] = v", goName.c_str());
            }
            f.recover();
        }
        else
        {
            // Integer types
            f.output("v, err := %s.ReadInt64()", recvName.c_str());
            f.indent();
            f.output("if err != nil {");
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(isMethod) {
                f.output("%s[i] = %s(v)", goName.c_str(), getFieldGoType(field, false));
            } else {
                f.output("s.%s[i] = %s(v)", goName.c_str(), getFieldGoType(field, false));
            }
            f.recover();
        }

        f.recover();
        f.output("}");
    }
    else
    {
        // Single field deserialization
        if(!isMethod) {
            f.output("// deserialize %s", field.getNameC());
        }

        if(field.getType() == FT_USER)
        {
            if(isMethod) {
                f.output("if err := %s.Deserialize(%s); err != nil {", goName.c_str(), recvName.c_str());
            } else {
                f.output("if err := s.%s.Deserialize(%s); err != nil {", goName.c_str(), recvName.c_str());
            }
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
        }
        else if(field.getType() == FT_STRING)
        {
            f.output("str, err := %s.ReadString(%u)", recvName.c_str(), maxLen);
            f.indent();
            f.output("if err != nil {");
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(isMethod) {
                f.output("%s = str", goName.c_str());
            } else {
                f.output("s.%s = str", goName.c_str());
            }
            f.recover();
        }
        else if(field.getType() == FT_BOOL)
        {
            f.output("b, err := %s.ReadBool()", recvName.c_str());
            f.indent();
            f.output("if err != nil {");
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(isMethod) {
                f.output("%s = b", goName.c_str());
            } else {
                f.output("s.%s = b", goName.c_str());
            }
            f.recover();
        }
        else if(field.getType() == FT_ENUM)
        {
            f.output("eb, err := %s.ReadUint8()", recvName.c_str());
            f.indent();
            f.output("if err != nil {");
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(isMethod) {
                f.output("%s = %s(eb)", goName.c_str(), field.getUserType()->getNameC());
            } else {
                f.output("s.%s = %s(eb)", goName.c_str(), field.getUserType()->getNameC());
            }
            f.recover();
        }
        else if(field.getType() == FT_FLOAT || field.getType() == FT_DOUBLE)
        {
            std::string method = field.getType() == FT_FLOAT ? "ReadFloat32" : "ReadFloat64";
            f.output("v, err := %s.%s()", recvName.c_str(), method.c_str());
            f.indent();
            f.output("if err != nil {");
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(isMethod) {
                f.output("%s = v", goName.c_str());
            } else {
                f.output("s.%s = v", goName.c_str());
            }
            f.recover();
        }
        else
        {
            // Integer types
            f.output("v, err := %s.ReadInt64()", recvName.c_str());
            f.indent();
            f.output("if err != nil {");
            f.indent();
            f.output("return err");
            f.recover();
            f.output("}");
            if(isMethod) {
                f.output("%s = %s(v)", goName.c_str(), getFieldGoType(field, false));
            } else {
                f.output("s.%s = %s(v)", goName.c_str(), getFieldGoType(field, false));
            }
            f.recover();
        }
    }
}

// ============================================================================
// Struct Generation
// ============================================================================

static void generateStructDecl(CodeFile& f, Struct* s)
{
    f.output("// struct %s", s->getNameC());
    f.output("type %s struct {", s->getNameC());

    f.indent();

    // Embed base struct if inherited
    if(s->super_)
    {
        f.output("%s", s->super_->getNameC());
        f.output("");
    }

    // Field list
    f.output("// member list");
    for(size_t i = 0; i < s->fields_.size(); i++)
    {
        Field& field = s->fields_[i];
        std::string goName = toGoFieldName(field.getNameC());
        std::string jsonTag = field.getNameC();
        f.output("%s %s `json:\"%s\"`",
            goName.c_str(),
            getFieldGoType(field),
            jsonTag.c_str());
    }

    f.recover();
    f.output("}");
    f.output("");

    // Field IDs
    f.output("// field ids");
    f.output("const (");
    f.indent();
    size_t fid = s->super_ ? s->super_->getFieldNum() : 0;
    for(size_t i = 0; i < s->fields_.size(); i++)
    {
        Field& field = s->fields_[i];
        std::string goName = toGoFieldName(field.getNameC());
        f.output("FID%s%s = %d", s->getNameC(), goName.c_str(), fid);
        fid++;
    }
    f.output("FID%sMax = %d", s->getNameC(), fid);
    f.recover();
    f.output(")");

    // Constructor
    f.output("");
    f.output("// New%s creates a new %s with default values", s->getNameC(), s->getNameC());
    f.output("func New%s() *%s {", s->getNameC(), s->getNameC());
    f.indent();
    f.output("return &%s{", s->getNameC());
    f.indent();
    for(size_t i = 0; i < s->fields_.size(); i++)
    {
        Field& field = s->fields_[i];
        if(getFieldGoDefault(field))
        {
            std::string goName = toGoFieldName(field.getNameC());
            f.output("%s: %s,", goName.c_str(), getFieldGoDefault(field));
        }
    }
    f.recover();
    f.output("}");
    f.recover();
    f.output("}");

    // Serialization method
    f.output("");
    f.output("// Serialize implements binary serialization");
    f.output("func (s *%s) Serialize(writer arpc.ProtocolWriter) error {", s->getNameC());
    f.indent();

    // Serialize base class first
    if(s->super_)
    {
        f.output("// Serialize base struct");
        f.output("if err := s.%s.Serialize(writer); err != nil {", s->super_->getNameC());
        f.indent();
        f.output("return err");
        f.recover();
        f.output("}");
        f.output("");
    }

    // Write field mask
    if(s->fields_.size() > 0)
    {
        // Write field mask length prefix for version compatibility
        f.output("// Write field mask length");
        f.output("fmLen := uint8((FID%sMax-1)/8+1)", s->getNameC());
        f.output("if err := writer.WriteUint8(fmLen); err != nil {");
        f.indent();
        f.output("return err");
        f.recover();
        f.output("}");

        f.output("// Write field mask");
        f.output("fm := arpc.NewFieldMask(FID%sMax)", s->getNameC());
        for(size_t i = 0; i < s->fields_.size(); i++)
        {
            Field& field = s->fields_[i];
            std::string goName = toGoFieldName(field.getNameC());

            if(field.getArray())
            {
                f.output("fm.WriteBit(len(s.%s) > 0)", goName.c_str());
            }
            else if(field.getType() == FT_USER)
            {
                f.output("fm.WriteBit(true)");
            }
            else if(field.getType() == FT_STRING)
            {
                f.output("fm.WriteBit(len(s.%s) > 0)", goName.c_str());
            }
            else if(field.getType() == FT_BOOL)
            {
                f.output("fm.WriteBit(s.%s)", goName.c_str());
            }
            else
            {
                f.output("fm.WriteBit(s.%s != %s)", goName.c_str(), getFieldGoDefault(field));
            }
        }
        f.output("if err := writer.Write(fm.Bytes()); err != nil {");
        f.indent();
        f.output("return err");
        f.recover();
        f.output("}");
        f.output("");
    }

    f.output("// Serialize fields");
    for(size_t i = 0; i < s->fields_.size(); i++)
    {
        generateFieldSerialize(f, s->fields_[i], "writer", false);
    }

    f.output("return nil");
    f.recover();
    f.output("}");

    // Deserialization method
    f.output("");
    f.output("// Deserialize implements binary deserialization");
    f.output("func (s *%s) Deserialize(reader arpc.ProtocolReader) error {", s->getNameC());
    f.indent();

    // Deserialize base class first
    if(s->super_)
    {
        f.output("// Deserialize base struct");
        f.output("if err := s.%s.Deserialize(reader); err != nil {", s->super_->getNameC());
        f.indent();
        f.output("return err");
        f.recover();
        f.output("}");
        f.output("");
    }

    // Read field mask
    if(s->fields_.size() > 0)
    {
        // Read field mask length prefix for version compatibility
        f.output("// Read field mask length");
        f.output("actualFmLen, err := reader.ReadUint8()");
        f.output("if err != nil {");
        f.indent();
        f.output("return err");
        f.recover();
        f.output("}");
        f.output("myFmLen := uint8((FID%sMax-1)/8+1)", s->getNameC());
        f.output("readFmLen := actualFmLen");
        f.output("if actualFmLen > myFmLen {");
        f.indent();
        f.output("readFmLen = myFmLen");
        f.recover();
        f.output("}");

        f.output("// Read field mask");
        f.output("fmBytes := make([]byte, myFmLen)");
        f.output("if _, err := reader.Read(fmBytes[:readFmLen]); err != nil {");
        f.indent();
        f.output("return err");
        f.recover();
        f.output("}");

        // Skip remaining field mask bytes
        f.output("// Skip remaining field mask bytes");
        f.output("if actualFmLen > readFmLen {");
        f.indent();
        f.output("if reader, ok := reader.(*arpc.MemReader); ok {");
        f.indent();
        f.output("if err := reader.Skip(uint32(actualFmLen - readFmLen)); err != nil {");
        f.indent();
        f.output("return err");
        f.recover();
        f.output("}");
        f.recover();
        f.output("}");
        f.recover();
        f.output("}");

        f.output("fm := &arpc.FieldMask{}");
        f.output("fm.SetBytes(fmBytes)");
        f.output("");
    }

    f.output("// Deserialize fields");
    for(size_t i = 0; i < s->fields_.size(); i++)
    {
        Field& field = s->fields_[i];
        std::string goName = toGoFieldName(field.getNameC());

        // Always use field mask for version compatibility
        f.output("if fm.ReadBit() {");
        f.indent();
        generateFieldDeserialize(f, field, "reader", false);
        f.recover();
        f.output("}");
    }

    f.output("return nil");
    f.recover();
    f.output("}");
}

// ============================================================================
// Service Generation
// ============================================================================

static void generateStubMethods(CodeFile& f, Service* s)
{
    size_t methodStartId = s->super_ ? s->super_->getMethodNum() : 0;

    for(size_t i = 0; i < s->methods_.size(); i++)
    {
        Method& method = s->methods_[i];
        uint16_t methodId = methodStartId + i;

        f.output("");
        f.output("// %s calls the remote method", method.getNameC());
        f.output("func (s *%sStub) %s(", s->getNameC(), method.getNameC());

        // Parameters
        for(size_t j = 0; j < method.fields_.size(); j++)
        {
            Field& field = method.fields_[j];
            std::string goName = toGoFieldName(field.getNameC());
            f.output("    %s %s,%s",
                goName.c_str(),
                getFieldGoType(field),
                (j < method.fields_.size() - 1) ? "" : ")");
        }

        f.output(" error {");
        f.indent();

        f.output("writer := s.beginMethod()");
        f.output("if writer == nil {");
        f.indent();
        f.output("return errors.New(\"failed to get writer\")");
        f.recover();
        f.output("}");
        f.output("");

        // Write method ID
        f.output("// Write method ID");
        f.output("if err := writer.WriteUint16(%d); err != nil {", methodId);
        f.indent();
        f.output("return err");
        f.recover();
        f.output("}");
        f.output("");

        // Serialize parameters
        f.output("// Serialize parameters");
        for(size_t j = 0; j < method.fields_.size(); j++)
        {
            generateFieldSerialize(f, method.fields_[j], "writer", true);
        }

        f.output("s.endMethod()");
        f.output("return nil");

        f.recover();
        f.output("}");
    }
}

static void generateStubDecl(CodeFile& f, Service* s)
{
    f.output("// service stub %s (client-side)", s->getNameC());

    if(s->super_)
    {
        f.output("type %sStub struct {", s->getNameC());
        f.output("    %sStub", s->super_->getNameC());
        f.output("}");
    }
    else
    {
        f.output("type %sStub struct {", s->getNameC());
        f.output("    // Transport callbacks");
        f.output("    beginMethod func() arpc.ProtocolWriter");
        f.output("    endMethod   func()");
        f.output("}");
        f.output("");
        f.output("// New%sStub creates a new service stub", s->getNameC());
        f.output("func New%sStub(", s->getNameC());
        f.indent();
        f.output("begin func() arpc.ProtocolWriter,");
        f.output("end func(),");
        f.recover();
        f.output(") *%sStub {", s->getNameC());
        f.indent();
        f.output("return &%sStub{", s->getNameC());
        f.indent();
        f.output("beginMethod: begin,");
        f.output("endMethod:   end,");
        f.recover();
        f.output("}");
        f.recover();
        f.output("}");
    }

    f.output("");

    // Generate method implementations
    generateStubMethods(f, s);
}

static void generateProxyDecl(CodeFile& f, Service* s)
{
    f.output("// service proxy %s (server-side interface)", s->getNameC());
    f.output("type %sProxy interface {", s->getNameC());
    f.indent();

    // Import methods from base service
    if(s->super_)
    {
        f.output("%sProxy", s->super_->getNameC());
        f.output("");
    }

    f.output("// methods");
    for(size_t i = 0; i < s->methods_.size(); i++)
    {
        Method& method = s->methods_[i];
        f.output("%s(", method.getNameC());

        for(size_t j = 0; j < method.fields_.size(); j++)
        {
            Field& field = method.fields_[j];
            std::string goName = toGoFieldName(field.getNameC());
            f.output("    %s %s%s",
                goName.c_str(),
                getFieldGoType(field),
                (j < method.fields_.size() - 1) ? ", " : ")");
        }

        if(i < s->methods_.size() - 1)
            f.output("");
    }

    f.recover();
    f.output("}");
    f.output("");
}

static void generateProxyDispatcher(CodeFile& f, Service* s)
{
    size_t methodStartId = s->super_ ? s->super_->getMethodNum() : 0;

    f.output("// %sDispatcher handles method dispatch for %s", s->getNameC(), s->getNameC());
    f.output("type %sDispatcher struct{}", s->getNameC());
    f.output("");
    f.output("// Dispatch reads the method ID and calls the appropriate handler");
    f.output("func (d *%sDispatcher) Dispatch(reader arpc.ProtocolReader, handler %sProxy) error {", s->getNameC(), s->getNameC());
    f.indent();

    f.output("// Read method ID");
    f.output("methodId, err := reader.ReadUint16()");
    f.indent();
    f.output("if err != nil {");
    f.indent();
    f.output("return err");
    f.recover();
    f.output("}");
    f.recover();
    f.output("");
    f.output("switch methodId {");
    f.indent();

    // Get all methods (including inherited)
    std::vector<Method*> allMethods;
    s->getAllMethods(allMethods);

    for(size_t i = 0; i < allMethods.size(); i++)
    {
        Method* method = allMethods[i];
        f.output("case %d:", i);
        f.indent();
        f.output("return d.dispatch%s(reader, handler)", method->getNameC());
        f.recover();
    }

    f.output("default:");
    f.indent();
    f.output("return arpc.ErrUnknownMethod");
    f.recover();
    f.recover();
    f.output("}");
    f.recover();
    f.output("}");
    f.output("");

    // Generate dispatch methods for each method in this service
    for(size_t i = 0; i < s->methods_.size(); i++)
    {
        Method& method = s->methods_[i];

        f.output("// dispatch%s handles deserialization and dispatch for %s", method.getNameC(), method.getNameC());
        f.output("func (d *%sDispatcher) dispatch%s(reader arpc.ProtocolReader, handler %sProxy) error {", s->getNameC(), method.getNameC(), s->getNameC());
        f.indent();

        // Declare parameters
        f.output("// Deserialize parameters");
        for(size_t j = 0; j < method.fields_.size(); j++)
        {
            Field& field = method.fields_[j];
            std::string goName = toGoFieldName(field.getNameC());
            const char* defVal = getFieldGoDefault(field);

            if(defVal && strlen(defVal) > 0)
            {
                f.output("%s := %s", goName.c_str(), defVal);
            }
            else
            {
                f.output("var %s %s", goName.c_str(), getFieldGoType(field));
            }
        }

        f.output("");
        for(size_t j = 0; j < method.fields_.size(); j++)
        {
            generateFieldDeserialize(f, method.fields_[j], "reader", true);
        }

        f.output("");
        f.output("// Call handler");
        f.output("return handler.%s(", method.getNameC());
        for(size_t j = 0; j < method.fields_.size(); j++)
        {
            Field& field = method.fields_[j];
            std::string goName = toGoFieldName(field.getNameC());
            f.output("%s%s", goName.c_str(), (j < method.fields_.size() - 1) ? ", " : "");
        }
        f.output(")");

        f.recover();
        f.output("}");
        f.output("");
    }
}

// ============================================================================
// Main Generate Function
// ============================================================================

void GoGenerator::generate()
{
    // Generate .go file
    std::string fn =
        Compiler::inst().outputDir_ +
        Compiler::inst().fileStem_ + ".go";
    CodeFile f(fn);

    // File header
    f.output("// Code generated by arpcc. DO NOT EDIT.");
    f.output("");
    f.output("package %s", toPackageName(Compiler::inst().filename_).c_str());
    f.output("");
    f.output("import (");
    f.indent();
    f.output("\"encoding/json\"");
    f.output("\"errors\"");
    f.output("");
    f.output("\"github.com/arpc/runtime\"");
    f.recover();
    f.output(")");
    f.output("");

    // Include imports for other files
    if(Compiler::inst().imports_.size() > 0)
    {
        for(size_t i = 0; i < Compiler::inst().imports_.size(); i++)
        {
            std::string importPkg = toPackageName(Compiler::inst().imports_[i]);
            f.output("import \"%s\"", importPkg.c_str());
        }
        f.output("");
    }

    // Generate definitions
    for(size_t i = 0; i < Compiler::inst().definitions_.size(); i++)
    {
        Definition* definition = Compiler::inst().definitions_[i];
        if(definition->getFile() != Compiler::inst().filename_)
            continue;

        f.output("//=============================================================");
        f.output("");

        if(definition->getEnum())
        {
            generateEnumDecl(f, definition->getEnum());
            generateEnumInfo(f, definition->getEnum());
        }
        else if(definition->getStruct())
        {
            generateStructDecl(f, definition->getStruct());
        }
        else if(definition->getService())
        {
            Service* svc = definition->getService();
            generateStubDecl(f, svc);
            generateProxyDecl(f, svc);
            generateProxyDispatcher(f, svc);
        }
    }
}
