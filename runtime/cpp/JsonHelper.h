#ifndef __JSON_HELPER_H__
#define __JSON_HELPER_H__

#include <string>
#include <stdint.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include "Common.h"
#include "EnumInfo.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

/** Escape JSON string - handles special characters like quotes, backslashes, etc.
    This function should be used when outputting strings in JSON format.
*/
inline std::string escapeJsonString(const std::string& s) {
	std::ostringstream oss;
	for (char c : s) {
		switch (c) {
			case '"':  oss << "\\\""; break;
			case '\\': oss << "\\\\"; break;
			case '\b': oss << "\\b"; break;
			case '\f': oss << "\\f"; break;
			case '\n': oss << "\\n"; break;
			case '\r': oss << "\\r"; break;
			case '\t': oss << "\\t"; break;
			default:
				// Control characters (0x00-0x1F) should be escaped as \uXXXX
				if (c >= 0 && c <= 0x1F) {
					oss << "\\u" << std::hex << std::setw(4) << std::setfill('0') << (int)(unsigned char)c << std::dec;
				} else {
					oss << c;
				}
				break;
		}
	}
	return oss.str();
}

/** JsonReader: rapidjson 封装类，为 Struct 的 loadJson 提供类型安全的读取接口

设计原则：
1. 与现有 deserialize() 保持相同的错误处理模式（返回 bool）
2. 支持所有数据类型：基本类型、字符串、数组、嵌套 Struct、枚举
3. 宽松模式：字段缺失时跳过（使用默认值），类型不匹配时返回 false
4. 使用栈式 API 避免拷贝问题（rapidjson::Document 不可拷贝）

使用示例：
    JsonReader reader("{\"name\":\"hello\",\"value\":123}");
    if (!reader.isValid()) { return false; }
    int64_t value;
    if (!reader.readInt("value", value)) { return false; }

    // 嵌套对象
    if (reader.enterObject("nested")) {
        reader.readInt("field", nestedValue);
        reader.leave();
    }

    // 数组
    if (reader.enterArray("items")) {
        size_t count = reader.getArraySize();
        for (size_t i = 0; i < count; i++) {
            if (reader.enterArrayItem(i)) {
                reader.readInt("item", itemValue);
                reader.leave();
            }
        }
        reader.leave();
    }
*/
class JsonReader
{
public:
	/** 从 JSON 字符串构造（根对象） */
	JsonReader(const char* json, size_t len)
		: currentValue_(NULL)
		, arrayIndex_(0)
	{
		document_.Parse(json, len);
		if (document_.IsObject())
		{
			currentValue_ = &document_;
			valueStack_.push_back(&document_);
		}
		else if (document_.HasParseError())
		{
			error_ = rapidjson::GetParseError_En(document_.GetParseError());
		}
	}

	~JsonReader()
	{
	}

	/** 禁止拷贝 */
	JsonReader(const JsonReader&) = delete;
	JsonReader& operator=(const JsonReader&) = delete;

	/** 检查解析是否成功 */
	bool isValid() const
	{
		return currentValue_ != NULL && currentValue_->IsObject();
	}

	/** 获取错误信息 */
	const char* getError() const
	{
		return error_.c_str();
	}

	/** 检查是否存在指定字段 */
	bool hasMember(const char* key) const
	{
		if (!currentValue_ || !currentValue_->IsObject())
			return false;
		return currentValue_->HasMember(key);
	}

	/** 读取整数类型（所有整型统一用 int64_t） */
	bool readInt(const char* key, int64_t& out)
	{
		if (!currentValue_ || !currentValue_->IsObject())
			return false;
		auto it = currentValue_->FindMember(key);
		if (it == currentValue_->MemberEnd())
			return false;
		const rapidjson::Value& v = it->value;

		if (v.IsInt())
		{
			out = v.GetInt();
			return true;
		}
		else if (v.IsInt64())
		{
			out = v.GetInt64();
			return true;
		}
		else if (v.IsUint())
		{
			out = v.GetUint();
			return true;
		}
		else if (v.IsUint64())
		{
			out = static_cast<int64_t>(v.GetUint64());
			return true;
		}
		return false;
	}

