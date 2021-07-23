#include <iostream>
#include "../src/PopEngine.h"	//	temp fix for linux
#include <string>


bool IsXCodeDebugParam(const std::string& Argument)
{
	if ( Argument == "-NSDocumentRevisionsDebugMode" )
		return true;
	
	if ( Argument == "YES" )
		return true;
	
	return false;
}

#if defined(TARGET_LINUX) || defined(TARGET_WINDOWS)//	ios bootin from swift now
int main(int argc, const char * argv[])
{
	//	if first arg is a path, then to make debugging easier in xcode,
	//	lets load that project instead of unit tests
	std::string ProjectPath = "UnitTest";
	
	//	see if any valid params are passed in
	for ( auto a=1;	a<argc;	a++ )
	{
		auto Arg = argv[a];
		if ( IsXCodeDebugParam(Arg) )
			continue;

		ProjectPath = Arg;
		break;
	}
	
	std::cerr << "Running project " << ProjectPath << std::endl;
	//	Unit test files should be under resources in build
	return PopEngine( ProjectPath.c_str() );
}
#endif
