#include "ToolKit/log_helper.h"
#include "ToolKit/Json_helper.h"

#include <iostream>

namespace utils {

    JsonObject::JsonObject() : obj_() {}

    std::string JsonObject::serialize() {
        return boost::json::serialize(obj_);
    }

    JsonObject JsonObject::fromJson(const std::string& jsonData) {
        JsonObject obj;
        boost::json::error_code ec;
        boost::json::value jsonValue = boost::json::parse(jsonData, ec);
        if (jsonValue.is_object()) {
            obj.obj_ = jsonValue.as_object();
            if (ec) {
                std::cerr << "Error occurred during JSON deserialization: " << ec.message() << std::endl;
            }
        }
        return obj;
    }

    void JsonObject::setString(const std::string& key, const std::string& value) {
        obj_[key] = value;
    }

    void JsonObject::setObject(const std::string& key, const JsonObject& value) {
        obj_[key] = value.obj_;
    }

    void JsonObject::setArray(const std::string& key, const std::vector<JsonObject>& array)
    {
        boost::json::array arr;
        for (const auto& it : array)
        {
            arr.emplace_back(it.obj_);
        }
        obj_[key] = std::move(arr);
    }

    void JsonObject::setInt64(const std::string& key, int64_t value) {
        obj_[key] = value;
    }

    void JsonObject::setUInt64(const std::string& key, uint64_t value) {
        obj_[key] = value;
    }

    void JsonObject::setBool(const std::string& key, bool value) {
        obj_[key] = value;
    }

    std::string JsonObject::getString(const std::string& key) {
        if (obj_.contains(key)) {
            const boost::json::value& value = obj_.at(key);
            if (value.is_string()) {
                return value.as_string().c_str();
            }
        }
        return "";
    }

    int64_t JsonObject::getInt64(const std::string& key) {
        if (obj_.contains(key)) {
            const boost::json::value& value = obj_.at(key);
            if (value.is_int64()) {
                return value.as_int64();
            }
        }
        return -1;
    }

    uint64_t JsonObject::getUInt64(const std::string& key) {
        if (obj_.contains(key)) {
            const boost::json::value& value = obj_.at(key);
            if (value.is_uint64()) {
                return value.as_uint64();
            }
        }
        return 0;
    }

    bool JsonObject::getBool(const std::string& key) {
        if (obj_.contains(key)) {
            const boost::json::value& value = obj_.at(key);
            if (value.is_bool()) {
                return value.as_bool();
            }
        }
        return false;
    }

    JsonObject JsonObject::getObject(const std::string& key) {
        JsonObject nestedObj;
        if (obj_.contains(key)) {
            const boost::json::value& value = obj_.at(key);
            if (value.is_object()) {
                nestedObj.obj_ = value.get_object();
            }
        }
        return nestedObj;
    }

    bool JsonObject::contains(const std::string& key) {
        if (obj_.contains(key)) {
            return true;
        }
        return false;
    }
    /*std::string JsonObject::getString(const std::string& key) const {
        if (obj_.contains(key)) {
            const boost::json::value& value = obj_.at(key);
            if (value.is_string()) {
                return value.as_string().c_str();
            }
        }
        return std::string();
    }
    int64_t JsonObject::getInt64(const std::string& key) const {
        if (obj_.contains(key)) {
            const boost::json::value& value = obj_.at(key);
            if (value.is_int64()) {
                return value.as_int64();
            }
        }
        return -1;
    }
    uint64_t JsonObject::getUInt64(const std::string& key) const {
        if (obj_.contains(key)) {
            const boost::json::value& value = obj_.at(key);
            if (value.is_uint64()) {
                return value.as_uint64();
            }
        }
        return 0;
    }
    bool JsonObject::getBool(const std::string& key) const {
        if (obj_.contains(key)) {
            const boost::json::value& value = obj_.at(key);
            if (value.is_uint64()) {
                return value.as_uint64();
            }
        }
        return 0;
    }
    JsonObject JsonObject::getObject(const std::string& key) const {
        JsonObject nestedObj;
        if (obj_.contains(key)) {
            const boost::json::value& value = obj_.at(key);
            if (value.is_object()) {
                nestedObj.obj_ = value.get_object();
            }
        }
        return nestedObj;
    }
    bool JsonObject::contains(const std::string& key) const {
        if (obj_.contains(key)) {
            return true;
        }
        return false;
    }*/
} // namespace Utils



