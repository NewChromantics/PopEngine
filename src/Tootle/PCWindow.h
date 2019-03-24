/*------------------------------------------------------

	

-------------------------------------------------------*/
#pragma once
#include "../TWindow.h"


namespace TLGui
{
	namespace Platform
	{
		class Window;
	}
}

//	forward declaration
namespace Win32
{
	class GWinControl;
}

class TLGui::Platform::Window : public TLGui::TWindow
{
public:
	Window(TRefRef WindowRef);
	
	virtual Bool			IsVisible() const;
	virtual void			Show();
	
	virtual void			SetSize(const Type2<u16>& WidthHeight);	//	set the client size of the window (game doesn't care about the real size, only the client size)
	virtual Type2<u16>		GetSize();								//	get the client size of the window
	virtual void			SetPosition(const Type2<u16>& xy);		//	set the window's top left position
	virtual Type2<u16>		GetPosition() const;					//	get the window's top left position
	
	virtual void*			GetHandle() const;

public:	//	access for anything that explicitly casts a window to this type
	TPtr<Win32::GWinControl>	m_pWindow;
};



