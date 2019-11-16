#include <iostream>
#include <PopEngine/PopEngine.h>

int main(int argc, const char * argv[])
{
	std::cout << "Hello, World!";
	//	Unit test files should be under resources in build
	return PopEngine("UnitTest");
}