/*-------------------将json字符串 打印输出为 json格式的字符串-------------------------*/
//string formatJson(string json)
//{
//    string result = "";
//    int level = 0;
//    for (string::size_type index = 0; index < json.size(); index++)
//    {
//        char c = json[index];
//        if (level > 0 && '\n' == json[json.size() - 1])
//        {
//            result += getLevelStr(level);
//        }
//
//        switch (c)
//        {
//        case '{':
//        case '[':
//            result = result + c + "\n";
//            level++;
//            result += getLevelStr(level);
//            break;
//        case ',':string getLevelStr(int level)
//        {
//            string levelStr = "";
//            for (int i = 0; i < level; i++)
//            {
//                levelStr += "\t"; //这里可以\t换成你所需要缩进的空格数
//            }
//            return levelStr;
//
//        }
//
//
//                string formatJson(string json)
//                {
//                    string result = "";
//                    int level = 0;
//                    for (string::size_type index = 0; index < json.size(); index++)
//                    {
//                        char c = json[index];
//
//                        if (level > 0 && '\n' == json[json.size() - 1])
//                        {
//                            result += getLevelStr(level);
//                        }
//
//                        switch (c)
//                        {
//                        case '{':
//                        case '[':
//                            result = result + c + "\n";
//                            level++;
//                            result += getLevelStr(level);
//                            break;
//                        case ',':
//                            result = result + c + "\n";
//                            result += getLevelStr(level);
//                            break;
//                        case '}':
//                        case ']':
//                            result += "\n";
//                            level--;
//                            result += getLevelStr(level);
//                            result += c;
//                            break;
//                        default:
//                            result += c;
//                            break;
//                        }
//
//                    }
//                    return result;
//                }
//
//
//
//
//
//
//                int main()
//                {
//                    std::cout << "Hello World!\n";
//
//                    string str = "{\"name\":\"John Doe\",\"age\":30,\"city\":\"New York\"}";
//
//                    std::cout << formatJson(str) << std::endl;
//
//
//                }
//
//                result = result + c + "\n";
//                result += getLevelStr(level);
//                break;
//        case '}':
//        case ']':
//            result += "\n";
//            level--;
//            result += getLevelStr(level);
//            result += c;
//            break;
//        default:
//            result += c;
//            break;
//        }
//
//    }
//    return result;
//}

/*-------------别的boost解析json，使用的是不用的操作---------------*/

//#include <boost/json/src.hpp>
//#include <iomanip>
//
//
//boost::json::value parse_demo_str(std::string str) {
//    boost::json::stream_parser p;
//    boost::json::error_code ec;
//
//    p.write(str.c_str(), str.length(), ec);
//    if (ec)
//        return nullptr;
//
//    p.finish(ec);
//    if (ec)
//        return nullptr;
//    return p.release();
//}
//
//void pretty_print(std::ostream& os, boost::json::value const& jv, std::string* indent = nullptr) {
//    std::string indent_;
//    if (!indent)
//        indent = &indent_;
//
//    switch (jv.kind())
//    {
//    case boost::json::kind::object:
//    {
//        os << "{\n";
//        indent->append(4, ' ');
//        auto const& obj = jv.get_object();
//        if (!obj.empty())
//        {
//            auto it = obj.begin();
//            for (;;)
//            {
//                os << *indent << boost::json::serialize(it->key()) << " : ";
//                pretty_print(os, it->value(), indent);
//                if (++it == obj.end())
//                    break;
//                os << ",\n";
//            }
//        }
//        os << "\n";
//        indent->resize(indent->size() - 4);
//        os << *indent << "}";
//        break;
//    }
//
//    case boost::json::kind::array:
//    {
//        os << "[\n";
//        indent->append(4, ' ');
//        auto const& arr = jv.get_array();
//        if (!arr.empty())
//        {
//            auto it = arr.begin();
//            for (;;)
//            {
//                os << *indent;
//                pretty_print(os, *it, indent);
//                if (++it == arr.end())
//                    break;
//                os << ",\n";
//            }
//        }
//        os << "\n";
//        indent->resize(indent->size() - 4);
//        os << *indent << "]";
//        break;
//    }
//
//    case boost::json::kind::string:
//    {
//        os << boost::json::serialize(jv.get_string());
//        break;
//    }
//
//    case boost::json::kind::uint64:
//        os << jv.get_uint64();
//        break;
//
//    case boost::json::kind::int64:
//        os << jv.get_int64();
//        break;
//
//    case boost::json::kind::double_:
//        os << jv.get_double();
//        break;
//
//    case boost::json::kind::bool_:
//        if (jv.get_bool())
//            os << "true";
//        else
//            os << "false";
//        break;
//
//    case boost::json::kind::null:
//        os << "null";
//        break;
//    }
//
//    if (indent->empty())
//        os << "\n";
//}