	/** 读取浮点类型 */
	bool readDouble(const char* key, double& out)
	{
		if (!currentValue_ || !currentValue_->IsObject())
			return false;
		auto it = currentValue_->FindMember(key);
		if (it == currentValue_->MemberEnd())
			return false;
		// Accept both integer and floating-point numbers
		if (it->value.IsDouble())
		{
			out = it->value.GetDouble();
			return true;
		}
		else if (it->value.IsInt())
		{
			out = static_cast<double>(it->value.GetInt());
			return true;
		}
		else if (it->value.IsInt64())
		{
			out = static_cast<double>(it->value.GetInt64());
			return true;
		}
		else if (it->value.IsUint())
		{
			out = static_cast<double>(it->value.GetUint());
			return true;
		}
		else if (it->value.IsUint64())
		{
			out = static_cast<double>(it->value.GetUint64());
			return true;
		}
		return false;
	}

	/** 读取字符串 */
	bool readString(const char* key, std::string& out, size_t maxLen)
	{
		if (!currentValue_ || !currentValue_->IsObject())
			return false;
		auto it = currentValue_->FindMember(key);
		if (it == currentValue_->MemberEnd())
			return false;
		if (!it->value.IsString())
			return false;
		const char* str = it->value.GetString();
		size_t len = it->value.GetStringLength();
		if (len > maxLen)
			return false;
		out.assign(str, len);
		return true;
	}

	/** 读取布尔值 */
	bool readBool(const char* key, bool& out)
	{
		if (!currentValue_ || !currentValue_->IsObject())
			return false;
		auto it = currentValue_->FindMember(key);
		if (it == currentValue_->MemberEnd())
			return false;
		if (!it->value.IsBool())
			return false;
		out = it->value.GetBool();
		return true;
	}

	/** 读取枚举值（通过枚举名称查找） */
	bool readEnum(const char* key, int32_t& out, EnumInfo* info)
	{
		if (!currentValue_ || !currentValue_->IsObject())
			return false;
		auto it = currentValue_->FindMember(key);
		if (it == currentValue_->MemberEnd())
			return false;

		// 支持两种格式：字符串名称 或 数字值
		if (it->value.IsString())
		{
			const char* name = it->value.GetString();
			int id = info->getItemId(name);
			if (id < 0)
				return false;
			out = id;
			return true;
		}
		else if (it->value.IsInt())
		{
			out = it->value.GetInt();
			return true;
		}
		return false;
	}

	/** 读取数组大小 */
	bool readArraySize(const char* key, size_t& out, size_t maxLen)
	{
		if (!currentValue_ || !currentValue_->IsObject())
			return false;
		auto it = currentValue_->FindMember(key);
		if (it == currentValue_->MemberEnd())
			return false;
		if (!it->value.IsArray())
			return false;
		size_t size = it->value.Size();
		if (size > maxLen)
			return false;
		out = size;
		return true;
	}

	/** 进入嵌套对象 */
	bool enterObject(const char* key)
	{
		if (!currentValue_ || !currentValue_->IsObject())
			return false;
		auto it = currentValue_->FindMember(key);
		if (it == currentValue_->MemberEnd())
			return false;
		if (!it->value.IsObject())
			return false;

		valueStack_.push_back(currentValue_);
		currentValue_ = &it->value;
		return true;
	}

	/** 进入数组 */
	bool enterArray(const char* key)
	{
		if (!currentValue_ || !currentValue_->IsObject())
			return false;
		auto it = currentValue_->FindMember(key);
		if (it == currentValue_->MemberEnd())
			return false;
		if (!it->value.IsArray())
			return false;

		valueStack_.push_back(currentValue_);
		currentValue_ = &it->value;
		return true;
	}

