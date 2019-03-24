#include "PCWindow.h"
#include "PCWinWindow.h"


//	temp whilst the factory functions are in this file
#include "../TTree.h"


//------------------------------------------------------
//	
//------------------------------------------------------
TPtr<TLGui::TWindow> TLGui::CreateGuiWindow(TRefRef Ref)
{
	TPtr<TLGui::TWindow> pWindow = new TLGui::Platform::Window( Ref );
	return pWindow;
}



TPtr<TLGui::TTree> TLGui::CreateTree(TLGui::TWindow& Parent,TRefRef Ref,TPtr<TLGui::TTreeItem>& pRootItem,const TArray<TRef>& Columns)
{
	TPtr<TLGui::TTree> pControl;
	return pControl;
}



TLGui::Platform::Window::Window(TRefRef WindowRef) : 
	TLGui::TWindow		( WindowRef )
{
	//	need to wait for win32 factory to exist
	if ( !Win32::g_pFactory )
	{
		TLDebug_Break("Win32 factory expected");
		return;
	}

	//	create a window
	TPtr<Win32::GWinControl> pNullParent;
	m_pWindow = Win32::g_pFactory->GetInstance( WindowRef, TRUE, "Window" );
	if ( !m_pWindow )
	{
		TLDebug_Break("Failed to allocate window from win32");
		return;
	}

	if ( !m_pWindow->Init( pNullParent, m_pWindow->DefaultFlags() ) )
	{
		TLDebug_Break("Failed to init win32 window");
		Win32::g_pFactory->RemoveInstance( m_pWindow->GetRef() );
		m_pWindow = NULL;
		return;
	}
}


Bool TLGui::Platform::Window::IsVisible() const
{
	return m_pWindow ? !m_pWindow->IsClosed() : false;
}

void TLGui::Platform::Window::Show()
{
	if ( !m_pWindow )
		return;

	m_pWindow->Show();
}


//---------------------------------------------------------
//	set the CLIENT SIZE ("content" in os x) of the window
//---------------------------------------------------------
void TLGui::Platform::Window::SetSize(const Type2<u16>& WidthHeight)
{
	if ( !m_pWindow )
		return;

	//	gr: not sure this is setting client size?
	m_pWindow->Resize( WidthHeight );
}


//---------------------------------------------------------
//	set the CLIENT SIZE ("content" in os x) of the window
//---------------------------------------------------------	 
Type2<u16> TLGui::Platform::Window::GetSize()
{
	if ( !m_pWindow )
		return Type2<u16>(0,0);

	return Type2<u16>( m_pWindow->m_ClientSize.x, m_pWindow->m_ClientSize.y );
}

//---------------------------------------------------
//	set the top-left position of the window frame
//---------------------------------------------------
void TLGui::Platform::Window::SetPosition(const Type2<u16>& xy)
{
	if ( !m_pWindow )
		return;

	m_pWindow->Move( int2(xy) );
}


//---------------------------------------------------
//	get the window's top left position
//---------------------------------------------------
Type2<u16> TLGui::Platform::Window::GetPosition() const
{
	if ( !m_pWindow )
		return Type2<u16>();

	return m_pWindow->m_ClientPos;
}


void* TLGui::Platform::Window::GetHandle() const						
{
	return m_pWindow ? m_pWindow->HwndConst() : NULL;	
}
