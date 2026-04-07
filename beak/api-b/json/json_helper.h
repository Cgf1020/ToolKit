#ifndef JSON_H_
#define JSON_H_

// #pragma warning( disable: 4251 )


#include <variant>
#include <optional>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <iostream>
#include <type_traits>
#include <iosfwd>
#include <cstdint>
#include "define.h"

/*------------------------------不足之处-----------------------------------
未实现的功能

-------------------------------------------------------------------------*/

/*使用案例
    itflee::JSON json;
    json["key"] = "value";
    json["key2"] = 1;
    json["key3"] = 1.1;
    json["key4"] = true;
    json["key5"] = std::vector<int>{1, 2, 3};
    json["key6"] = std::map<std::string, int>{{"key1", 1}, {"key2", 2}};

    itflee::JSON json2;
    json2 = json;
    
    json["key7"] = json2;

    itflee::JSON v1("hello");
    itflee::JSON v2(1);
    itflee::JSON v3(1.1);
    itflee::JSON v4(true);
    itflee::JSON v5(std::vector<int>{1, 2, 3});
    itflee::JSON v6(std::map<std::string, int>{{"key1", 1}, {"key2", 2}});
    
    // 新增：读取还原为原生类型
    std::string s  = json["key"];    // "value"
    int         i  = json["key2"];   // 1
    double      d  = json["key3"];   // 1.1
    bool        b  = json["key4"];   // true
    std::vector<int> vi = json["key5"]; // {1,2,3}
    std::map<std::string,int> mp = json["key6"]; // {"key1":1, "key2":2}

    // 新增：嵌套对象与链式创建
    json["user"]["name"] = "Tom";
    json["user"]["age"]  = 18;
    json["scores"] = std::vector<int>{95, 88, 76};
*/

#ifdef JSON_DEBUG_LOG 
#define JSON_DEBUG_LOG_PRINT(log) std::cout << log << std::endl;
#else
#define JSON_DEBUG_LOG_PRINT(log)
#endif

namespace itflee {
    
    class ITFLEEEXPORT JSON {
    public:
        // 定义支持的数据类型
        using value_type = std::variant<
            std::monostate,
            bool,
            int,
            int64_t,
            uint64_t,
            float,
            double,
            const char*,
            std::string,
            std::shared_ptr<std::vector<JSON>>,
            std::shared_ptr<std::map<std::string, JSON>>
        >;

        using array_type = std::vector<JSON>;
        using object_type = std::map<std::string, JSON>;

    public: // 构造与析构
        JSON();
        JSON(std::nullptr_t);
        JSON(bool v);
        JSON(int v);
        JSON(int64_t v);
        JSON(uint64_t v);
        JSON(float v);
        JSON(double v);
        JSON(const char* s);
        JSON(const std::string& s);
        JSON(const std::vector<JSON>& vec);
        JSON(const std::map<std::string, JSON>& mp);

        // 泛型数组/对象构造：例如 std::vector<int> / std::map<std::string,int>
        // 约束：T 必须能用于构造 JSON（即 JSON(T) 合法），从而覆盖 value_type 中所有基础类型
        template <typename T,
                  typename = std::enable_if_t<
                      !std::is_same_v<T, JSON> &&
                      std::is_constructible_v<JSON, T>
                  >>
        JSON(const std::vector<T>& vec)
            : value_(std::make_shared<array_type>()) {
            array_type& arr = *std::get<std::shared_ptr<array_type>>(value_);
            arr.reserve(vec.size());
            for (const auto& v : vec) {
                arr.emplace_back(JSON(v));
            }
        }

        template <typename T,
                  typename = std::enable_if_t<
                      !std::is_same_v<T, JSON> &&
                      std::is_constructible_v<JSON, T>
                  >>
        JSON(const std::map<std::string, T>& mp)
            : value_(std::make_shared<object_type>()) {
            object_type& obj = *std::get<std::shared_ptr<object_type>>(value_);
            for (const auto& kv : mp) {
                obj.emplace(kv.first, JSON(kv.second));
            }
        }

