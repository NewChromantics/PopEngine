#include "SoyOpenglContext_Win32.h"


void Win32::TOpenglContext::Lock()
{
	//	osx does base context lock first
	TContext::Lock();

	try
	{
		auto mHDC = GetHdc();
		auto mHGLRC = GetHglrc();

		//	switch to this thread
		if (!wglMakeCurrent(mHDC, mHGLRC))
			Platform::ThrowLastError("wglMakeCurrent failed");
	}
	catch (...)
	{
		Unlock();
		throw;
	}
}

void Win32::TOpenglContext::Unlock()
{
	auto mHDC = GetHdc();
	if (!wglMakeCurrent(mHDC, nullptr))
		throw Soy::AssertException("wglMakeCurrent unbind failed");

	TContext::Unlock();
}


Platform::TOpenglContext::TOpenglContext(TControl& Parent, TOpenglParams& Params) :
	Opengl::TRenderTarget(Parent.mName),
	mParent(Parent)
{
	BYTE ColorDepth = 24;
	DWORD Flags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL;

	//	gr: no double buffering stops flicker
	//	none of these flags help
	/*
	#define PFD_SWAP_EXCHANGE           0x00000200
	#define PFD_SWAP_COPY               0x00000400
	#define PFD_SWAP_LAYER_BUFFERS      0x00000800
	Flags |= PFD_DOUBLEBUFFER;
	*/

	//	make the pixel format descriptor
	PIXELFORMATDESCRIPTOR pfd =				// pfd Tells Windows How We Want Things To Be
	{
		sizeof(PIXELFORMATDESCRIPTOR),		// Size Of This Pixel Format Descriptor
		1,									// Version Number
		Flags,					// Must Support Double Buffering
		ColorDepth,									// Select Our Color Depth
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

	uint32_t PixelFormat = 0;

	//	get the existing hdc of the window
	mHDC = GetDC(mParent.mHwnd);
	if (!mHDC)
		throw Soy::AssertException("Failed to get HDC");

	//	store size
	//m_Size = Window.m_ClientSize;

	/*
	//	if multisample is supported, use a multisample pixelformat
	//	SyncBool MultisampleSupport = OpenglExtensions::IsHardwareSupported(OpenglExtensions::GHardware_ARBMultiSample);
	SyncBool MultisampleSupport = SyncFalse;

	if ( MultisampleSupport == SyncTrue )
	{
	//		PixelFormat = OpenglExtensions::GetArbMultisamplePixelFormat();
	m_HasArbMultiSample = TRUE;
	}
	else*/
	{
		//	check we can use this pfd
		PixelFormat = ChoosePixelFormat(mHDC, &pfd);
		if (!PixelFormat)
			throw Soy::AssertException("Failed to choose pixel format X");//%d\n", PixelFormat));
		//m_HasArbMultiSample = FALSE;
	}

	//	set it to the pfd
	if (!SetPixelFormat(mHDC, PixelFormat, &pfd))
		throw Soy::AssertException("Failed to set pixel format");//%d\n", PixelFormat));

	//	make and get the windows gl context for the hdc
	//	gr: this should be wrapped in a context
	mHGLRC = wglCreateContext(mHDC);
	if (!mHGLRC)
		throw Soy::AssertException("Failed to create context");

	//	init opengl context
	Lock();
	this->Init();
	Unlock();

	auto OnPaint = [this](TControl& Control)
	{
		this->OnPaint();
	};
	mParent.mOnPaint = OnPaint;
}

Platform::TOpenglContext::~TOpenglContext()
{
	mParent.mOnPaint = nullptr;
	wglDeleteContext(mHGLRC);
	ReleaseDC(mHwnd, mHDC);
}

void Platform::TOpenglContext::Repaint()
{
	mParent.Repaint();
}


void Platform::TOpenglContext::Bind()
{
	//throw Soy::AssertException("Bind default render target");
}

void Platform::TOpenglContext::Unbind()
{
	//throw Soy::AssertException("Unbind default render target");
}


Soy::Rectx<size_t> Platform::TOpenglContext::GetSize()
{
	//	rect is now correct for window, but for opengl context, it should still be at 0,0
	auto Rect = mRect;
	Rect.x = 0;
	Rect.y = 0;
	return Rect;
}

void Platform::TOpenglContext::OnPaint()
{
	//	better place to flush the queue?
	this->Flush(*this);

	auto& Control = mParent;
	auto& Context = *this;
	Soy::Rectx<size_t> BoundsRect = Control.GetClientRect();
	auto& RenderTarget = *this;


	//	render
	//	much of this code should be copy+pasta from osx as its independent logic
	bool DoneLock = false;
	auto LockContext = [&]
	{
		if (DoneLock)
			return;
		Context.Lock();
		DoneLock = true;
		Opengl::IsOkay("pre drawRect flush", false);
		//	do parent's minimal render
		//	gr: reset state here!
		RenderTarget.mRect = BoundsRect;
		RenderTarget.Bind();
	};
	auto UnlockContext = [&]
	{
		try
		{
			RenderTarget.Unbind();
			Opengl::IsOkay("UnlockContext RenderTarget.Unbind", false);
			Context.Unlock();
		}
		catch (std::exception& e)
		{
			std::Debug << "UnlockContext unbind failed (" << e.what() << "), hail mary context unlock" << std::endl;
			Context.Unlock();
			//	rethrow?
			//throw;
		}
	};

	/*
	if ( !mParent )
	{
		auto ContextLock = SoyScope( LockContext, UnlockContext );
		Opengl::ClearColour( Soy::TRgb(1,0,0) );
		return;
	}
	*/
	try
	{
		if (!mOnRender)
			throw Soy::AssertException("No OnRender callback");

		mOnRender(RenderTarget, LockContext);
	}
	catch (std::exception& e)
	{
		std::Debug << "Window OnRender Exception: " << e.what() << std::endl;
		try
		{
			UnlockContext();
		}
		catch (std::exception&e)
		{
			//std::Debug << "OnRender UnlockContext exception " << e.what() << std::endl;
		}
		return;
		/*
		//	gr: if there's an exception here, it might be the LockContext, rather than the render...
		//		so we're just failing again...
		try
		{
			LockContext();
			Opengl::ClearColour(Soy::TRgb(0, 0, 1));
			std::Debug << "Window OnRender Exception: " << e.what() << std::endl;
		}
		catch (std::exception& e)
		{
			//	okay, lock is the problem
			UnlockContext();
			return;
		}
		*/
	}

	//	in case lock hasn't been done
	LockContext();

	try
	{
		//	flip
		if (!SwapBuffers(mHDC))
			std::Debug << "Failed to SwapBuffers(): " << Platform::GetLastErrorString() << std::endl;

		UnlockContext();
	}
	catch (std::exception& e)
	{
		UnlockContext();
		std::Debug << e.what() << std::endl;
	}
}
