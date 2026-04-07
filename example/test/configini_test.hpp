#pragma once

#include <iostream>
#include <vector>
#include <string>
#include "config/configINI.h"
#include "base/json/json_helper.h"

namespace itflee {
    namespace test {
        void configini_test()
        {
            std::cout << "\n======================================== ConfigINI 测试 ========================================\n";

            // 测试文件路径（尝试多个可能的路径）
            std::vector<std::string> possible_paths = {
                "src/config/Config.ini",
                "../src/config/Config.ini",
                "../../../src/config/Config.ini",
                "Config.ini"
            };

            std::string config_file;
            std::shared_ptr<itflee::ConfigINI> config;

            // 创建配置对象并加载文件
            std::cout << "\n[1] 加载配置文件" << std::endl;
            for (const auto& path : possible_paths) {
                std::cout << "  尝试路径: " << path << std::endl;
                config = itflee::ConfigINI::Create(path);
                if (config) {
                    config_file = path;
                    std::cout << "   配置文件加载成功！路径: " << config_file << std::endl;
                    break;
                }
            }

            if (!config) {
                std::cout << "   加载配置文件失败！请确保配置文件存在。" << std::endl;
                std::cout << "  尝试的路径：" << std::endl;
                for (const auto& path : possible_paths) {
                    std::cout << "    - " << path << std::endl;
                }
                return;
            }

            // 测试读取字符串值
            std::cout << "\n[2] 测试读取字符串值" << std::endl;
            {
                std::string ip = config->GetString("Common", "DetectProcessIp", "127.0.0.1");
                std::cout << "  Common.DetectProcessIp = " << ip << std::endl;

                std::string yaml = config->GetString("Detect", "irDetectModelYaml", "");
                std::cout << "  Detect.irDetectModelYaml = " << yaml << std::endl;

                std::string not_exist = config->GetString("Common", "NotExistKey", "default_value");
                std::cout << "  Common.NotExistKey (默认值) = " << not_exist << std::endl;
            }

            // 测试读取整数值
            std::cout << "\n[3] 测试读取整数值" << std::endl;
            {
                int port = config->GetInt("Common", "DetectProcessPort", 8080);
                std::cout << "  Common.DetectProcessPort = " << port << std::endl;

                int mode = config->GetInt("Detect", "detectMode", 0);
                std::cout << "  Detect.detectMode = " << mode << std::endl;

                int height = config->GetInt("HoloCamera", "HoloCameraHeight", 0);
                std::cout << "  HoloCamera.HoloCameraHeight = " << height << std::endl;
            }

            // 测试读取长整数值
            std::cout << "\n[4] 测试读取长整数值" << std::endl;
            {
                long port = config->GetLong("Common", "YiminServerListenPort", 0);
                std::cout << "  Common.YiminServerListenPort = " << port << std::endl;

                long threshold = config->GetLong("HoloCamera", "HoloMissThreshold", 0);
                std::cout << "  HoloCamera.HoloMissThreshold = " << threshold << std::endl;
            }

            // 测试读取浮点数值
            std::cout << "\n[5] 测试读取浮点数值" << std::endl;
            {
                double longitude = config->GetDouble("Temp", "longitude", 0.0);
                std::cout << "  Temp.longitude = " << longitude << std::endl;

                double latitude = config->GetDouble("Temp", "latitude", 0.0);
                std::cout << "  Temp.latitude = " << latitude << std::endl;

                double scale_w = config->GetDouble("Detect", "scale_w", 1.0);
                std::cout << "  Detect.scale_w = " << scale_w << std::endl;
            }

            // 测试读取布尔值
            std::cout << "\n[6] 测试读取布尔值" << std::endl;
            {
                bool isIrNight = config->GetBool("Detect", "isIrnight", false);
                std::cout << "  Detect.isIrnight = " << (isIrNight ? "true" : "false") << std::endl;

                bool isFusion = config->GetBool("Fusion", "RTCPU_GPU", false);
                std::cout << "  Fusion.RTCPU_GPU = " << (isFusion ? "true" : "false") << std::endl;

                bool isSave = config->GetBool("Result", "isSaveResult", false);
                std::cout << "  Result.isSaveResult = " << (isSave ? "true" : "false") << std::endl;
            }

            // 测试获取所有节
            std::cout << "\n[7] 测试获取所有节" << std::endl;
            {
                auto sections = config->GetAllSections();
                std::cout << "  所有节 (" << sections.size() << " 个):" << std::endl;
                for (const auto& section : sections) {
                    std::cout << "    - [" << section << "]" << std::endl;
                }
            }

            // 测试获取指定节下的所有键
            std::cout << "\n[8] 测试获取指定节下的所有键" << std::endl;
            {
                auto keys = config->GetAllKeys("Common");
                std::cout << "  [Common] 节下的所有键 (" << keys.size() << " 个):" << std::endl;
                for (const auto& key : keys) {
                    std::string value = config->GetString("Common", key, "");
                    std::cout << "    - " << key << " = " << value << std::endl;
                }
            }

            // 测试检查键是否存在
            std::cout << "\n[9] 测试检查键是否存在" << std::endl;
            {
                bool exists1 = config->HasKey("Common", "DetectProcessIp");
                std::cout << "  Common.DetectProcessIp 存在: " << (exists1 ? "是" : "否") << std::endl;

                bool exists2 = config->HasKey("Common", "NotExistKey");
                std::cout << "  Common.NotExistKey 存在: " << (exists2 ? "是" : "否") << std::endl;
            }

            // 测试设置值
            std::cout << "\n[10] 测试设置值" << std::endl;
            {
                // 设置字符串值
                bool set1 = config->SetString("Test", "TestString", "Hello World");
                std::cout << "  设置 Test.TestString = \"Hello World\": " << (set1 ? "成功" : "失败") << std::endl;
                std::cout << "  读取 Test.TestString = " << config->GetString("Test", "TestString", "") << std::endl;

                // 设置整数值
                bool set2 = config->SetInt("Test", "TestInt", 12345);
                std::cout << "  设置 Test.TestInt = 12345: " << (set2 ? "成功" : "失败") << std::endl;
                std::cout << "  读取 Test.TestInt = " << config->GetInt("Test", "TestInt", 0) << std::endl;

                // 设置浮点数值
                bool set3 = config->SetDouble("Test", "TestDouble", 3.14159);
                std::cout << "  设置 Test.TestDouble = 3.14159: " << (set3 ? "成功" : "失败") << std::endl;
                std::cout << "  读取 Test.TestDouble = " << config->GetDouble("Test", "TestDouble", 0.0) << std::endl;

                // 设置布尔值
                bool set4 = config->SetBool("Test", "TestBool", true);
                std::cout << "  设置 Test.TestBool = true: " << (set4 ? "成功" : "失败") << std::endl;
                std::cout << "  读取 Test.TestBool = " << (config->GetBool("Test", "TestBool", false) ? "true" : "false") << std::endl;
            }

            // 测试修改现有值
            std::cout << "\n[11] 测试修改现有值" << std::endl;
            {
                std::string old_value = config->GetString("Common", "DetectProcessIp", "");
                std::cout << "  修改前 Common.DetectProcessIp = " << old_value << std::endl;

                config->SetString("Common", "DetectProcessIp", "192.168.1.100");
                std::string new_value = config->GetString("Common", "DetectProcessIp", "");
                std::cout << "  修改后 Common.DetectProcessIp = " << new_value << std::endl;

                // 恢复原值
                config->SetString("Common", "DetectProcessIp", old_value);
                std::cout << "  恢复原值 Common.DetectProcessIp = " << config->GetString("Common", "DetectProcessIp", "") << std::endl;
            }

            // 测试删除键
            std::cout << "\n[12] 测试删除键" << std::endl;
            {
                // 先设置一个测试键
                config->SetString("Test", "ToDeleteKey", "ToDeleteValue");
                std::cout << "  设置 Test.ToDeleteKey = \"ToDeleteValue\"" << std::endl;
                std::cout << "  删除前存在: " << (config->HasKey("Test", "ToDeleteKey") ? "是" : "否") << std::endl;

                bool deleted = config->DeleteKey("Test", "ToDeleteKey");
                std::cout << "  删除 Test.ToDeleteKey: " << (deleted ? "成功" : "失败") << std::endl;
                std::cout << "  删除后存在: " << (config->HasKey("Test", "ToDeleteKey") ? "是" : "否") << std::endl;
            }

            // 测试删除节
            std::cout << "\n[13] 测试删除节" << std::endl;
            {
                // 先创建一个测试节
                config->SetString("TestSection", "Key1", "Value1");
                config->SetString("TestSection", "Key2", "Value2");
                std::cout << "  创建测试节 [TestSection] 并添加两个键" << std::endl;

                auto sections_before = config->GetAllSections();
                bool has_section = false;
                for (const auto& s : sections_before) {
                    if (s == "TestSection") {
                        has_section = true;
                        break;
                    }
                }
                std::cout << "  删除前 [TestSection] 存在: " << (has_section ? "是" : "否") << std::endl;

                bool deleted = config->DeleteSection("TestSection");
                std::cout << "  删除 [TestSection]: " << (deleted ? "成功" : "失败") << std::endl;

                auto sections_after = config->GetAllSections();
                has_section = false;
                for (const auto& s : sections_after) {
                    if (s == "TestSection") {
                        has_section = true;
                        break;
                    }
                }
                std::cout << "  删除后 [TestSection] 存在: " << (has_section ? "是" : "否") << std::endl;
            }

            // 测试保存文件（可选，注释掉以避免修改原文件）
            std::cout << "\n[14] 测试保存文件（跳过，避免修改原配置文件）" << std::endl;
            std::cout << "  如需测试保存功能，可以取消注释以下代码：" << std::endl;
            std::cout << "  // std::string test_file = \"test_config.ini\";" << std::endl;
            std::cout << "  // config->SaveFile(test_file);" << std::endl;
            /*
            std::string test_file = "test_config.ini";
            bool saved = config->SaveFile(test_file);
            std::cout << "  保存到 " << test_file << ": " << (saved ? "成功" : "失败") << std::endl;
            */

            std::cout << "\n======================================== ConfigINI 测试完成 ========================================\n\n";



            itflee::JSON json;
            json["isSaveResult"] = config->GetBool("Result", "isSaveResult", false);
            json["lightRawData"] = config->GetString("Result", "lightRawData", "");

            if(json["isSaveResult"])
				std::cout << "lightRawData: " << json["lightRawData"] << std::endl;
        }
    }
}

