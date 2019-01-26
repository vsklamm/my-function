#include "function.h"
#include <tuple>
#include <iostream>
#include <functional>
#include <cassert>

struct foo {
	int *ptr;
	int x;
	foo(int x) : x{ x }
	{
		ptr = &this->x;
	}
	foo(const foo &other) : foo(other.x) { }

	int operator()(int arg) {
		return *ptr + arg;
	}
};

int main() 
{
	vsklamm::function<int(int)> f = foo(42);
	vsklamm::function<int(int)> f2 = foo(0);
	f.swap(f2);

	assert(f(228) == 228);
	assert(f2(228) == 270);

	system("pause");
}