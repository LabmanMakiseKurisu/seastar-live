#include <seastar/core/app-template.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/future.hh>
#include <seastar/core/sleep.hh>
#include <seastar/core/seastar.hh>
#include <iostream>

using namespace seastar;
future<int> incr(int i)
{
	using namespace std::chrono_literals;
	return sleep(10ms).then([i=std::move(i)] { return i + 1; });
}

future<> slow_op(int& o) {
	using namespace std::chrono_literals;
	return sleep(10ms).then([&o]()
									 { std::cout << o << std::endl; })
		.then([&o]()
			  { std::cout << o << std::endl; });
}

future<> fcn()
{
	int a = 1;
	//这个lambda函数的参数必须是被持久化变量的引用类型，因为持久化变量在堆上
	return do_with(a, [](int &o)
							{
		return slow_op(o); });
}


int main(int argc, char **argv)
{
	app_template app;
	app.run(argc, argv, fcn);
	return 0;
}