#pragma once

#include <iostream>
#include <vector>
#include "base/json/json_helper.h"
//#include "ToolKit/timer/timing.hpp"



[[maybe_unused]] static void jsonTestBug(const itflee::JSON& root)
{
	if(root.contains("key"))
	{
		std::string value = root["key"].as<std::string>();
		std::cout << "value: " << value << std::endl;
	}
	else
	{
		std::cout << "key not found" << std::endl;
	}
}

[[maybe_unused]] static void jsonTestFunc()
{
	//1---------------------------------创建一个json对象
	itflee::JSON root;

	//2---------------------------------设置值
	{
		//2.1 设置基本类型值
		{	
			//root.setValue("xuanzhuan", "diajf");
			//root.setValue("afd", 123);
			//root.setValue("flag", true);
		}

		//2.2 设置数组(值为基本类型)
		{
			//std::vector<int> vec1 = { 10, 20, 50 };
			//root.setArray("vec1", vec1);
		}
		//2.3 设置数组(值为json对象)
		//{
		//	itflee::JSON vecjson1;	vecjson1.setValue("xuanzhuan", "diajf"); vecjson1.setValue("afd", 123);
		//	itflee::JSON vecjson2;	vecjson2.setValue("xuanzhuan", "diajf"); vecjson2.setValue("afd", 123);
		//	itflee::JSON vecjson3;	vecjson3.setValue("xuanzhuan", "diajf"); vecjson3.setValue("afd", 123);
		//	std::vector<itflee::JSON> vec2 = { vecjson1, vecjson2, vecjson3 };
		//	root.setArray("vec2", vec2);
		//}
		//2.4 设置json对象
		{
			//itflee::JSON object;
			{
				//2.4.1 object1 设置基本类型
				//object.setValue("xuanzhuan", "diajf");
				//object.setValue("afd", 1323);
				//object.setValue("flag", false);

		//		//2.4.2 object1 设置json数组
		//		itflee::JSON vecjson1;	vecjson1.setValue("xuanzhuan", "12313"); vecjson1.setValue("afd", 123);
		//		itflee::JSON vecjson2;	vecjson2.setValue("xuanzhuan", "dsf"); vecjson2.setValue("afd", 456);
		//		itflee::JSON vecjson3;	vecjson3.setValue("xuanzhuan", "sfe"); vecjson3.setValue("afd", 789);
		//		std::vector<itflee::JSON> object1vec = { vecjson1, vecjson2, vecjson3 };
		//		object.setArray("object1vec", object1vec);

		//		//2.4.3 object1 设置json对象
		//		itflee::JSON object2;
		//		{
		//			object2.setValue("xuanzhuan", "diajf");
		//			object2.setValue("afd", 123);
		//		}
		//		object.setValue("object2", object2);			//这里就是使用的 拷贝赋值操作
			}
			//root.setValue("object", std::move(object));		//这路不使用 std::move 的话，JSON内部会调用 拷贝赋值操作，使用的话会调用移动赋值 函数,加快效率
		}
	}

	//3.---------------------------------获取值
	{
		std::cout << "-----------------test get value ------------------" << std::endl;
		//3.1获取基本类型值
		//std::string value1 = root.getValue<std::string>("xuanzhuan");
		//std::cout << "value1: " << value1 << std::endl;
		//
		//int value2 = root.getValue<int>("afd");
		//std::cout << "value2: " << value2 << std::endl;

		//bool flag = root.getValue<bool>("flag");
		//std::cout << "flag: " << flag << std::endl;

		//3.2获取数组基本类型值
		//std::vector<int> getvecc1 = root.getArray<int>("vec1");

		//std::vector<int> getvecc1 = root.getValue<std::vector<int>>("vec1");
		//for (auto it = getvecc1.cbegin(); it != getvecc1.cend(); ++it)
		//	std::cout << ' ' << *it;
		//std::cout << '\n';


		//3..2 获取Json 基本类型值  
		// 方法1
		//itflee::JSON obj_v = root.getValue<itflee::JSON>("object");
		//std::string obj_v1 = obj_v.getValue<std::string>("xuanzhuan");
		//std::cout << "obj_v1: " << obj_v1 << std::endl;
		//int obj_v2 = obj_v.getValue<int>("afd");
		//std::cout << "obj_v2: " << obj_v2 << std::endl;
		//bool obj_flag = obj_v.getValue<bool>("flag");
		//std::cout << "obj_flag: " << obj_flag << std::endl;
		//// 方法2
		//std::map<std::string, itflee::JSON> obj_v1_2 = root.getValue<std::map<std::string, itflee::JSON>>("object");
		//std::cout << "obj_v1_2: " << (std::string)(obj_v1_2["xuanzhuan"]) << std::endl;
		

		//3.3获取数组JSON类型值
		//std::vector<itflee::JSON> getvecc2 = root.getArray<itflee::JSON>("vec2");
		//std::string value3 = getvecc2[0].getValue<std::string>("xuanzhuan");
		//std::cout << "vec2  value1: " << value3 << " " << getvecc2[0].getValue<int>("afd") << std::endl;


		//3.3获取JSON类型值
		//{
		//	itflee::JSON object = root.getValue<itflee::JSON>("object");

		//	std::string serialize = object.serialize();

		//	std::cout << "------序列化 object string: " << serialize << std::endl;
		//}
	}


	// 遍历json ，获取 first、second 
	//std::map<std::string, itflee::JSON>

	//for(int i = 0; )
	

	//4---------------------------------序列化成 string
	//std::string serialize = root.serialize();
	//{
	//	std::cout << "-------------------------------" << std::endl;
	//	std::cout << "------序列化 string: " << serialize << std::endl;
	//}
	
	//4.1---------------------------------使用 << 操作符输出
	//{
	//	std::cout << "-------------------------------" << std::endl;
	//	std::cout << "------使用 << 操作符: " << root << std::endl;
	//}
	
	
	//5.--------反序列化string
	//itflee::JSON deserializeObject = itflee::JSON::deserialize(serialize);
	//{
	//	std::cout << "-------------------------------" << std::endl;
	//	std::cout << "deserializeObject  " << deserializeObject.getArray<int>("vec1")[2] << std::endl;
	//}
}

