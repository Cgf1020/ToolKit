#include <iostream>
#include <thread>

#include "jsontest.hpp"
#include "loghelpertest.hpp"
#include "time_test.hpp"
#include "cache_queue_test.hpp"
#include "configini_test.hpp"
#include "thread_invoke_test.hpp"
#include "threadpool_invoke_test.hpp"



int main()
{
	std::cout << "threadid: " << std::this_thread::get_id() << std::endl;

	logHelperTest();
	//jsonTestBug(itflee::JSON::deserialize("{\"key\": \"dfakjfklajf\"}"));
	jsonTestAll();
	// jsonTestFunc_vector();
	// jsonTestFunc_map();
	// time_test();
	// itflee::test::configini_test();
	
	// ThreadInvoke tests
	// itflee::test::thread_invoke_test_simple();
	// itflee::test::thread_invoke_test_sync();
	itflee::test::threadpool_invoke_test_simple();
	// itflee::test::threadpool_invoke_test_sync();
	// itflee::test::threadpool_invoke_test_concurrent();
	itflee::test::threadpool_invoke_test_wait();


	// itflee::test::cache_queue_test();
	// auto timer = itflee::Timer::CreateTimer("test", 1000, false, [](const std::string& name) {
	// 	std::cout << "timer213: " << name << std::endl;
	// 	}, [](const std::string& name) {
	// 		std::cout << "timer342: " << name << std::endl;
	// 		});

	// 	timer->StartTimer();

// #ifdef _WIN32
// 	system("pause");
// #else
	std::cout << "Press Enter to continue...";
	std::cin.get();


	return 0;
}