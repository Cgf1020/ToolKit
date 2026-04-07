#include "base/json/json_helper.h"
#include <iostream>
#include <sstream>
#include <cstdio>

#include "rapidjson/reader.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/document.h"



namespace itflee {
    // 深拷贝
    void JSON::deep_copy_from(const JSON& other) {
        if (std::holds_alternative<std::monostate>(other.value_)) 
        {
            value_ = std::monostate{};
        } 
        else if (std::holds_alternative<bool>(other.value_)) 
        {
            value_ = std::get<bool>(other.value_);
        } 
        else if (std::holds_alternative<int>(other.value_)) 
        {
            value_ = std::get<int>(other.value_);
        } 
        else if (std::holds_alternative<int64_t>(other.value_)) 
        {
            value_ = std::get<int64_t>(other.value_);
        } 
        else if (std::holds_alternative<uint64_t>(other.value_)) 
        {
            value_ = std::get<uint64_t>(other.value_);
        } 
        else if (std::holds_alternative<float>(other.value_))
        {
            value_ = std::get<float>(other.value_);
        } 
        else if (std::holds_alternative<double>(other.value_))
        {
            value_ = std::get<double>(other.value_);
        } 
        else if (std::holds_alternative<std::string>(other.value_))
        {
            value_ = std::get<std::string>(other.value_);
        } 
        else if (std::holds_alternative<std::shared_ptr<array_type>>(other.value_)) 
        {
            const auto& src = std::get<std::shared_ptr<array_type>>(other.value_);
            if (src)
            {
                auto dst = std::make_shared<array_type>();
                dst->reserve(src->size());
                for (const auto& item : *src) {
                    dst->emplace_back(item);
                }
                value_ = dst;
            }
            else
            {
                value_ = std::shared_ptr<array_type>{};
            }
        } 
        else if (std::holds_alternative<std::shared_ptr<object_type>>(other.value_)) 
        {
            const auto& src = std::get<std::shared_ptr<object_type>>(other.value_);
            if (src)
            {
                auto dst = std::make_shared<object_type>();
                for (const auto& kv : *src) {
                    dst->emplace(kv.first, kv.second);
                }
                value_ = dst;
            }
            else
            {
                value_ = std::shared_ptr<object_type>{};
            }
        }
    }

    /*---------------------- 构造函数 ----------------------*/
    JSON::JSON() : value_(std::monostate{}) {
        JSON_DEBUG_LOG_PRINT("JSON()");
    }

    JSON::JSON(std::nullptr_t) : value_(std::monostate{}) {
        JSON_DEBUG_LOG_PRINT("JSON(std::nullptr_t)");
    }

    JSON::JSON(bool v) : value_(v) {
        JSON_DEBUG_LOG_PRINT("JSON(bool v) " << v);
    }

    JSON::JSON(int v) : value_(v) {
        JSON_DEBUG_LOG_PRINT("JSON(int v) " << v);
    }

    JSON::JSON(int64_t v) : value_(v) {
        JSON_DEBUG_LOG_PRINT("JSON(int64_t v) " << v);
    }

    JSON::JSON(uint64_t v) : value_(v) {
        JSON_DEBUG_LOG_PRINT("JSON(uint64_t v) " << v);
    }

    JSON::JSON(float v) : value_(v) {
        JSON_DEBUG_LOG_PRINT("JSON(float v) " << v);
    }

    JSON::JSON(double v) : value_(v) {
        JSON_DEBUG_LOG_PRINT("JSON(double v) " << v);
    }

    JSON::JSON(const char* s) : value_(std::string(s)) {
        JSON_DEBUG_LOG_PRINT("JSON(const char* s) " << s);
    }

    JSON::JSON(const std::string& s) : value_(s) {
        JSON_DEBUG_LOG_PRINT("JSON(const std::string& s) " << s);
    }

