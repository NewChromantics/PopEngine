#pragma once

#include "SoyWin32.h"
#include <SoyWindow.h>



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

	void			SetVisible(bool Visible);
	void			SetColour(const vec3x<uint8_t>& Rgb);

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




class Platform::TSlider : public TControl, public SoySlider
{
public:
	TSlider(Platform::TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void		SetVisible(bool Visible) override { Platform::TControl::SetVisible(Visible); }
	virtual void		SetColour(const vec3x<uint8_t>& Rgb) override { Platform::TControl::SetColour(Rgb); }

	virtual void		SetMinMax(uint16_t Min, uint16_t Max, uint16_t NotchCount) override;
	virtual void		SetValue(uint16_t Value) override;
	virtual uint16_t	GetValue() override;

	virtual void		OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;
};

class Platform::TLabel : public TControl, public SoyLabel
{
public:
	TLabel(Platform::TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void		SetVisible(bool Visible) override { Platform::TControl::SetVisible(Visible); }
	virtual void		SetColour(const vec3x<uint8_t>& Rgb) override { Platform::TControl::SetColour(Rgb); }

	virtual void		SetValue(const std::string& Value) override;
	virtual std::string	GetValue() override;
};


class Platform::TTextBox : public TControl, public SoyTextBox
{
public:
	TTextBox(Platform::TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void		SetVisible(bool Visible) override { Platform::TControl::SetVisible(Visible); }
	virtual void		SetColour(const vec3x<uint8_t>& Rgb) override { Platform::TControl::SetColour(Rgb); }

	virtual void		SetValue(const std::string& Value) override;
	virtual std::string	GetValue() override;

	virtual void		OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;

private:
	bool				mTextChangedByCode = true;
};


class Platform::TTickBox : public TControl, public SoyTickBox
{
public:
	TTickBox(Platform::TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void		SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void		SetVisible(bool Visible) override { Platform::TControl::SetVisible(Visible); }
	virtual void		SetColour(const vec3x<uint8_t>& Rgb) override { Platform::TControl::SetColour(Rgb); }

	virtual void		SetValue(bool Value) override;
	virtual bool		GetValue() override;
	virtual void		SetLabel(const std::string& Label) override;

	virtual void		OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;
};


class Platform::TColourButton : public TControl, public SoyColourButton
{
public:
	TColourButton(Platform::TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void			SetRect(const Soy::Rectx<int32_t>& Rect) override { SetClientRect(Rect); }
	virtual void			SetVisible(bool Visible) override { Platform::TControl::SetVisible(Visible); }
	virtual void			SetColour(const vec3x<uint8_t>& Rgb) override { Platform::TControl::SetColour(Rgb); }

	virtual void			SetValue(vec3x<uint8_t> Value) override;
	virtual vec3x<uint8_t>	GetValue() override;

	virtual void			OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;
};


class Platform::TImageMap : public TControl, public Gui::TImageMap
{
public:
	TImageMap(Platform::TControl& Parent, Soy::Rectx<int32_t>& Rect);

	virtual void			SetRect(const Soy::Rectx<int32_t>& Rect) override	{	SetClientRect(Rect); }
	virtual void			SetVisible(bool Visible) override					{	Platform::TControl::SetVisible(Visible); }
	virtual void			SetColour(const vec3x<uint8_t>& Rgb) override		{	Platform::TControl::SetColour(Rgb); }

	virtual void			SetImage(const SoyPixelsImpl& Pixels) override;
	virtual void			SetCursorMap(const SoyPixelsImpl& CursorMap, const ArrayBridge<std::string>&& CursorIndexes)override;

	virtual void			OnWindowMessage(UINT EventMessage, DWORD WParam, DWORD LParam) override;
};

