#include "PCApp.h"
#include <TootleCore/TCoreManager.h>
#include <TootleCore/TLCore.h>
#include <TootleCore/TLTime.h>
#include <TootleCore/PC/PCDebug.h>
#include <TootleFileSys/TLFileSys.h>




namespace TLGui
{
	namespace Platform
	{
		u32							g_TimerUpdateID = 0;			//	ID of the win32 timer we're using for the update intervals
		MMRESULT					g_MMTimerUpdateID = NULL;		//	gr: null is the correct "invalid" state	
	}
}



namespace TLGui
{
	namespace Platform
	{
		void CALLBACK	UpdateTimerCallback(HWND hwnd,UINT uMsg,UINT_PTR idEvent,DWORD dwTime);
		void CALLBACK	UpdateMMTimerCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);
	}
}



//--------------------------------------------------
//	mmsystem update timer callback
//--------------------------------------------------
void CALLBACK TLGui::Platform::UpdateMMTimerCallback(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2)
{
	//	dont do any threaded code whilst breaking
	if ( TLDebug::IsBreaking() )
		return;

	if ( TLCore::g_pCoreManager )
		TLCore::g_pCoreManager->SetReadyForUpdate();
}



//--------------------------------------------------
//	win32 update timer callback
//--------------------------------------------------
void CALLBACK TLGui::Platform::UpdateTimerCallback(HWND hwnd,UINT uMsg,UINT_PTR idEvent,DWORD dwTime)
{
	//	dont do any threaded code whilst breaking
	if ( TLDebug::IsBreaking() )
		return;

	//	check params, this should just be a callback for the update timer
	if( uMsg != WM_TIMER )
	{
		if ( !TLDebug_Break("Unexpected timer callback") )
			return;
	}

	if(idEvent != g_TimerUpdateID)
	{
		// Suggests that we are running out of a frame??
	}

	//	ready for another update
	if ( TLCore::g_pCoreManager )
		TLCore::g_pCoreManager->SetReadyForUpdate();
}





//--------------------------------------------------
//	platform init
//--------------------------------------------------
Bool TLGui::Platform::App::Init()
{
	//	setup the update timer
	u32 UpdateInterval = (u32)TLTime::GetUpdateTimeMilliSecsf();

	Bool UseMMTimer = TRUE;

	//	to make debugging easier in VS (ie. let windows breath) we dont use the MM timer, this way the windows
	//	message queue is blocking
	//	gr: with the current engine update layout, we have to use MM timer, this 
	//		platform update won't get called until we get a timer message, but we dont
	//		get a timer message unless platform update is called. wx will fix this
//	if ( TLDebug::IsEnabled() )
//		UseMMTimer = FALSE;

	if ( UseMMTimer )
	{
		g_MMTimerUpdateID = timeSetEvent( UpdateInterval, 0, TLGui::Platform::UpdateMMTimerCallback, 0, TIME_PERIODIC );
		if ( g_MMTimerUpdateID == NULL )
			UseMMTimer = FALSE;
	}

	if ( !UseMMTimer )
	{
		UpdateInterval = 1;
		g_TimerUpdateID = (u32)SetTimer( NULL, 0, UpdateInterval, TLGui::Platform::UpdateTimerCallback );
		if ( g_TimerUpdateID == 0 )
		{
			TLDebug::Platform::CheckWin32Error();
			return false;
		}
	}

	return true;
}


//--------------------------------------------------
//	platform shutdown
//--------------------------------------------------
SyncBool TLGui::Platform::App::Shutdown()
{
	if ( g_TimerUpdateID != 0 )
	{
		KillTimer( NULL, g_TimerUpdateID );
	}

	return SyncTrue;
}


//---------------------------------------------------
//	win32 entry
//---------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow)
{
	int Result = 0;
	TStringLowercase<TTempString> Params = lpCmdLine;
	if ( TLGui::OnCommandLine( Params, Result ) )
		return Result;

	//	set the global reference to HInstance
	TLGui::Platform::g_HInstance = hInstance;

	//	set global app exe
	TBufferString<MAX_PATH> Filename;
	TArray<TChar>& Buffer = Filename.GetStringArray();
	Buffer.SetSize( MAX_PATH );
	u32 ExeStringLength = GetModuleFileName( NULL, Buffer.GetData(), Buffer.GetSize() );
	Filename.SetLength( ExeStringLength );
	TLFileSys::GetParentDir( Filename );
	TLGui::SetAppPath( Filename );

	//	allocate an app
	TPtr<TLGui::Platform::App> pApp = new TLGui::Platform::App();

	//	do init loop
	Bool InitResult = pApp->Init();
	InitResult = InitResult ? TLCore::TootInit() : false;
	
	//	init was okay, do update loop
	SyncBool UpdateLoopResult = InitResult ? SyncWait : SyncFalse;
	while ( UpdateLoopResult == SyncWait )
	{
		pApp->Update();
		UpdateLoopResult = TLCore::TootUpdate();
	}

	//	shutdown loop
	Bool ShutdownLoopResult = TLCore::TootShutdown( InitResult );
	pApp->Shutdown();
	pApp = NULL;

	return (UpdateLoopResult==SyncTrue) ? 0 : 255;
}



//--------------------------------------------------
//	platform update
//--------------------------------------------------
SyncBool TLGui::Platform::App::Update()
{
	MSG msg;
	
	//	win32 style app update (blocking)
	Bool Blocking = TRUE;
	Blocking = (g_MMTimerUpdateID == NULL);

	if ( Blocking )
	{
		//	wait for message
		while (GetMessage(&msg, NULL, 0, 0)) 
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

			//	no more messages, and we've got updates to do so break out and let the app loop
			if ( !PeekMessage(&msg,NULL,0,0,PM_NOREMOVE) )
			{
				//	ready for an update - break out so we can do an update
				if ( TLCore::g_pCoreManager->IsReadyForUpdate() )
					break;
			}
		}

	}
	else
	{
		//	process windows messages if there are any
		while ( PeekMessage(&msg,NULL,0,0,PM_REMOVE) )
		{
			if ( msg.message == WM_QUIT )
			{
				TLCore::Quit();
				return SyncTrue;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}


	//	keep app running
	return SyncTrue;
}


