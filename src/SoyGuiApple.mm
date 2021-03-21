#include "SoyGuiApple.h"
#include "PopMain.h"


void Platform::RunJobOnMainThread(std::function<void()> Lambda,bool Block)
{
	Soy::TSemaphore Semaphore;

	//	testing if raw dispatch is faster, results negligable
	static bool UseNsDispatch = false;
	
	if ( UseNsDispatch )
	{
		Soy::TSemaphore* pSemaphore = Block ? &Semaphore : nullptr;
		
		dispatch_async( dispatch_get_main_queue(), ^(void){
			
			try
			{
				//	run and @try/@catch
				Platform::ExecuteTryCatchObjc(Lambda);
				if ( pSemaphore )
					pSemaphore->OnCompleted();
			}
			catch(std::exception& e)
			{
				if ( pSemaphore )
					pSemaphore->OnFailed( e.what() );
			}			
		});
		
		if ( pSemaphore )
			pSemaphore->WaitAndReset();
	}
	else
	{
		auto ObjCatchLambda = [=]()
		{
			Platform::ExecuteTryCatchObjc(Lambda);
		};
		
		auto& Thread = *Soy::Platform::gMainThread;
		if ( Block )
		{
			Thread.PushJob(ObjCatchLambda,Semaphore);
			Semaphore.WaitAndReset();
		}
		else
		{
			Thread.PushJob(ObjCatchLambda);
		}
	}
}


