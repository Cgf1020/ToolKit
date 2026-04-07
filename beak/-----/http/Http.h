#pragma once

#include <functional>
#include <map>
#include <string>

namespace util::Http {
    void transfer(std::string const& method, 
        std::string const& url, 
        std::map<std::string, std::string> const& headers, 
        std::string const& body,
        std::function<void(std::string&& r)>&& postProcessor) noexcept;
    void transfer(std::string const& method, 
        std::string const& url, 
        std::map<std::string, std::string> const& headers, 
        std::string const& body,
        std::string const& fileName,
        std::function<void(std::string&& r)>&& postProcessor) noexcept;
    void transfer(std::string const& method, 
        std::string const& url, 
        std::map<std::string, std::string> const& headers, 
        std::string const& body, 
        std::string const& fileName,
        std::function<void(int&& r)>&& postProcessor) noexcept;
} // namespace util::Http