// 测试JSON为基本的类型
[[maybe_unused]] static void jsonTestFunc_2()
{
	// 1. bool
	if(1)
	{
		itflee::JSON base_bool(true);	// JSON(bool v)
		std::cout << "隐式转换: bool: " << base_bool.as<bool>() << std::endl;		// as<bool>()
		
		base_bool = false;				// operator=(bool v)
		bool v = base_bool;				// operator bool() const

		std::cout << "隐式转换: bool: " << v << std::endl;	
		std::cout << "流式输出: bool: " << base_bool << std::endl;	// operator<<(std::ostream& os, const JSON& json)
	}

	// 2. int
	if(1)
	{
		itflee::JSON base_int(1);		// JSON(int v)
		std::cout << "隐式转换: int: " << base_int.as<int>() << std::endl;		// 

		int v = base_int;				// operator int() const
		std::cout << "隐式转换: int: " << v << std::endl;	

		std::cout << "流式输出: int: " << base_int << std::endl;	// operator<<(std::ostream& os, const JSON& json)
	}

	// 3. double
	if(1)
	{
		itflee::JSON base_double(1.1);	// JSON(double v)
		std::cout << "隐式转换: double: " << base_double.as<double>() << std::endl;		// as<double>()
		double v = base_double;				// operator double() const
		std::cout << "隐式转换: double: " << v << std::endl;	
		// 保留小数点后两位输出
		std::cout << "流式输出: double: " << base_double << std::endl;	// operator<<(std::ostream& os, const JSON& json)
	}

	// 4. std::string
	if (1)
	{
		itflee::JSON base_string("hello");	// JSON(const char* s)
		std::cout << "隐式转换: std::string: " << base_string.as<std::string>() << std::endl;		// as<std::string>()

		//base_string = "world";				// operator=(const char* s)
		//std::string v = base_string;		// operator std::string() const

		//std::cout << "隐式转换: std::string: " << v << std::endl;
		//std::cout << "流式输出: std::string: " << base_string << std::endl;	// operator<<(std::ostream& os, const JSON& json)
	}

	// 5. std::vector<JSON>
	if (1)
	{
		std::vector<itflee::JSON> vec = { 10, 20 };
		itflee::JSON base_vector(vec);	// JSON(const std::vector<JSON>& vec)


		std::vector<itflee::JSON> v = base_vector;				// operator std::vector<JSON>() const

		std::cout << "as: std::vector<int>: ";
		for (const auto& item : v) {
			std::cout << ' ' << item.as<int>();
		}
		std::cout << '\n';

		std::cout << "修改第一个元素为 15 " << std::endl;
		base_vector[0] = 15;				// operator[](int index)

		std::cout << "流式输出: std::vector<int>: " << base_vector << std::endl;	// operator<<(std::ostream& os, const JSON& json)
	}

	// 6. std::map<std::string, JSON>
	if (1)
	{
		std::map<std::string, itflee::JSON> mp = { {"key1", 1}, {"key2", "hee"}};
		itflee::JSON base_map(mp);	// JSON(const std::map<std::string, JSON>& mp)
		std::map<std::string, itflee::JSON> v = base_map;				// operator std::map<std::string, JSON>() const
		std::cout << "as: std::map<std::string, int>: ";
		for (const auto& kv : v) {
			std::cout << " " << kv.first << ": " << kv.second;
		}
		std::cout << '\n';
		std::cout << "修改 key1 为 3 " << std::endl;
		base_map["key1"] = 3;				// operator[](const std::string& key)
		std::cout << "流式输出: std::map<std::string, int>: " << base_map << std::endl;	// operator<<(std::ostream& os, const JSON& json)
	}
	
}


