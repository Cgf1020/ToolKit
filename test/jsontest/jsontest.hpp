#pragma once	

#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include "base/json/json_helper.h"
#include "base/time/timing.hpp"


[[maybe_unused]] static void jsonTestBug(const itflee::JSON& root)
{
	if(root.contains("key"))
	{
		std::string value = root["key"];
		std::cout << "value: " << value << std::endl;
	}
	else
	{
		std::cout << "key not found" << std::endl;
	}
}



[[maybe_unused]] static void jsonTestBase()
{
	itflee::Timing timer;
	timer.start();
	itflee::JSON root;

	root["key"] = "value";
	root["key2"] = 1;
	root["key3"] = 1.1;
	root["key4"] = true;
	root["key5"] = std::vector<int>{1, 2, 3};
	root["key6"] = std::map<std::string, int>{{"key1", 1}, {"key2", 2}};
	std::string json_str = root.serialize_pretty_rj();

	timer.stop();
	std::cout << "root: " << json_str << std::endl;
	std::cout << "time: " << timer.getElapsedMilliseconds() << "ms" << std::endl;

	if (root.contains("key4"))
	{

		std::cout << "root.contains  key4: " << root["key4"].as<bool>() << std::endl;
	}


	std::string val1 = root["key"].as<std::string>();


}

[[maybe_unused]] static void jsonTestCpuAndMemory()
{
	std::atomic<bool> running{ true };

	itflee::Timing timer;
	timer.start();

	std::thread worker([&]() {
		std::size_t i = 0;
		while (running)
		{
			// 构造一个较复杂的 JSON 结构，反复创建/销毁，用于观察 CPU 和内存占用
			itflee::JSON root;
			root["index"] = static_cast<int64_t>(i);
			root["name"]  = "Tom";

			std::vector<int> vec{ 1,2,3,4,5 };
			root["vec"] = vec;

			itflee::JSON nested;
			nested["v"]    = static_cast<int64_t>(i);
			nested["flag"] = (i % 2 == 0);

			std::vector<itflee::JSON> arr;
			arr.push_back(nested);
			arr.push_back(nested);
			arr.push_back(nested);
			root["nested_array"] = arr;

			// 序列化，触发内部遍历与字符串拼接
			std::string s = root.serialize_compact_rj();
			(void)s;

			// 周期性打印迭代次数，方便观察运行状态
			if (++i % 100000 == 0)
			{
				std::cout << "[jsonTestCpuAndMemory] iterations: " << i << std::endl;
			}
		}
	});

	// 主线程控制运行时长，例如 30 秒，方便外部通过任务管理器/PerfMon 观察 CPU 和内存
	while (timer.getElapsedSeconds() < 30.0)
	{
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::cout << "[jsonTestCpuAndMemory] elapsed: "
			      << timer.getElapsedSeconds() << " s" << std::endl;
	}

	running = false;
	if (worker.joinable())
	{
		worker.join();
	}
}


