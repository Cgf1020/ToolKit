#ifndef CONFIG_INI_H
#define CONFIG_INI_H

#include <string>
#include <vector>
#include "define.h"

namespace itflee
{
    class ITFLEEEXPORT ConfigINI
    {
    public:   
        /**
         * @brief 创建实例
         * @param file_path 配置文件路径（可选）
         * @return ConfigINI 实例指针
         */
        static std::shared_ptr<ConfigINI> Create(const std::string& file_path = "");

        virtual ~ConfigINI() = default;

        /**
         * @brief 加载配置文件
         * @param file_path 配置文件路径
         * @return true: 成功  false: 失败
         */
        virtual bool LoadFile(const std::string& file_path) = 0;

        /**
         * @brief 保存配置文件
         * @param file_path 配置文件路径（为空则保存到加载的文件）
         * @return true: 成功  false: 失败
         */
        virtual bool SaveFile(const std::string& file_path = "") = 0;

        /**
         * @brief 获取字符串值
         * @param section 节名
         * @param key 键名
         * @param default_value 默认值
         * @return 配置值或默认值
         */
        virtual std::string GetString(const std::string& section, const std::string& key, const std::string& default_value = "") = 0;

        /**
         * @brief 获取整数值
         * @param section 节名
         * @param key 键名
         * @param default_value 默认值
         * @return 配置值或默认值
         */
        virtual int GetInt(const std::string& section, const std::string& key, int default_value = 0) = 0;

        /**
         * @brief 获取长整数值
         * @param section 节名
         * @param key 键名
         * @param default_value 默认值
         * @return 配置值或默认值
         */
        virtual long GetLong(const std::string& section, const std::string& key, long default_value = 0) = 0;

        /**
         * @brief 获取双精度浮点数值
         * @param section 节名
         * @param key 键名
         * @param default_value 默认值
         * @return 配置值或默认值
         */
        virtual double GetDouble(const std::string& section, const std::string& key, double default_value = 0.0) = 0;

        /**
         * @brief 获取布尔值
         * @param section 节名
         * @param key 键名
         * @param default_value 默认值
         * @return 配置值或默认值
         */
        virtual bool GetBool(const std::string& section, const std::string& key, bool default_value = false) = 0;

        /**
         * @brief 设置字符串值
         * @param section 节名
         * @param key 键名
         * @param value 值
         * @return true: 成功  false: 失败
         */
        virtual bool SetString(const std::string& section, const std::string& key, const std::string& value) = 0;

        /**
         * @brief 设置整数值
         * @param section 节名
         * @param key 键名
         * @param value 值
         * @return true: 成功  false: 失败
         */
        virtual bool SetInt(const std::string& section, const std::string& key, int value) = 0;

        /**
         * @brief 设置双精度浮点数值
         * @param section 节名
         * @param key 键名
         * @param value 值
         * @return true: 成功  false: 失败
         */
        virtual bool SetDouble(const std::string& section, const std::string& key, double value) = 0;

        /**
         * @brief 设置布尔值
         * @param section 节名
         * @param key 键名
         * @param value 值
         * @return true: 成功  false: 失败
         */
        virtual bool SetBool(const std::string& section, const std::string& key, bool value) = 0;

        /**
         * @brief 检查键是否存在
         * @param section 节名
         * @param key 键名
         * @return true: 存在  false: 不存在
         */
        virtual bool HasKey(const std::string& section, const std::string& key) = 0;

        /**
         * @brief 获取所有节名
         * @return 节名列表
         */
        virtual std::vector<std::string> GetAllSections() = 0;

        /**
         * @brief 获取指定节下的所有键名
         * @param section 节名
         * @return 键名列表
         */
        virtual std::vector<std::string> GetAllKeys(const std::string& section) = 0;

        /**
         * @brief 删除键
         * @param section 节名
         * @param key 键名
         * @return true: 成功  false: 失败
         */
        virtual bool DeleteKey(const std::string& section, const std::string& key) = 0;

        /**
         * @brief 删除节
         * @param section 节名
         * @return true: 成功  false: 失败
         */
        virtual bool DeleteSection(const std::string& section) = 0;
    };
}

#endif // CONFIG_INI_H