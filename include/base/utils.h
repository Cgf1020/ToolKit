#ifndef UTILS_H_
#define UTILS_H_

#include <iostream>
#include <string>
#include <sstream>
#include <cstddef>
#include <cstdint>
#include <string_view>


#include "define.h"

namespace utils
{

	ITFLEEEXPORT std::wstring s2ws(const std::string& str);

	ITFLEEEXPORT std::string ws2s(const std::wstring& s);

	//数字转字符串
	template <typename T> 
	ITFLEEEXPORT std::string toString(T&& num)
	{
		std::stringstream ss;
		std::string str;
		ss << num;
		ss >> str;

		return std::move(str);
	}

	//字符串转数字
	template <typename T> 
	ITFLEEEXPORT T toNumber(std::string strNum)
	{
		T num = 0;
		std::stringstream ss;
		ss << strNum;
		ss >> num;
		return static_cast<T>(num);
	}

	/*生成长度为 length的随机数
	*/
	ITFLEEEXPORT std::string generateRandomString(std::size_t length);



	/*系统：进程有关的----------------------------------------------------------------*/

	/**
	* 获取可执行文件的路径
	* @return std::string success
	*/
	ITFLEEEXPORT std::string GetModulePath();


	/**
	* 获取进程ID
	* @param processName 进程名
	* @return 0 success
	*/
	ITFLEEEXPORT uint32_t GetProcessIdByName(const std::string& processName);


	/**
	* 杀死进程
	* @param processID 进程ID
	* @return true success
	*/
	ITFLEEEXPORT bool KillProcess(uint32_t processID);


	/**
	* 编码base64
	* @param bytes_to_encode 需要编码的内存
	* @param in_len 需要编码的内存长度
	* @return std::string  编码完成的base64字符串
	*/
	ITFLEEEXPORT std::string base64_encode(const char* bytes_to_encode, unsigned int in_len);
	ITFLEEEXPORT std::string base64_decode(const std::string& encoded_string);
	ITFLEEEXPORT std::string jpegToBase64(const std::string& filename);
	ITFLEEEXPORT std::string imgToBase64(const char* bytes_to_encode, unsigned int in_len, std::string img_base64_header);
};





namespace tempfun
{
	//ffplay 的命令行调用
	void musicffplay();


	constexpr const std::string_view testConstexprFun();



};

#endif