    public: // 拷贝/移动/析构
        JSON(const JSON& other);
        JSON(JSON&& other) noexcept;
        JSON& operator=(const JSON& other);
        JSON& operator=(JSON&& other) noexcept;
        ~JSON();

    public: // 重载的赋值运算符   JSON j = 10; ...
        JSON& operator=(std::nullptr_t);
        JSON& operator=(bool v);
        JSON& operator=(int v);
        JSON& operator=(int64_t v);
        JSON& operator=(uint64_t v);
        JSON& operator=(float v);
        JSON& operator=(double v);
        JSON& operator=(const char* s);
        JSON& operator=(const std::string& s);
        JSON& operator=(const std::vector<JSON>& vec);
        JSON& operator=(const std::map<std::string, JSON>& mp);

        // 支持直接赋值 std::vector<T> / std::map<std::string,T>
        template <typename T,
                  typename = std::enable_if_t<
                      !std::is_same_v<T, JSON> &&
                      std::is_constructible_v<JSON, T>
                  >>
        JSON& operator=(const std::vector<T>& vec) {
            reset();
            value_ = std::make_shared<array_type>();
            array_type& arr = *std::get<std::shared_ptr<array_type>>(value_);
            arr.reserve(vec.size());
            for (const auto& v : vec) {
                arr.emplace_back(JSON(v));
            }
            return *this;
        }

        template <typename T,
                  typename = std::enable_if_t<
                      !std::is_same_v<T, JSON> &&
                      std::is_constructible_v<JSON, T>
                  >>
        JSON& operator=(const std::map<std::string, T>& mp) {
            reset();
            value_ = std::make_shared<object_type>();
            object_type& obj = *std::get<std::shared_ptr<object_type>>(value_);
            for (const auto& kv : mp) {
                obj.emplace(kv.first, JSON(kv.second));
            }
            return *this;
        }
    
    public: // 隐式转换
        operator bool() const;
        operator int() const;
        operator int64_t() const;
        operator uint64_t() const;
        operator float() const;
        operator double() const;
        operator std::string() const;
        operator std::vector<JSON>() const;
        operator std::map<std::string, JSON>() const;
        // 便捷隐式转换为原生容器（数组）
        operator std::vector<bool>() const;
        operator std::vector<int>() const;
        operator std::vector<int64_t>() const;
        operator std::vector<uint64_t>() const;
        operator std::vector<float>() const;
        operator std::vector<double>() const;
        operator std::vector<std::string>() const;

        // 便捷隐式转换为原生容器（对象）
        operator std::map<std::string, bool>() const;
        operator std::map<std::string, int>() const;
        operator std::map<std::string, int64_t>() const;
        operator std::map<std::string, uint64_t>() const;
        operator std::map<std::string, float>() const;
        operator std::map<std::string, double>() const;
        operator std::map<std::string, std::string>() const;

    public: // [] 运算符重载
        // 对象访问/创建
        JSON& operator[](const std::string& key);
        JSON& operator[](const char* key);
        const JSON& operator[](const std::string& key) const;
        const JSON& operator[](const char* key) const;
        
        // 数组访问/创建
        JSON& operator[](int index);
        const JSON& operator[](int index) const;

        // 只读访问（找不到抛异常）
        // const JSON& at(const std::string& key) const;

    public: // 类型判断
        bool is_null() const;
        bool is_boolean() const;
        bool is_integer() const;
        bool is_int64() const;
        bool is_uint64() const;
        bool is_float() const;
        bool is_double() const;
        bool is_number() const;
        bool is_string() const;
        bool is_array() const;
        bool is_object() const;
        
        // 判断是否存在某个键（仅对对象类型有效）
        bool contains(const std::string& key) const;
        bool contains(const char* key) const;