[[maybe_unused]] static void jsonTestFunc_vector() 
{
	itflee::JSON root;

	//1. simple use way
	std::vector<int> vec1 = { 1, 2, 3, 4, 5 };
	root["vec1"] = vec1;
	std::vector<std::string> vec2 = { "hello", "world", "123" };
	root["vec2"] = vec2;

	std::vector<int> vec1_ = root["vec1"];
	for (const auto& item : vec1_) {
		std::cout << "vec1_ item: " << item << std::endl;
	}

	std::vector<std::string> vec2_ = root["vec2"];
	for (const auto& item : vec2_) {
		std::cout << "vec2_ item: " << item << std::endl;
	}

	itflee::JSON vec3 = vec1_;
	std::vector<int> vec3_ = vec3;
	for (const auto& item : vec3_) {
		std::cout << "vec3_ item: " << item << std::endl;
	}
}

[[maybe_unused]] static void jsonTestFunc_map() {
	itflee::JSON root;
	std::map<std::string, int> mp1 = { {"key1", 1}, {"key2", 2} };
	root["mp1"] = mp1;
	std::map<std::string, std::string> mp2 = { {"key1", "hello"}, {"key2", "world"} };
	root["mp2"] = mp2;
	
	std::map<std::string, int> mp1_ = root["mp1"];
	for (const auto& kv : mp1_) {
		std::cout << "mp1_ item: " << kv.first << " " << kv.second << std::endl;
	}

	std::map<std::string, std::string> mp2_ = root["mp2"];
	for (const auto& kv : mp2_) {
		std::cout << "mp2_ item: " << kv.first << " " << kv.second << std::endl;
	}
}


// 测试JSON 的嵌套对象与链式创建
[[maybe_unused]] static void jsonTestFunc_3()
{
	// 1. 创建嵌套对象
	itflee::JSON root;
	root["name"] = "Tom";		// operator[](const std::string& key), operator=(const char* s)
	root["age"] = 18;		// operator[](const std::string& key), operator=(int v)

	itflee::JSON obg1;
	obg1["key1"] = 1;
	obg1["key2"] = "value2";

	itflee::JSON obg2;
	obg2["key1"] = 1;
	obg2["key2"] = "value2";

	itflee::JSON obg3;
	obg3["key1"] = 1;
	obg3["key2"] = "value2";

	std::vector<itflee::JSON> obj_array = { obg1, obg2, obg3 };

	root["object"] = obj_array;			// operator=(const JSON& other)


	std::string serialize = root.serialize_compact_rj();

	std::cout << "serialize: " << serialize << std::endl;	// operator<<(std::ostream& os, const JSON& json)

	itflee::JSON deserializeObject = itflee::JSON::deserialize(serialize);
	{
		std::cout << "-------------------------------" << std::endl;
		std::cout << "deserializeObject  " << deserializeObject["age"] << std::endl;
	}
}

