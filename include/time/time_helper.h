#ifndef UTILS_TIME_H_
#define UTILS_TIME_H_

#include <iostream>
#include <string>
#include <sstream>

#include "define.h"

namespace utils
{
	/*
	* Return the number of milliseconds since January 1, 1970, UTC.
	* 也就是64位的当前时间戳
	*/
	ITFLEEEXPORT int64_t Time64();

	//获取 32位的时间戳
	ITFLEEEXPORT int32_t Time32();

	/*获取当前的MS时间戳*/
	ITFLEEEXPORT int64_t GetCurrentTimerMS();

	// 获取本地时间格式
	ITFLEEEXPORT std::string GetCurrentFormattedTime();
};



#endif