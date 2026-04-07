#ifndef CONFIG_INI_IMPL_H
#define CONFIG_INI_IMPL_H

#include "base/config/configINI.h"
#include "simpleini/SimpleIni.h"
#include <string>
#include <memory>

namespace itflee
{
    class ConfigINIImpl : public ConfigINI
    {
    public:
        ConfigINIImpl();
        ~ConfigINIImpl() override = default;

    public:
        bool LoadFile(const std::string& file_path) override;
        bool SaveFile(const std::string& file_path = "") override;

        std::string GetString(const std::string& section, const std::string& key, const std::string& default_value = "") override;
        int GetInt(const std::string& section, const std::string& key, int default_value = 0) override;
        long GetLong(const std::string& section, const std::string& key, long default_value = 0) override;
        double GetDouble(const std::string& section, const std::string& key, double default_value = 0.0) override;
        bool GetBool(const std::string& section, const std::string& key, bool default_value = false) override;

        bool SetString(const std::string& section, const std::string& key, const std::string& value) override;
        bool SetInt(const std::string& section, const std::string& key, int value) override;
        bool SetDouble(const std::string& section, const std::string& key, double value) override;
        bool SetBool(const std::string& section, const std::string& key, bool value) override;

        bool HasKey(const std::string& section, const std::string& key) override;
        std::vector<std::string> GetAllSections() override;
        std::vector<std::string> GetAllKeys(const std::string& section) override;
        bool DeleteKey(const std::string& section, const std::string& key) override;
        bool DeleteSection(const std::string& section) override;

    private:
        std::unique_ptr<CSimpleIniA> ini_;
        std::string file_path_;
    };
}

#endif // CONFIG_INI_IMPL_H

