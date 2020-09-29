#pragma once

#include "SoyWin32.h"



namespace Platform
{
	class TControlClass;
	class TControl;
	class TWindow;

	class TWin32Thread;	//	A window(control?) is associated with a thread so must pump messages in the same thread https://docs.microsoft.com/en-us/windows/desktop/procthread/creating-windows-in-threads

	//	COM interfaces
	class TDragAndDropHandler;
}


class Platform::TWin32Thread : public SoyWorkerJobThread
{
public:
	TWin32Thread(const std::string& Name, bool Win32Blocking = true);

	virtual void	Wake() override;
	virtual bool	Iteration(std::function<void(std::chrono::milliseconds)> Sleep);

	bool	mWin32Blocking = true;
	DWORD	mThreadId = 0;
};

class Platform::TControl// : public Platform::TDragAndDropHandler
{
public:
	TControl(const std::string& Name, TControlClass& Class, TControl* Parent, DWORD StyleFlags, DWORD StyleExFlags, Soy::Rectx<int> Rect, TWin32Thread& Thread);
	TControl(const std::string& Name, const char* ClassName, TControl& Parent, DWORD StyleFlags, DWORD StyleExFlags, Soy::Rectx<int> Rect);
	virtual ~TControl();

	//	trigger actions
	void					Repaint();
	void					EnableDragAndDrop();

	//	reflection
	Soy::Rectx<int32_t>		GetClientRect();
	void					SetClientRect(const Soy::Rectx<int32_t>& Rect)
	{
		std::Debug << "todo SetClientRect(" << Rect << ")" << std::endl;
	}

	//	callbacks from windows message loop
	virtual void			OnDestroyed();		//	window handle is being destroyed
	virtual void			OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) {}	//	called from window thread which means we can flush jobs
	virtual void			OnScrolled(bool Horizontal, uint16_t ScrollCommand, uint16_t CurrentScrollPosition);

	//	return true if handled, or false to return default behavouir
	bool			OnMouseEvent(int x, int y, WPARAM Flags, UINT EventMessage);
	bool			OnKeyDown(WPARAM KeyCode, LPARAM Flags);
	bool			OnKeyUp(WPARAM KeyCode, LPARAM Flags);
	bool			OnDragDrop(HDROP DropHandle);

	virtual TWin32Thread&	GetJobQueue()
	{
		return mThread;
	}

	TControl&		GetChild(HWND Handle);
	virtual void	AddChild(TControl& Child);

public:
	std::function<void(TControl&)>	mOnPaint;
	std::function<void(TControl&, const TMousePos&, SoyMouseButton::Type)>	mOnMouseDown;
	std::function<void(TControl&, const TMousePos&, SoyMouseButton::Type)>	mOnMouseMove;
	std::function<void(TControl&, const TMousePos&, SoyMouseButton::Type)>	mOnMouseUp;
	std::function<void(TControl&, SoyKeyButton::Type)>	mOnKeyDown;
	std::function<void(TControl&, SoyKeyButton::Type)>	mOnKeyUp;
	std::function<bool(TControl&, ArrayBridge<std::string>&&)>	mOnDragAndDrop;

	std::function<void(TControl&)>	mOnDestroy;	//	todo: expand this to OnClose to allow user to stop it from closing

	HWND			mHwnd = nullptr;
	std::string		mName;
	TWin32Thread&	mThread;
	uint64_t		mChildIdentifier = 0;
	static uint64_t	gChildIdentifier;

	Array<TControl*>	mChildren;
};


class Platform::TWindow : public TControl, public SoyWindow
{
public:
	TWindow(const std::string& Name, Soy::Rectx<int> Rect, TWin32Thread& Thread, bool Resizable);

	virtual void	OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;
	virtual void	OnScrolled(bool Horizontal, uint16_t ScrollCommand, uint16_t CurrentScrollPosition) override;

	virtual Soy::Rectx<int32_t>		GetScreenRect() override { return GetClientRect(); }
	virtual void					SetFullscreen(bool Fullscreen) override;
	virtual bool					IsFullscreen() override;
	virtual bool					IsMinimised() override;
	virtual bool					IsForeground() override;
	virtual void					EnableScrollBars(bool Horz, bool Vert) override;

	void			UpdateScrollbars();
	virtual void	AddChild(TControl& Child) override;

private:
	//	for saving/restoring fullscreen mode
	std::shared_ptr<WINDOWPLACEMENT>	mSavedWindowPlacement;	//	saved state before going fullscreen
	LONG				mSavedWindowFlags = 0;
	LONG				mSavedWindowExFlags = 0;

public:
	std::shared_ptr<TWin32Thread>		mOwnThread;
};



class Platform::TOpenglContext : public  Opengl::TRenderTarget, public Win32::TOpenglContext
{
public:
	TOpenglContext(TControl& Parent, Win32::TOpenglParams& Params);
	~TOpenglContext();

	//	render target
	virtual void				Bind() override;
	virtual void				Unbind() override;
	virtual Soy::Rectx<size_t>	GetSize() override;

	//	window stuff
	void			Repaint();
	void			OnPaint();

	std::function<void(Opengl::TRenderTarget&, std::function<void()>)>	mOnRender;

	//	win32::TOpenglContext
	virtual HDC		GetHdc() override { return mHDC; }
	virtual HGLRC	GetHglrc() override { return mHGLRC; }
	virtual HWND	GetHwnd() override { return mHwnd; }

	//	context stuff
	TControl&		mParent;	//	control we're bound to
	HWND&			mHwnd = mParent.mHwnd;
	HDC				mHDC = nullptr;		//	DC we've setup for opengl
	HGLRC			mHGLRC = nullptr;	//	opengl context
	bool			mHasArbMultiSample = false;	//	is antialiasing supported?

	//	render target
	Soy::Rectx<size_t>	mRect;
};