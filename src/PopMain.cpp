#include "PopMain.h"
#include "PopEngine.h"

#if defined(TARGET_WINDOWS)
	#include <tchar.h>
#endif


namespace Pop
{
	std::string ProjectPath;
}


namespace Platform
{
#if defined(TARGET_WINDOWS)
	namespace Private
	{
		extern HINSTANCE InstanceHandle;
	}
#endif


	//	from SoyDebug
#if defined(TARGET_LUMIN) || defined(TARGET_ANDROID)
	const char*	LogIdentifer = "PopEngine";
#endif
}

extern "C" EXPORT int PopEngine(const char* ProjectPath)
{
	Pop::ProjectPath = ProjectPath;

#if defined(TARGET_LUMIN)
	//	for now
	return 1;
#elif defined(TARGET_OSX)|| defined(TARGET_IOS)
	return Soy::Platform::BundleAppMain();
#else
	return PopMain();
#endif
}


#if !defined(TARGET_LINUX)	//	temporarily TestApp and engine are built together
extern "C" int main(int argc,const char* argv[])
{
	if ( argc < 2 )
		return PopEngine(nullptr);
	
	return PopEngine( argv[1] );
}
#endif


//	define winmain AND main for gui & console subsystem builds
#if defined(TARGET_WINDOWS)
//int _tmain(int argc, _TCHAR* argv[])
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	Platform::Private::InstanceHandle = hInstance;
	const char* argv[1] = { lpCmdLine };
	int argc = std::size(argv);

	return main(argc, argv);
}
#endif