    public: // 流输出
        // 使用自定义的序列化
        std::string serialize() const;
        std::string to_string() const;

		// 使用 RapidJSON 的序列化
		std::string serialize_compact_rj() const;  		// 紧凑版
		std::string serialize_pretty_rj(int indent = 2) const; // 美化版

        // 反序列化
        static JSON deserialize(const std::string& jsonText);

        // 流输出操作符
        friend ITFLEEEXPORT std::ostream& operator<<(std::ostream& os, const JSON& json);

    public: // 转换（读取为原生类型）
        template <typename T>
        T as() const
        {
            // 主模板：默认抛异常，所有支持的类型通过显式特化/重载提供
            throw std::runtime_error("JSON: value is not " + std::string(typeid(T).name()));
        }

        // 仅在内部性能敏感代码中使用：返回容器的 const 引用，避免不必要的拷贝
        const array_type& get_array_ref() const;
        const object_type& get_object_ref() const;

    private:
        void reset();
        void ensure_object();
        void deep_copy_from(const JSON& other);

    private:
#ifdef _MSC_VER  // 仅在 MSVC（Windows）下生效
    #pragma warning(push)
    #pragma warning(disable:4251)
#elif defined(__GNUC__) || defined(__clang__)  // 仅在 GCC/Clang（Linux）下生效
    #pragma GCC diagnostic push
    // #pragma GCC diagnostic ignored "-Wpedantic"  // 替换为实际需要抑制的警告
#endif
        value_type value_{};
#ifdef _MSC_VER
    #pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
    #pragma GCC diagnostic pop
#endif
    };

    // === 模板特化：为了兼容 MSVC + DLL 场景，将实现放在头文件中（内联） ===

    // 标量类型
    template <>
    inline bool JSON::as<bool>() const
    {
        if (std::holds_alternative<bool>(value_)) return std::get<bool>(value_);
        throw std::runtime_error("JSON: value is not bool");
    }

    template <>
    inline int JSON::as<int>() const
    {
        if (std::holds_alternative<int>(value_)) return std::get<int>(value_);
        throw std::runtime_error("JSON: value is not int");
    }

    template <>
    inline int64_t JSON::as<int64_t>() const
    {
        if (std::holds_alternative<int64_t>(value_)) return std::get<int64_t>(value_);
        if (std::holds_alternative<int>(value_))     return static_cast<int64_t>(std::get<int>(value_));
        throw std::runtime_error("JSON: value is not int64_t");
    }

    template <>
    inline uint64_t JSON::as<uint64_t>() const
    {
        if (std::holds_alternative<uint64_t>(value_)) return std::get<uint64_t>(value_);
        if (std::holds_alternative<int>(value_)) {
            int v = std::get<int>(value_);
            if (v < 0) throw std::runtime_error("JSON: cannot convert negative int to uint64_t");
            return static_cast<uint64_t>(v);
        }
        if (std::holds_alternative<int64_t>(value_)) {
            int64_t v = std::get<int64_t>(value_);
            if (v < 0) throw std::runtime_error("JSON: cannot convert negative int64_t to uint64_t");
            return static_cast<uint64_t>(v);
        }
        throw std::runtime_error("JSON: value is not uint64_t");
    }

    template <>
    inline float JSON::as<float>() const
    {
        if (std::holds_alternative<float>(value_))  return std::get<float>(value_);
        if (std::holds_alternative<double>(value_)) return static_cast<float>(std::get<double>(value_));
        if (std::holds_alternative<int>(value_))    return static_cast<float>(std::get<int>(value_));
        throw std::runtime_error("JSON: value is not float");
    }

