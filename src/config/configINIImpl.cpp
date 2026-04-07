#include "configINIImpl.h"
#include <algorithm>
#include <cctype>

namespace itflee
{
    ConfigINIImpl::ConfigINIImpl()
        : ini_(std::make_unique<CSimpleIniA>())
    {
        // 设置选项：不区分大小写
        ini_->SetUnicode(false);
    }

    bool ConfigINIImpl::LoadFile(const std::string& file_path)
    {
        if (file_path.empty()) {
            return false;
        }

        SI_Error rc = ini_->LoadFile(file_path.c_str());
        if (rc == SI_OK) {
            file_path_ = file_path;
            return true;
        }
        return false;
    }

    bool ConfigINIImpl::SaveFile(const std::string& file_path)
    {
        std::string save_path = file_path.empty() ? file_path_ : file_path;
        if (save_path.empty()) {
            return false;
        }

        SI_Error rc = ini_->SaveFile(save_path.c_str());
        return rc == SI_OK;
    }

    std::string ConfigINIImpl::GetString(const std::string& section, const std::string& key, const std::string& default_value)
    {
        const char* value = ini_->GetValue(section.c_str(), key.c_str(), default_value.c_str());
        return value ? std::string(value) : default_value;
    }

    int ConfigINIImpl::GetInt(const std::string& section, const std::string& key, int default_value)
    {
        return static_cast<int>(ini_->GetLongValue(section.c_str(), key.c_str(), default_value));
    }

    long ConfigINIImpl::GetLong(const std::string& section, const std::string& key, long default_value)
    {
        return ini_->GetLongValue(section.c_str(), key.c_str(), default_value);
    }

    double ConfigINIImpl::GetDouble(const std::string& section, const std::string& key, double default_value)
    {
        return ini_->GetDoubleValue(section.c_str(), key.c_str(), default_value);
    }

    bool ConfigINIImpl::GetBool(const std::string& section, const std::string& key, bool default_value)
    {
        return ini_->GetBoolValue(section.c_str(), key.c_str(), default_value);
    }

    bool ConfigINIImpl::SetString(const std::string& section, const std::string& key, const std::string& value)
    {
        SI_Error rc = ini_->SetValue(section.c_str(), key.c_str(), value.c_str());
        return rc == SI_OK;
    }

    bool ConfigINIImpl::SetInt(const std::string& section, const std::string& key, int value)
    {
        SI_Error rc = ini_->SetLongValue(section.c_str(), key.c_str(), value);
        return rc == SI_OK;
    }

    bool ConfigINIImpl::SetDouble(const std::string& section, const std::string& key, double value)
    {
        SI_Error rc = ini_->SetDoubleValue(section.c_str(), key.c_str(), value);
        return rc == SI_OK;
    }

    bool ConfigINIImpl::SetBool(const std::string& section, const std::string& key, bool value)
    {
        SI_Error rc = ini_->SetBoolValue(section.c_str(), key.c_str(), value);
        return rc == SI_OK;
    }

    bool ConfigINIImpl::HasKey(const std::string& section, const std::string& key)
    {
        return ini_->GetValue(section.c_str(), key.c_str()) != nullptr;
    }

    std::vector<std::string> ConfigINIImpl::GetAllSections()
    {
        std::vector<std::string> sections;
        CSimpleIniA::TNamesDepend section_names;
        ini_->GetAllSections(section_names);
        
        for (const auto& name : section_names) {
            sections.push_back(std::string(name.pItem));
        }
        
        return sections;
    }

    std::vector<std::string> ConfigINIImpl::GetAllKeys(const std::string& section)
    {
        std::vector<std::string> keys;
        CSimpleIniA::TNamesDepend key_names;
        ini_->GetAllKeys(section.c_str(), key_names);
        
        for (const auto& name : key_names) {
            keys.push_back(std::string(name.pItem));
        }
        
        return keys;
    }

    bool ConfigINIImpl::DeleteKey(const std::string& section, const std::string& key)
    {
        return ini_->Delete(section.c_str(), key.c_str());
    }

    bool ConfigINIImpl::DeleteSection(const std::string& section)
    {
        return ini_->Delete(section.c_str(), nullptr);
    }

    // 静态方法实现
    std::shared_ptr<ConfigINI> ConfigINI::Create(const std::string& file_path)
    {
        auto impl = std::make_shared<ConfigINIImpl>();
        if (!file_path.empty()) {
            if (!impl->LoadFile(file_path)) {
                return nullptr;
            }
        }
        return impl;
    }
}

