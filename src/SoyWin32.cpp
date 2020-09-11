#include "SoyWin32.h"
#include <SoyAssert.h>
#include <SoyWindow.h>
#include <SoyShellExecute.h>

#pragma comment(lib, "Comctl32.lib")
#pragma comment(lib, "Shell32.lib")


namespace Platform
{
	namespace Private
	{
		HINSTANCE InstanceHandle = nullptr;
	}
}

#if defined(TARGET_UWP)
std::shared_ptr<SoyWindow> Platform::CreateWindow(const std::string& Name, Soy::Rectx<int32_t>& Rect, bool Resizable)
{
	Soy_AssertTodo();
}
#endif

#if defined(TARGET_UWP)
std::shared_ptr<SoyLabel> Platform::CreateLabel(SoyWindow& Parent, Soy::Rectx<int32_t>& Rect)
{
	Soy_AssertTodo();
}
#endif

#if defined(TARGET_UWP)
Soy::TShellExecute::TShellExecute(const std::string& Command, const ArrayBridge<std::string>&& Arguments, std::function<void(int)> OnExit, std::function<void(const std::string&)> OnStdOut, std::function<void(const std::string&)> OnStdErr) :
	SoyThread(std::string("ShellExecute ") + Command)
{
	Soy_AssertTodo();
}
#endif

#if defined(TARGET_UWP)
Soy::TShellExecute::~TShellExecute()
{
}
#endif

#if defined(TARGET_UWP)
bool Soy::TShellExecute::ThreadIteration()
{
	Soy_AssertTodo();
}
#endif