// 测试JSON 序列化与反序列化的性能
[[maybe_unused]] static void jsonTestFunc_4()
{
	// 性能测试
	const int test_count = 100000;
	itflee::JSON root;
	root["name"] = "Tom";		// operator[](const std::string& key), operator=(const char* s)
	root["age"] = 18;		// operator[](const std::string& key), operator=(int v)

	itflee::JSON obg1;
	obg1["key1"] = 1;
	obg1["key2"] = "value2";
	itflee::JSON obg2;
	obg2["key1"] = 1;
	obg2["key2"] = "value2";
	itflee::JSON obg3;
	obg3["key1"] = 1;
	obg3["key2"] = "value2";

	std::vector<itflee::JSON> obj_array = { obg1, obg2, obg3 };
	root["object"] = obj_array;			// operator=(const JSON& other)


	// 测试rap
	{
		//itflee::Timing timing;
		//timing.start();
		for (int i = 0; i < test_count; ++i)
		{
			std::string serialize = root.serialize_pretty_rj();
			//std::cout << "serialize: " << serialize << std::endl;	// operator<<(std::ostream& os, const JSON& json)
		}
		//timing.stop();

		//std::cout << "rap的序列化 " << test_count << " 次耗时: " << timing.getElapsedMilliseconds() << " 毫秒" << std::endl;
	}

	// 测试自定义的序列化
	{
		//itflee::Timing timing;
		//timing.start();
		for (int i = 0; i < test_count; ++i)
		{
			std::string serialize = root.serialize();
			//std::cout << "serialize: " << serialize << std::endl;	// operator<<(std::ostream& os, const JSON& json)
		}
		//timing.stop();

		//std::cout << "自定义的序列化 " << test_count << " 次耗时: " << timing.getElapsedMilliseconds() << " 毫秒" << std::endl;
	}
}

[[maybe_unused]] static void jsonTestFunc_5() {
	const std::string json = R"(
        {
            "timestamp": 1234567890,
            "msg_typ": 1,
            "rtk_data":
            {
                "timestamp": 1234567000,
                "fixed": true,
                "longitude": 116.123456,
                "latitude": 39.987654,
                "altitude": 50.5,
                "yaw": 1.57,
                "velocity": 10.2
            },
            "position":
            {
                "link": 1001,
                "offset": 200
            },
            "train_head": 85
        }
        )";

	itflee::JSON j = itflee::JSON::deserialize(json);

	std::cout << "j: " << j.to_string() << std::endl;


	itflee::JSON rtk = j["rtk_data"];
	std::cout << "rtk: " << rtk.to_string() << std::endl;

	if (rtk.contains("longitude")) {
		itflee::JSON lon = rtk["longitude"];
		if (lon.is_number()) {
			std::cout << "lon as number: " << lon << std::endl;
		}
		else
		{
			std::cout << "lon no as number: " << std::endl;
		}
	}
	else {
		std::cout << "lon not found" << std::endl;
	}
}

[[maybe_unused]] static void jsonTestFunc_6() {
	itflee::JSON j;
	j["name"] = "Tom";
	j["age"] = 18;


	double longitude = 116.123456;
	j["longitude"] = longitude;

	std::cout << "j: " << j.to_string() << std::endl;	



	if(j.contains("longitude")) {
		double lon = j["longitude"];
		std::cout << "lon: " << lon << std::endl;
	}
	else {
		std::cout << "lon not found" << std::endl;
	}
}

[[maybe_unused]] static void jsonTestFunc_vector_and_map() {


	itflee::JSON root;
	// test vector
	std::vector<int> vec = { 1, 2, 3, 4, 5 };
	root["vec1"] = vec;
	std::vector<std::string> vec2 = { "hello", "world", "123" };
	root["vec2"] = vec2;

	// test map
	std::map<std::string, int> mp = { {"key1", 1}, {"key2", 2} };
	root["mp1"] = mp;
	std::map<std::string, std::string> mp2 = { {"key1", "hello"}, {"key2", "world"} };
	root["mp2"] = mp2;

	std::cout << "root: " << root.to_string() << std::endl;



	// 

	if (root.contains("vec1")) {
		std::cout << "vec1 found" << std::endl;
		std::vector<int> vec1 = root["vec1"].as<std::vector<int>>();
		std::cout << "vec1: " << vec1.size() << std::endl;
	}
	else {
		std::cout << "vec1 not found" << std::endl;
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
		obj["age"] = 18;
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

		std::vector<int> vi{ 1,2,3,4,5 };
		std::vector<std::string> vs{ "hello","world","json" };

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
		root["user"]["age"] = 20;

		itflee::JSON addr1;
		addr1["city"] = "Beijing";
		addr1["code"] = 100000;

		itflee::JSON addr2;
		addr2["city"] = "Shanghai";
		addr2["code"] = 200000;

		std::vector<itflee::JSON> addrs{ addr1, addr2 };
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
		std::cout << "double as<float>  = " << j_num.as<float>() << std::endl;
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