    template <>
    inline double JSON::as<double>() const
    {
        if (std::holds_alternative<double>(value_))  return std::get<double>(value_);
        if (std::holds_alternative<float>(value_))   return static_cast<double>(std::get<float>(value_));
        if (std::holds_alternative<int>(value_))     return static_cast<double>(std::get<int>(value_));
        if (std::holds_alternative<int64_t>(value_)) return static_cast<double>(std::get<int64_t>(value_));
        if (std::holds_alternative<uint64_t>(value_))return static_cast<double>(std::get<uint64_t>(value_));
        throw std::runtime_error("JSON: value is not number");
    }

    template <>
    inline std::string JSON::as<std::string>() const
    {
        if (std::holds_alternative<std::string>(value_)) return std::get<std::string>(value_);
        throw std::runtime_error("JSON: value is not string");
    }

    // JSON 容器
    template <>
    inline std::vector<JSON> JSON::as<std::vector<JSON>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<array_type>>(value_)) {
            throw std::runtime_error("JSON: value is not array");
        }
        const auto& arr = std::get<std::shared_ptr<array_type>>(value_);
        std::vector<JSON> out;
        out.reserve(arr->size());
        for (const auto& item : *arr) {
            out.emplace_back(item);  // 调用 JSON(const JSON& other)
        }
        return out;
    }

    template <>
    inline std::map<std::string, JSON> JSON::as<std::map<std::string, JSON>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<object_type>>(value_)) {
            throw std::runtime_error("JSON: value is not object");
        }
        const auto& obj = std::get<std::shared_ptr<object_type>>(value_);
        std::map<std::string, JSON> out;
        for (const auto& kv : *obj) {
            out.emplace(kv.first, kv.second);  // 调用 JSON(const JSON& other)
        }
        return out;
    }

    // 数组：支持 value_type 中所有标量类型
    template <>
    inline std::vector<bool> JSON::as<std::vector<bool>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<array_type>>(value_)) {
            throw std::runtime_error("JSON: value is not array");
        }
        const auto& arr = std::get<std::shared_ptr<array_type>>(value_);
        std::vector<bool> out;
        out.reserve(arr->size());
        for (const auto& item : *arr) {
            out.emplace_back(item.as<bool>());
        }
        return out;
    }

    template <>
    inline std::vector<int> JSON::as<std::vector<int>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<array_type>>(value_)) {
            throw std::runtime_error("JSON: value is not array");
        }
        const auto& arr = std::get<std::shared_ptr<array_type>>(value_);
        std::vector<int> out;
        out.reserve(arr->size());
        for (const auto& item : *arr) {
            out.emplace_back(item.as<int>());
        }
        return out;
    }

    template <>
    inline std::vector<int64_t> JSON::as<std::vector<int64_t>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<array_type>>(value_)) {
            throw std::runtime_error("JSON: value is not array");
        }
        const auto& arr = std::get<std::shared_ptr<array_type>>(value_);
        std::vector<int64_t> out;
        out.reserve(arr->size());
        for (const auto& item : *arr) {
            out.emplace_back(item.as<int64_t>());
        }
        return out;
    }

    template <>
    inline std::vector<uint64_t> JSON::as<std::vector<uint64_t>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<array_type>>(value_)) {
            throw std::runtime_error("JSON: value is not array");
        }
        const auto& arr = std::get<std::shared_ptr<array_type>>(value_);
        std::vector<uint64_t> out;
        out.reserve(arr->size());
        for (const auto& item : *arr) {
            out.emplace_back(item.as<uint64_t>());
        }
        return out;
    }

    template <>
    inline std::vector<float> JSON::as<std::vector<float>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<array_type>>(value_)) {
            throw std::runtime_error("JSON: value is not array");
        }
        const auto& arr = std::get<std::shared_ptr<array_type>>(value_);
        std::vector<float> out;
        out.reserve(arr->size());
        for (const auto& item : *arr) {
            out.emplace_back(item.as<float>());
        }
        return out;
    }

    template <>
    inline std::vector<double> JSON::as<std::vector<double>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<array_type>>(value_)) {
            throw std::runtime_error("JSON: value is not array");
        }
        const auto& arr = std::get<std::shared_ptr<array_type>>(value_);
        std::vector<double> out;
        out.reserve(arr->size());
        for (const auto& item : *arr) {
            out.emplace_back(item.as<double>());
        }
        return out;
    }

    template <>
    inline std::vector<std::string> JSON::as<std::vector<std::string>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<array_type>>(value_)) {
            throw std::runtime_error("JSON: value is not array");
        }
        const auto& arr = std::get<std::shared_ptr<array_type>>(value_);
        std::vector<std::string> out;
        out.reserve(arr->size());
        for (const auto& item : *arr) {
            out.emplace_back(item.as<std::string>());
        }
        return out;
    }

    // 对象：支持 value_type 中所有标量类型
    template <>
    inline std::map<std::string, bool> JSON::as<std::map<std::string, bool>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<object_type>>(value_)) {
            throw std::runtime_error("JSON: value is not object");
        }
        const auto& obj = std::get<std::shared_ptr<object_type>>(value_);
        std::map<std::string, bool> out;
        for (const auto& kv : *obj) {
            out.emplace(kv.first, kv.second.as<bool>());
        }
        return out;
    }

    template <>
    inline std::map<std::string, int> JSON::as<std::map<std::string, int>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<object_type>>(value_)) {
            throw std::runtime_error("JSON: value is not object");
        }
        const auto& obj = std::get<std::shared_ptr<object_type>>(value_);
        std::map<std::string, int> out;
        for (const auto& kv : *obj) {
            out.emplace(kv.first, kv.second.as<int>());
        }
        return out;
    }

    template <>
    inline std::map<std::string, int64_t> JSON::as<std::map<std::string, int64_t>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<object_type>>(value_)) {
            throw std::runtime_error("JSON: value is not object");
        }
        const auto& obj = std::get<std::shared_ptr<object_type>>(value_);
        std::map<std::string, int64_t> out;
        for (const auto& kv : *obj) {
            out.emplace(kv.first, kv.second.as<int64_t>());
        }
        return out;
    }

    template <>
    inline std::map<std::string, uint64_t> JSON::as<std::map<std::string, uint64_t>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<object_type>>(value_)) {
            throw std::runtime_error("JSON: value is not object");
        }
        const auto& obj = std::get<std::shared_ptr<object_type>>(value_);
        std::map<std::string, uint64_t> out;
        for (const auto& kv : *obj) {
            out.emplace(kv.first, kv.second.as<uint64_t>());
        }
        return out;
    }

    template <>
    inline std::map<std::string, float> JSON::as<std::map<std::string, float>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<object_type>>(value_)) {
            throw std::runtime_error("JSON: value is not object");
        }
        const auto& obj = std::get<std::shared_ptr<object_type>>(value_);
        std::map<std::string, float> out;
        for (const auto& kv : *obj) {
            out.emplace(kv.first, kv.second.as<float>());
        }
        return out;
    }

    template <>
    inline std::map<std::string, double> JSON::as<std::map<std::string, double>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<object_type>>(value_)) {
            throw std::runtime_error("JSON: value is not object");
        }
        const auto& obj = std::get<std::shared_ptr<object_type>>(value_);
        std::map<std::string, double> out;
        for (const auto& kv : *obj) {
            out.emplace(kv.first, kv.second.as<double>());
        }
        return out;
    }

    template <>
    inline std::map<std::string, std::string> JSON::as<std::map<std::string, std::string>>() const
    {
        if (!std::holds_alternative<std::shared_ptr<object_type>>(value_)) {
            throw std::runtime_error("JSON: value is not object");
        }
        const auto& obj = std::get<std::shared_ptr<object_type>>(value_);
        std::map<std::string, std::string> out;
        for (const auto& kv : *obj) {
            out.emplace(kv.first, kv.second.as<std::string>());
        }
        return out;
    }
}

#endif // JSON_HPP