[[maybe_unused]] static void jsonTestAll()
{
	std::cout << "========== JSON 全量用例 ==========" << std::endl;

	// 1. 基础类型构造与访问
	{
		std::cout << "\n[1] 基础类型构造与访问" << std::endl;

		itflee::JSON j_null;
		itflee::JSON j_bool(true);
		itflee::JSON j_int(42);
		itflee::JSON j_double(3.14);
		itflee::JSON j_str("hello");

		std::cout << "null is_null=" << j_null.is_null() << std::endl;
		std::cout << "bool  = " << j_bool.as<bool>() << std::endl;
		std::cout << "int   = " << j_int.as<int>() << std::endl;
		std::cout << "double= " << j_double.as<double>() << std::endl;
		std::cout << "str   = " << j_str.as<std::string>() << std::endl;
	}

	// 2. 对象：operator[] 写入 / 读取
	{
		std::cout << "\n[2] 对象写入 / 读取" << std::endl;
		itflee::JSON obj;
		obj["name"] = "Tom";
		obj["age"]  = 18;
		obj["score"] = 95.5;

		if (obj.contains("name")) {
			std::string name = obj["name"];
			std::cout << "name = " << name << std::endl;
		}
		if (obj.contains("age")) {
			int age = obj["age"];
			std::cout << "age  = " << age << std::endl;
		}
		std::cout << "obj serialize: " << obj.serialize_pretty_rj() << std::endl;
	}

	// 3. 数组：vector<int> / vector<string> 与 JSON 互转
	{
		std::cout << "\n[3] 数组：vector<int> / vector<string> 与 JSON 互转" << std::endl;

		std::vector<int> vi{1,2,3,4,5};
		std::vector<std::string> vs{"hello","world","json"};

		itflee::JSON j;
		j["vi"] = vi;
		j["vs"] = vs;

		std::vector<int> vi2 = j["vi"];
		std::vector<std::string> vs2 = j["vs"];

		std::cout << "vi2: ";
		for (auto v : vi2) std::cout << v << " ";
		std::cout << "\nvs2: ";
		for (auto& s : vs2) std::cout << s << " ";
		std::cout << std::endl;
	}

	// 4. map<string, JSON> 与 JSON 互转
	{
		std::cout << "\n[4] 对象：map<string, JSON> 与 JSON 互转" << std::endl;
		std::map<std::string, itflee::JSON> mp{
			{"k1", 1},
			{"k2", "v2"},
			{"k3", true}
		};
		itflee::JSON j(mp);      // JSON(const std::map<std::string, JSON>&)
		std::map<std::string, itflee::JSON> mp2 = j; // operator std::map<std::string, JSON>() const

		for (const auto& kv : mp2) {
			std::cout << kv.first << " => " << kv.second.to_string() << std::endl;
		}
	}

	// 5. 拷贝 / 移动 构造与赋值
	{
		std::cout << "\n[5] 拷贝 / 移动 构造与赋值" << std::endl;

		itflee::JSON src;
		src["a"] = 1;
		src["b"] = "str";

		itflee::JSON copy = src;      // 拷贝构造
		itflee::JSON assigned;
		assigned = src;               // 拷贝赋值

		itflee::JSON moved = std::move(src); // 移动构造
		itflee::JSON moved2;
		moved2 = std::move(copy);           // 移动赋值

		std::cout << "assigned: " << assigned.to_string() << std::endl;
		std::cout << "moved   : " << moved.to_string() << std::endl;
		std::cout << "moved2  : " << moved2.to_string() << std::endl;
	}

	// 6. 嵌套对象与数组 + 序列化 / 反序列化
	{
		std::cout << "\n[6] 嵌套对象 + 序列化 / 反序列化" << std::endl;
		itflee::JSON root;
		root["user"]["name"] = "Alice";
		root["user"]["age"]  = 20;

		itflee::JSON addr1;
		addr1["city"] = "Beijing";
		addr1["code"] = 100000;

		itflee::JSON addr2;
		addr2["city"] = "Shanghai";
		addr2["code"] = 200000;

		std::vector<itflee::JSON> addrs{addr1, addr2};
		root["addrs"] = addrs;

		std::string s = root.serialize_compact_rj();
		std::cout << "serialize_compact_rj: " << s << std::endl;

		itflee::JSON parsed = itflee::JSON::deserialize(s);
		std::cout << "parsed[\"user\"][\"name\"] = " << std::string(parsed["user"]["name"]) << std::endl;
	}

	// 7. 数值转换测试（int / int64 / uint64 / double / float）
	{
		std::cout << "\n[7] 数值转换测试" << std::endl;
		itflee::JSON j_num;
		j_num = 42;          // int
		std::cout << "as<int>    = " << j_num.as<int>() << std::endl;
		std::cout << "as<int64>  = " << j_num.as<int64_t>() << std::endl;
		std::cout << "as<uint64> = " << j_num.as<uint64_t>() << std::endl;
		std::cout << "as<double> = " << j_num.as<double>() << std::endl;

		j_num = 3.14159;
		std::cout << "double as<double> = " << j_num.as<double>() << std::endl;
		std::cout << "double as<float>  = " << j_num.as<float>()  << std::endl;
	}

	// 8. contains + operator[] 访问（单次查找示例）
	{
		std::cout << "\n[8] contains + operator[] 访问" << std::endl;
		itflee::JSON j;
		j["a"] = 1;
		j["b"] = 2;

		if (j.contains("a")) {
			int v = j["a"];
			std::cout << "a = " << v << std::endl;
		}
		else {
			std::cout << "a not found" << std::endl;
		}
	}

	std::cout << "\n========== JSON 全量用例结束 ==========" << std::endl;
}

