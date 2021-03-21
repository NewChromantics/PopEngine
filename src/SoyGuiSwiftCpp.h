#pragma once

#include "SoyGuiSwift.h"


#include <string>
#include "SoyWindow.h"	//	could forward declare these

namespace Swift
{
	//	this finds a class with this name, and returns it's window from .window
	NSWindow* _Nonnull	CreateSwiftWindow(const std::string& Name);

	std::shared_ptr<SoyWindow>			GetWindow(const std::string& Name);
	std::shared_ptr<SoyLabel>			GetLabel(const std::string& Name);
	std::shared_ptr<SoyButton>			GetButton(const std::string& Name);
	std::shared_ptr<SoyTickBox>			GetTickBox(const std::string& Name);
	std::shared_ptr<Gui::TRenderView>	GetRenderView(const std::string& Name);
	std::shared_ptr<Gui::TList>			GetList(const std::string& Name);
}

