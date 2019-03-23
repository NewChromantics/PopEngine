/*------------------------------------------------------

	

-------------------------------------------------------*/
#pragma once
#include "../TOpenglCanvas.h"
#include "PCWinWindow.h"	//	need to include for HDC etc
#include <TootleOpenglRasteriser/TLOpengl.h>



namespace TLGui
{
	namespace Platform
	{
		class OpenglCanvas;
	}
}


class TLGui::Platform::OpenglCanvas : public TLGui::TOpenglCanvas
{
public:
	OpenglCanvas(TLGui::TWindow& Parent,TRefRef ControlRef);
	~OpenglCanvas();
	
	virtual Bool	BeginRender();
	virtual void	EndRender();
	
	virtual Type2<u16>	GetSize() const						{	return m_Size;	}

protected:
	bool			InitContext(Win32::GWindow& Window);	//	create the device context and opengl context

protected:
	Type2<u16>		m_Size;		//	size of the DC
	HDC				m_HDC;		//	DC we've setup for opengl
	HGLRC			m_HGLRC;	//	opengl context
	bool			m_HasArbMultiSample;	//	is antialiasing supported?
};



