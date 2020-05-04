#include <iostream>
#include "PopEngine.h"
#include <string>


bool IsXCodeDebugParam(const std::string& Argument)
{
	if ( Argument == "-NSDocumentRevisionsDebugMode" )
		return true;
	
	if ( Argument == "YES" )
		return true;
	
	return false;
}

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
	
	std::cout << "Running project " << ProjectPath << std::endl;
	//	Unit test files should be under resources in build
	return PopEngine( ProjectPath.c_str() );
}