    JSON::JSON(const std::vector<JSON>& vec) : value_(std::make_shared<array_type>()) {
        JSON_DEBUG_LOG_PRINT("JSON(const std::vector<JSON>& vec) " << vec.size());
        array_type& arr = *std::get<std::shared_ptr<array_type>>(value_);
        arr.reserve(vec.size());
        for (const auto& item : vec) {
            arr.emplace_back(item);
        }
    }

    JSON::JSON(const std::map<std::string, JSON>& mp) : value_(std::make_shared<object_type>()) {
        JSON_DEBUG_LOG_PRINT("JSON(const std::map<std::string, JSON>& mp) " << mp.size());
        object_type& obj = *std::get<std::shared_ptr<object_type>>(value_);
        for (const auto& kv : mp) {
            obj.emplace(kv.first, kv.second);
        }
    }

    /*---------------------- 拷贝/移动/析构 ----------------------*/
    JSON::JSON(const JSON& other) {
        JSON_DEBUG_LOG_PRINT("JSON(const JSON& other)");
        deep_copy_from(other);
    }

    JSON::JSON(JSON&& other) noexcept {
        JSON_DEBUG_LOG_PRINT("JSON(JSON&& other) noexcept");
        value_ = std::move(other.value_);
        other.value_ = std::monostate{};
    }

    JSON::~JSON() {
        JSON_DEBUG_LOG_PRINT("JSON::~JSON()");
        reset();
    }

    JSON& JSON::operator=(const JSON& other) {
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(const JSON& other)");
        if (this == &other) return *this;
        reset();
        deep_copy_from(other);
        return *this;
    }

    JSON& JSON::operator=(JSON&& other) noexcept {
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(JSON&& other) noexcept");
        if (this == &other) return *this;
        reset();
        value_ = std::move(other.value_);
        other.value_ = std::monostate{};
        return *this;
    }


    /*----------------------重载的赋值运算符  赋值：原生类型 ----------------------*/
    JSON& JSON::operator=(std::nullptr_t) { 
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(std::nullptr_t)"); 
        reset(); 
        value_ = std::monostate{}; 
        return *this; 
    }
    
    JSON& JSON::operator=(bool v) { 
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(bool v)"); 
        reset(); 
        value_ = v; 
        return *this; 
    }
    
    JSON& JSON::operator=(int v) { 
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(int v)"); 
        reset(); 
        value_ = v; 
        return *this; 
    }
    
    JSON& JSON::operator=(int64_t v) { 
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(int64_t v)"); 
        reset(); 
        value_ = v; 
        return *this; 
    }
    
    JSON& JSON::operator=(uint64_t v) { 
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(uint64_t v)"); 
        reset(); 
        value_ = v; 
        return *this; 
    }
    
    JSON& JSON::operator=(float v) { 
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(float v)"); 
        reset(); 
        value_ = v; 
        return *this; 
    }
    
    JSON& JSON::operator=(double v) { 
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(double v)"); 
        reset(); 
        value_ = v; 
        return *this; 
    }
    
    JSON& JSON::operator=(const char* s) { 
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(const char* s)"); 
        reset(); 
        value_ = std::string(s ? s : ""); 
        return *this; 
    }
    
    JSON& JSON::operator=(const std::string& s) { 
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(const std::string& s)"); 
        reset(); 
        value_ = s; 
        return *this; 
    }

    JSON& JSON::operator=(const std::vector<JSON>& vec)  {
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(const std::vector<JSON>& vec)");
        reset();
        value_ = std::make_shared<array_type>();
        array_type& arr = *std::get<std::shared_ptr<array_type>>(value_);
        arr.reserve(static_cast<size_t>(vec.size()));
        for (const auto& item : vec) {
            arr.emplace_back(item);  // 这里会调用 JSON(const JSON& other) 
        }
        return *this;
    }
    
