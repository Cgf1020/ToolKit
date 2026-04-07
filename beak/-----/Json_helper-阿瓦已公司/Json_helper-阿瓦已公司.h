#pragma once

#include <boost/json.hpp>
#include <sstream>
#include <vector>

namespace utils {
    class JsonObject {
        enum DataType { Array, Object };

    public:
        JsonObject();
        /*
         * 往json对象中设置值
         */
        void setBool(const std::string& key, bool value);
        void setInt64(const std::string& key, int64_t value);
        void setUInt64(const std::string& key, uint64_t value);
        void setString(const std::string& key, const std::string& value);
        void setObject(const std::string& key, const JsonObject& value);


        /*Template*/
        template <typename T> void set(const std::string& key, const T& value) {
            obj_[key] = value;
        }

        void setArray(const std::string& key, const std::vector<JsonObject>& array);

        template <typename T> void setArray(const std::string& key, const std::vector<T>& array) {
            boost::json::array arr;
            for (const auto& elem : array) {
                arr.push_back(elem);
            }
            obj_[key] = std::move(arr);
        }

        /*
         * 从json对象中获取值
         */
        std::string getString(const std::string& key);
        int64_t getInt64(const std::string& key);
        uint64_t getUInt64(const std::string& key);
        bool getBool(const std::string& key);
        JsonObject getObject(const std::string& key);

        template <typename T> T get(const std::string& key) {
            if (obj_.contains(key)) {
                return convertValue<T>(obj_.at(key));
            }
            return T();
        }

        template <typename T> std::vector<T> getArray(const std::string& key) {
            std::vector<T> result;
            if (obj_.contains(key)) {
                const boost::json::array& jsonArray = obj_.at(key).as_array();
                for (const auto& value : jsonArray) {
                    result.push_back(/*boost::json::value_to<T>(value)*/ convertValue<T>(value));
                }
            }
            return result;
        }

        /*
         * 从json对象中查询是否存在此key值
         */
        bool contains(const std::string& key);



        /*
         * 将json字符串转换成 json对象
         */
        static JsonObject fromJson(const std::string& json);

        /*
         * 将json对象转换成json字符串
         */
        std::string serialize();

    private:
        boost::json::object obj_;

        template <typename T> T convertValue(const boost::json::value& value) const {
            return boost::json::value_to<T>(value);
        }

        template <> JsonObject convertValue<JsonObject>(const boost::json::value& value) const {
            JsonObject helper;
            helper.obj_ = value.as_object();
            return helper;
        }
    };
} // namespace utils
