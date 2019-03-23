#include "PCOpenglCanvas.h"
#include "PCWindow.h"

//	my old win32 implementaation
#include "PCWinWindow.h"


//	gr: once the opengl context has been created we need to initialise opengl
//	this will move to the rasteriser, but will still need to be called from here.
#include <TootleRender/TLRender.h>


//------------------------------------------------------
//	
//------------------------------------------------------
TPtr<TLGui::TOpenglCanvas> TLGui::CreateOpenglCanvas(TWindow& Parent,TRefRef Ref)
{
	TPtr<TLGui::TOpenglCanvas> pControl = new TLGui::Platform::OpenglCanvas( Parent, Ref );
	return pControl;
}



TLGui::Platform::OpenglCanvas::OpenglCanvas(TLGui::TWindow& Parent,TRefRef ControlRef) :
	TLGui::TOpenglCanvas	( ControlRef ),
	m_HDC					( NULL ),
	m_HGLRC					( NULL )
{
	//	setup HDC, context etc for the parent window
	TLGui::Platform::Window& Window = static_cast<TLGui::Platform::Window&>(Parent);
	Win32::GWindow* pWindow = Window.m_pWindow.GetObjectPointer<Win32::GWindow>();
	if ( !pWindow )
	{
		TLDebug_Break("win32 window expected");
		return;
	}

	if ( !InitContext( *pWindow ) )
	{
		TLDebug_Break("Failed to setup HDC for opengl");
		return;
	}
}


	
//------------------------------------------------------
//	clean up
//------------------------------------------------------
TLGui::Platform::OpenglCanvas::~OpenglCanvas()
{
#pragma message("todo: delete context, delete HDC")
}

//-------------------------------------------------------
//	create the device context and opengl context 
//-------------------------------------------------------
bool TLGui::Platform::OpenglCanvas::InitContext(Win32::GWindow& Window)
{
	//	make the pixel format descriptor
	PIXELFORMATDESCRIPTOR pfd=				// pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),		// Size Of This Pixel Format Descriptor
		1,									// Version Number
		PFD_DRAW_TO_WINDOW |				// Format Must Support Window
		PFD_SUPPORT_OPENGL |				// Format Must Support OpenGL
		PFD_DOUBLEBUFFER,					// Must Support Double Buffering
		PFD_TYPE_RGBA,						// Request An RGBA Format
	//	16,									// Select Our Color Depth
		24,									// Select Our Color Depth
		0, 0, 0, 0, 0, 0,					// Color Bits Ignored
		0,									// No Alpha Buffer
		0,									// Shift Bit Ignored
		0,									// No Accumulation Buffer
		0, 0, 0, 0,							// Accumulation Bits Ignored
		16,									// 16Bit Z-Buffer (Depth Buffer)  
		1,									//	use stencil buffer
		0,									// No Auxiliary Buffer
		PFD_MAIN_PLANE,						// Main Drawing Layer
		0,									// Reserved
		0, 0, 0								// Layer Masks Ignored
	};
			
	u32 PixelFormat=0;

	//	get the existing hdc of the window
	m_HDC = GetDC( Window.Hwnd() );
	if ( !m_HDC )
	{
		TLDebug_Break("Failed to get HDC");
		return false;
	}

	//	store size
	m_Size = Window.m_ClientSize;

	//	if multisample is supported, use a multisample pixelformat
//	SyncBool MultisampleSupport = OpenglExtensions::IsHardwareSupported(OpenglExtensions::GHardware_ARBMultiSample);
	SyncBool MultisampleSupport = SyncFalse;

	if ( MultisampleSupport == SyncTrue )
	{
//		PixelFormat = OpenglExtensions::GetArbMultisamplePixelFormat();
		m_HasArbMultiSample = TRUE;
	}
	else
	{
		//	check we can use this pfd
		PixelFormat = ChoosePixelFormat( m_HDC, &pfd );
		if ( !PixelFormat )
		{	
			TLDebug_Break( TString("Failed to choose pixel format %d\n",PixelFormat) );
			return false;
		}
		m_HasArbMultiSample = FALSE;
	}

	//	set it to the pfd
	if ( !SetPixelFormat( m_HDC, PixelFormat, &pfd ) )
	{
		TLDebug_Break("Failed to set pixel format");
		return FALSE;
	}

	//	make and get the windows gl context for the hdc
	m_HGLRC = wglCreateContext( m_HDC );
	if ( !m_HGLRC )
	{
		TLDebug_Break("Failed to create context");
		return FALSE;
	}

	//	set current context
	//	gr: unneccesary? done in BeginRender when we *need* it...
	if ( !wglMakeCurrent( m_HDC, m_HGLRC ) )
	{
		TLDebug_Break("Failed wglMakeCurrent");
		return FALSE;
	}

	//	mark opengl as initialised once we've created a GL wglCreateContext
	//	context has been initialised (successfully?) so init opengl
	TLRender::Opengl::Init();
	
	return true;
}


//------------------------------------------------------
//	set as current opengl canvas. If this fails we cannot draw to it
//------------------------------------------------------
Bool TLGui::Platform::OpenglCanvas::BeginRender()
{
	//	not initialised properly
	if ( !m_HDC || !m_HGLRC )
		return false;

	//	set current context
	if ( !wglMakeCurrent( m_HDC, m_HGLRC ) )
		return false;

	return true;
}

//------------------------------------------------------
//	flip buffer at end of render
//------------------------------------------------------
void TLGui::Platform::OpenglCanvas::EndRender()
{
	//	flip buffers
	if ( !m_HDC || !m_HGLRC )
	{
		TLDebug_Break("No view to flip, should have failed BeginRender");
		return;
	}

	//	flip buffers
	SwapBuffers( m_HDC );
}