    JSON& JSON::operator=(const std::map<std::string, JSON>& mp) {
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator=(const std::map<std::string, JSON>& mp)");
        reset();
        value_ = std::make_shared<object_type>();
        object_type& obj = *std::get<std::shared_ptr<object_type>>(value_);
        for (const auto& kv : mp) {
            obj.emplace(kv.first, kv.second);
        }
        return *this;
    }


    /*---------------------- 隐式转换 ----------------------*/
    JSON::operator bool() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator bool() const");
        return as<bool>();
    }

    JSON::operator int() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator int() const");
        return as<int>();
    }

    JSON::operator int64_t() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator int64_t() const");
        return as<int64_t>();
    }

    JSON::operator uint64_t() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator uint64_t() const");
        return as<uint64_t>();
    }

    JSON::operator float() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator float() const");
        return as<float>();
    }

    JSON::operator double() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator double() const");
        return as<double>();
    }
    
    JSON::operator std::string() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::string() const");
        return as<std::string>();
    }

    JSON::operator std::vector<JSON>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::vector<JSON>() const");
        return as<std::vector<JSON>>();
    }

    JSON::operator std::map<std::string, JSON>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::map<std::string, JSON>() const");
        return as<std::map<std::string, JSON>>();
    }

    // === 数组隐式转换 ===
    JSON::operator std::vector<bool>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::vector<bool>() const");
        return as<std::vector<bool>>();
    }

    JSON::operator std::vector<int>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::vector<int>() const");
        return as<std::vector<int>>();
    }

    JSON::operator std::vector<int64_t>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::vector<int64_t>() const");
        return as<std::vector<int64_t>>();
    }

    JSON::operator std::vector<uint64_t>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::vector<uint64_t>() const");
        return as<std::vector<uint64_t>>();
    }

    JSON::operator std::vector<float>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::vector<float>() const");
        return as<std::vector<float>>();
    }

    JSON::operator std::vector<double>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::vector<double>() const");
        return as<std::vector<double>>();
    }

    JSON::operator std::vector<std::string>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::vector<std::string>() const");
        return as<std::vector<std::string>>();
    }

    // === map 隐式转换 ===
    JSON::operator std::map<std::string, bool>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::map<std::string,bool>() const");
        return as<std::map<std::string, bool>>();
    }

    JSON::operator std::map<std::string, int>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::map<std::string,int>() const");
        return as<std::map<std::string, int>>();
    }

    JSON::operator std::map<std::string, int64_t>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::map<std::string,int64_t>() const");
        return as<std::map<std::string, int64_t>>();
    }

    JSON::operator std::map<std::string, uint64_t>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::map<std::string,uint64_t>() const");
        return as<std::map<std::string, uint64_t>>();
    }

    JSON::operator std::map<std::string, float>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::map<std::string,float>() const");
        return as<std::map<std::string, float>>();
    }

    JSON::operator std::map<std::string, double>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::map<std::string,double>() const");
        return as<std::map<std::string, double>>();
    }

    JSON::operator std::map<std::string, std::string>() const {
        JSON_DEBUG_LOG_PRINT("JSON::operator std::map<std::string,std::string>() const");
        return as<std::map<std::string, std::string>>();
    }

    /*---------------------- 内部工具 ----------------------*/
    void JSON::reset() {
        // shared_ptr 自动管理内存，这里只需清空 variant
        value_ = std::monostate{};
    }

    void JSON::ensure_object() {
        if (std::holds_alternative<std::shared_ptr<object_type>>(value_)) return;
        reset();
        value_ = std::make_shared<object_type>();
    }

    // 仅供内部使用：返回数组 / 对象的 const 引用，避免在热路径上产生拷贝
    const JSON::array_type& JSON::get_array_ref() const {
        if (!std::holds_alternative<std::shared_ptr<array_type>>(value_)) {
            throw std::runtime_error("JSON: value is not array");
        }
        const auto& ptr = std::get<std::shared_ptr<array_type>>(value_);
        if (!ptr) {
            static const array_type empty_array{};
            return empty_array;
        }
        return *ptr;
    }

    const JSON::object_type& JSON::get_object_ref() const {
        if (!std::holds_alternative<std::shared_ptr<object_type>>(value_)) {
            throw std::runtime_error("JSON: value is not object");
        }
        const auto& ptr = std::get<std::shared_ptr<object_type>>(value_);
        if (!ptr) {
            static const object_type empty_object{};
            return empty_object;
        }
        return *ptr;
    }

    /*---------------------- 索引访问/创建 ----------------------*/
    JSON& JSON::operator[](const std::string& key) {
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator[](const std::string& key)" << key);
        ensure_object();
        auto& obj = std::get<std::shared_ptr<object_type>>(value_);
        return (*obj)[key];
    }

    JSON& JSON::operator[](const char* key) {
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator[](const char* key)" << key);
        return (*this)[std::string(key ? key : "")];
    }

    const JSON& JSON::operator[](const std::string& key) const {
        JSON_DEBUG_LOG_PRINT("const JSON& JSON::operator[](const std::string& key) const" << key);
        if (!is_object()) {
            throw std::runtime_error("JSON: value is not object");
        }
        const auto& obj = std::get<std::shared_ptr<object_type>>(value_);
        auto it = obj->find(key);
        if (it == obj->end()) {
            throw std::out_of_range("JSON: key not found: " + key);
        }
        return it->second;
    }

    const JSON& JSON::operator[](const char* key) const {
        JSON_DEBUG_LOG_PRINT("const JSON& JSON::operator[](const char* key) const" << key);
        return (*this)[std::string(key ? key : "")];
    }

    /*---------------------- 数组索引访问 ----------------------*/
    JSON& JSON::operator[](int index) {
        JSON_DEBUG_LOG_PRINT("JSON& JSON::operator[](int index) " << index);
        auto& arr = std::get<std::shared_ptr<array_type>>(value_);
        if (index < 0 || static_cast<size_t>(index) >= arr->size()) {
            throw std::out_of_range("JSON: array index out of range");
        }
        return (*arr)[index];
    }

    const JSON& JSON::operator[](int index) const {
        JSON_DEBUG_LOG_PRINT("const JSON& JSON::operator[](int index) const " << index);
        const auto& arr = std::get<std::shared_ptr<array_type>>(value_);
        if (index < 0 || static_cast<size_t>(index) >= arr->size()) {
            throw std::out_of_range("JSON: array index out of range");
        }
        return (*arr)[index];
    }

    /*---------------------- 类型判断 ----------------------*/
    bool JSON::is_null() const { return std::holds_alternative<std::monostate>(value_); }
    bool JSON::is_boolean() const { return std::holds_alternative<bool>(value_); }
    bool JSON::is_integer() const { return std::holds_alternative<int>(value_); }
    bool JSON::is_int64() const { return std::holds_alternative<int64_t>(value_); }
    bool JSON::is_uint64() const { return std::holds_alternative<uint64_t>(value_); }
    bool JSON::is_float() const { return std::holds_alternative<float>(value_); }
    bool JSON::is_double() const { return std::holds_alternative<double>(value_); }
    bool JSON::is_number() const { 
        return std::holds_alternative<int>(value_) || 
               std::holds_alternative<int64_t>(value_) || 
               std::holds_alternative<uint64_t>(value_) || 
               std::holds_alternative<float>(value_) || 
               std::holds_alternative<double>(value_); 
    }
    bool JSON::is_string() const { return std::holds_alternative<std::string>(value_); }
    bool JSON::is_array() const { return std::holds_alternative<std::shared_ptr<array_type>>(value_); }
    bool JSON::is_object() const { return std::holds_alternative<std::shared_ptr<object_type>>(value_); }

    /*---------------------- 判断是否存在某个键 ----------------------*/
    bool JSON::contains(const std::string& key) const {
        if (!is_object()) {
            return false;
        }
        const auto& obj = std::get<std::shared_ptr<object_type>>(value_);
        return obj->find(key) != obj->end();
    }

    bool JSON::contains(const char* key) const {
        if (!key) {
            return false;
        }
        return contains(std::string(key));
    }

    /*---------------------- 流输出操作符 ----------------------*/
    std::ostream& operator<<(std::ostream& os, const JSON& json) {
        os << json.serialize();
        return os;
    }

    /*---------------------- 序列化与反序列化 ----------------------*/

    // 辅助：转义字符串
    static std::string escape_string(const std::string& input)
    {
        std::string out;
        out.reserve(input.size() + 8);
        for (unsigned char ch : input)
        {
            switch (ch)
            {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (ch < 0x20)
                {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04X", (unsigned)ch);
                    out += buf;
                }
                else
                {
                    out += static_cast<char>(ch);
                }
            }
        }
        return out;
    }

    // 辅助：缩进
    static inline void append_indent(std::string& out, int indent, int level)
    {
        for (int i = 0; i < indent * level; ++i) out += ' ';
    }

    // 递归格式化（使用公开接口）
    static void serialize_impl(const JSON& node, std::string& out, int indent, int level)
    {
        if (node.is_null())
        {
            out += "null";
            return;
        }
        if (node.is_boolean())
        {
            out += (node.as<bool>() ? "true" : "false");
            return;
        }
        if (node.is_integer())
        {
            out += std::to_string(node.as<int>());
            return;
        }
        if (node.is_int64())
        {
            out += std::to_string(node.as<int64_t>());
            return;
        }
        if (node.is_uint64())
        {
            out += std::to_string(node.as<uint64_t>());
            return;
        }
        if (node.is_float())
        {
            std::ostringstream oss;
            oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
            oss.precision(7);  // float 精度为7位
            oss << node.as<float>();
            out += oss.str();
            return;
        }
        if (node.is_number())
        {
            std::ostringstream oss;
            oss.setf(std::ios::fmtflags(0), std::ios::floatfield);  // 设置浮点数格式
            oss.precision(15);  // 设置精度为15位
            oss << node.as<double>();
            out += oss.str();
            return;
        }
        if (node.is_string())
        {
            out += '"';
            out += escape_string(node.as<std::string>());
            out += '"';
            return;
        }
        if (node.is_array())
        {
            // 使用引用，避免整体拷贝
            const auto& arr = node.get_array_ref();
            out += "[\n";
            for (size_t i = 0; i < arr.size(); ++i)
            {
                append_indent(out, indent, level + 1);
                serialize_impl(arr[i], out, indent, level + 1);
                if (i + 1 < arr.size()) out += ",";
                out += "\n";
            }
            append_indent(out, indent, level);
            out += "]";
            return;
        }
        if (node.is_object())
        {
            // 使用引用，避免整体拷贝
            const auto& obj = node.get_object_ref();
            out += "{\n";
            size_t count = 0;
            for (const auto& kv : obj)
            {
                append_indent(out, indent, level + 1);
                out += '"';
                out += escape_string(kv.first);
                out += "\": ";
                serialize_impl(kv.second, out, indent, level + 1);
                if (++count < obj.size()) out += ",";
                out += "\n";
            }
            append_indent(out, indent, level);
            out += "}";
            return;
        }
        out += "null";
    }

    // 序列化
    std::string JSON::serialize() const 
	{
        std::string out;
        out.reserve(128);   // 预留128个字符空间
        serialize_impl(*this, out, 2, 0);
        return out;
	}

    std::string JSON::to_string() const {
        return serialize();
    }

    

    // -------- RapidJSON 序列化实现 --------
    static void to_rapidjson_value(const JSON& node, rapidjson::Value& out, rapidjson::Document::AllocatorType& alloc)
    {
        if (node.is_null()) { out.SetNull(); return; }
        if (node.is_boolean()) { out.SetBool(node.as<bool>()); return; }
        if (node.is_integer()) { out.SetInt(node.as<int>()); return; }
        if (node.is_int64()) { out.SetInt64(node.as<int64_t>()); return; }
        if (node.is_uint64()) { out.SetUint64(node.as<uint64_t>()); return; }
        if (node.is_float()) { out.SetFloat(node.as<float>()); return; }
        if (node.is_number()) { out.SetDouble(node.as<double>()); return; }
        if (node.is_string()) {
            auto s = node.as<std::string>();
            out.SetString(s.c_str(), static_cast<rapidjson::SizeType>(s.size()), alloc);
            return;
        }
        if (node.is_array()) {
            out.SetArray();
            const auto& arr = node.get_array_ref();
            out.Reserve(static_cast<rapidjson::SizeType>(arr.size()), alloc);
            for (const auto& item : arr) {
                rapidjson::Value v;
                to_rapidjson_value(item, v, alloc);
                out.PushBack(v, alloc);
            }
            return;
        }
        if (node.is_object()) {
            out.SetObject();
            const auto& obj = node.get_object_ref();
            for (const auto& kv : obj) {
                rapidjson::Value k;
                k.SetString(kv.first.c_str(), static_cast<rapidjson::SizeType>(kv.first.size()), alloc);
                rapidjson::Value v;
                to_rapidjson_value(kv.second, v, alloc);
                out.AddMember(k, v, alloc);
            }
            return;
        }
        out.SetNull();
    }

    std::string JSON::serialize_compact_rj() const
    {
        rapidjson::Document doc;
        auto& alloc = doc.GetAllocator();
        rapidjson::Value root;
        to_rapidjson_value(*this, root, alloc);
        doc.Swap(root);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);
        return buffer.GetString();
    }

    std::string JSON::serialize_pretty_rj(int indent) const
    {
        rapidjson::Document doc;
        auto& alloc = doc.GetAllocator();
        rapidjson::Value root;
        to_rapidjson_value(*this, root, alloc);
        doc.Swap(root);

        rapidjson::StringBuffer buffer;
        rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
        writer.SetIndent(' ', indent < 0 ? 0 : indent);
        doc.Accept(writer);
        return buffer.GetString();
    }

    // 反序列化：使用 rapidjson 将字符串解析为 itflee::JSON
    static JSON from_rapidjson(const rapidjson::Value& v)
    {
        if (v.IsNull()) return JSON(nullptr);
        if (v.IsBool()) return JSON(v.GetBool());
        if (v.IsInt()) return JSON(v.GetInt());
        if (v.IsInt64()) return JSON(v.GetInt64());
        if (v.IsUint64()) return JSON(v.GetUint64());
        if (v.IsFloat()) return JSON(v.GetFloat());
        if (v.IsDouble()) return JSON(v.GetDouble());
        if (v.IsString()) return JSON(std::string(v.GetString(), v.GetStringLength()));
        if (v.IsArray())
        {
            std::vector<JSON> arr;
            arr.reserve(v.Size());
            for (auto it = v.Begin(); it != v.End(); ++it)
            {
                arr.emplace_back(from_rapidjson(*it));
            }
            return JSON(arr);
        }
        if (v.IsObject())
        {
            std::map<std::string, JSON> obj;
            for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it)
            {
                const auto& name = it->name;
                obj.emplace(std::string(name.GetString(), name.GetStringLength()), from_rapidjson(it->value));
            }
            return JSON(obj);
        }
        return JSON(nullptr);
    }

    JSON JSON::deserialize(const std::string& jsonText)
    {
        rapidjson::Document doc;
        doc.Parse(jsonText.c_str());
        if (doc.HasParseError())
        {
            throw std::runtime_error("JSON parse error at offset " + std::to_string(doc.GetErrorOffset()));
        }
        return from_rapidjson(doc);
    }

}