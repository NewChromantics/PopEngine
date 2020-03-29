#include <iostream>
#include <PopEngine_Osx/PopEngine.h>

int main(int argc, const char * argv[])
{
	//	if first arg is a path, then to make debugging easier in xcode,
	//	lets load that project instead of unit tests
	std::string ProjectPath = "UnitTest";
	/*
	if ( argc > 1 )
	{
		//if ( Platform::IsFullPath(argv[0] )
		ProjectPath = argv[1];
	}
	*/
	std::cout << "Running project " << ProjectPath;
	//	Unit test files should be under resources in build
	return PopEngine( ProjectPath.c_str() );
}