	/** 进入数组的指定元素 */
	bool enterArrayItem(size_t index)
	{
		if (!currentValue_ || !currentValue_->IsArray())
			return false;
		if (index >= currentValue_->Size())
			return false;

		valueStack_.push_back(currentValue_);
		currentValue_ = &(*currentValue_)[static_cast<rapidjson::SizeType>(index)];
		return true;
	}

	/** 获取当前数组的大小 */
	size_t getArraySize() const
	{
		if (!currentValue_ || !currentValue_->IsArray())
			return 0;
		return currentValue_->Size();
	}

	/** 返回到上一层 */
	void leave()
	{
		if (!valueStack_.empty())
		{
			currentValue_ = valueStack_.back();
			valueStack_.pop_back();
		}
	}

	/** 检查当前值是否有效 */
	bool isValueValid() const
	{
		return currentValue_ != NULL;
	}

	/** 从当前值读取整数（用于数组元素） */
	bool readValueInt(int64_t& out)
	{
		if (!currentValue_)
			return false;

		if (currentValue_->IsInt())
		{
			out = currentValue_->GetInt();
			return true;
		}
		else if (currentValue_->IsInt64())
		{
			out = currentValue_->GetInt64();
			return true;
		}
		else if (currentValue_->IsUint())
		{
			out = currentValue_->GetUint();
			return true;
		}
		else if (currentValue_->IsUint64())
		{
			out = static_cast<int64_t>(currentValue_->GetUint64());
			return true;
		}
		return false;
	}

	/** 从当前值读取浮点（用于数组元素） */
	bool readValueDouble(double& out)
	{
		if (!currentValue_)
			return false;

		// Accept both integer and floating-point numbers
		if (currentValue_->IsDouble())
		{
			out = currentValue_->GetDouble();
			return true;
		}
		else if (currentValue_->IsInt())
		{
			out = static_cast<double>(currentValue_->GetInt());
			return true;
		}
		else if (currentValue_->IsInt64())
		{
			out = static_cast<double>(currentValue_->GetInt64());
			return true;
		}
		else if (currentValue_->IsUint())
		{
			out = static_cast<double>(currentValue_->GetUint());
			return true;
		}
		else if (currentValue_->IsUint64())
		{
			out = static_cast<double>(currentValue_->GetUint64());
			return true;
		}
		return false;
	}

	/** 从当前值读取字符串（用于数组元素） */
	bool readValueString(std::string& out, size_t maxLen)
	{
		if (!currentValue_ || !currentValue_->IsString())
			return false;
		const char* str = currentValue_->GetString();
		size_t len = currentValue_->GetStringLength();
		if (len > maxLen)
			return false;
		out.assign(str, len);
		return true;
	}

	/** 从当前值读取布尔（用于数组元素） */
	bool readValueBool(bool& out)
	{
		if (!currentValue_ || !currentValue_->IsBool())
			return false;
		out = currentValue_->GetBool();
		return true;
	}

	/** 从当前值读取枚举（用于数组元素） */
	bool readValueEnum(int32_t& out, EnumInfo* info)
	{
		if (!currentValue_)
			return false;

		if (currentValue_->IsString())
		{
			const char* name = currentValue_->GetString();
			int id = info->getItemId(name);
			if (id < 0)
				return false;
			out = id;
			return true;
		}
		else if (currentValue_->IsInt())
		{
			out = currentValue_->GetInt();
			return true;
		}
		return false;
	}

private:
	rapidjson::Document document_;               // 根对象时持有所有权
	const rapidjson::Value* currentValue_;      // 当前作用域的 Value 指针
	std::vector<const rapidjson::Value*> valueStack_;  // 值栈，用于 enter/leave
	std::string error_;                          // 错误信息
	size_t arrayIndex_;                          // 当前数组索引（保留）
};

#endif//__JSON_HELPER_H__
