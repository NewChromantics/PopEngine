#include "TApiEngine.h"

#include "SoyWindow.h"
#include "TApiGui.h"

namespace ApiEngine
{
	//	gr: oops, can't use Pop.Debug()...
	const char Namespace[] = "Pop.Engine";
	DEFINE_BIND_TYPENAME(StatsWindow);
}


class ApiEngine::TStatsWindow : public SoyWorkerThread
{
public:
	TStatsWindow(Soy::Rectx<int32_t> Rect, std::function<Bind::TContext&()> GetContext);
	TStatsWindow(std::shared_ptr<SoyLabel> Label, std::function<Bind::TContext&()> GetContext);

protected:
	virtual bool	Iteration() override;

protected:
	std::shared_ptr<SoyWindow>	mWindow;
	std::shared_ptr<SoyLabel>	mLabel;

	//	we could keep a pointer/reference here as this shouldn't exist if the context doesn't, but lets be safer
	std::function<Bind::TContext&()>	mGetContext;
};



void ApiEngine::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindObjectType<TStatsWindowWrapper>(Namespace);
}





void ApiEngine::TStatsWindowWrapper::Construct(Bind::TCallback& Params)
{
	auto GetContext = [this]() -> Bind::TContext&
	{
		return this->GetContext();
	};

	
	//	if user provides a gui label, use that
	if ( Params.IsArgumentObject(0) )
	{
		auto& Label = Params.GetArgumentPointer<ApiGui::TLabelWrapper>(0);
		auto pLabel = Label.mLabel;
		mWindow.reset(new TStatsWindow(pLabel, GetContext));
		return;
	}
	
	Soy::Rectx<int32_t> Rect(0, 0, 200, 200);

	//	if no rect, get rect from screen
	if (!Params.IsArgumentUndefined(1))
	{
		BufferArray<int32_t, 4> Rect4;
		Params.GetArgumentArray(1, GetArrayBridge(Rect4));
		Rect.x = Rect4[0];
		Rect.y = Rect4[1];
		Rect.w = Rect4[2];
		Rect.h = Rect4[3];
	}
	else
	{
		//	get first monitor size
		auto SetRect = [&](const Platform::TScreenMeta& Screen)
		{
			if (Rect.w > 0)
				return;
			auto BorderX = Screen.mWorkRect.w / 4;
			auto BorderY = Screen.mWorkRect.h / 4;
			Rect.x = Screen.mWorkRect.x + BorderX;
			Rect.y = Screen.mWorkRect.y + BorderY;
			Rect.w = Screen.mWorkRect.w - BorderX - BorderX;
			Rect.h = Screen.mWorkRect.h - BorderY - BorderY;
		};
		Platform::EnumScreens(SetRect);
	}
	
	mWindow.reset(new TStatsWindow(Rect, GetContext));
}

void ApiEngine::TStatsWindowWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	using namespace ApiEngine;

	//Template.BindFunction<BindFunction::Alloc>( Alloc );
}



ApiEngine::TStatsWindow::TStatsWindow(Soy::Rectx<int32_t> Rect, std::function<Bind::TContext&()> GetContext) :
	SoyWorkerThread	(__PRETTY_FUNCTION__,SoyWorkerWaitMode::Sleep),
	mGetContext		( GetContext )
{
	auto WindowName = "Pop.Debug.Stats";
	auto Resizable = true;
	mWindow = Platform::CreateWindow(WindowName, Rect, Resizable);

	//	create a label that fills the size
	mLabel = Platform::CreateLabel(*mWindow, Rect);
	mLabel->SetValue("Hello!");

	//	todo: resize label when window resizes

	//	start auto-update thread
	Start();
}


ApiEngine::TStatsWindow::TStatsWindow(std::shared_ptr<SoyLabel> Label,std::function<Bind::TContext&()> GetContext) :
	SoyWorkerThread	(__PRETTY_FUNCTION__,SoyWorkerWaitMode::Sleep),
	mGetContext		( GetContext )
{
	if ( !Label )
		throw Soy::AssertException("Label expected");

	mLabel = Label;
	mLabel->SetValue("Hello!");
	
	//	start auto-update thread
	Start();
}

bool ApiEngine::TStatsWindow::Iteration()
{
	static int Counter = 0;
	Counter++;

	auto& Context = mGetContext();
	auto JobCount = Context.mJobQueue.GetJobCount();

	std::stringstream String;
	String << "Root Directory :" << Context.mRootDirectory << std::endl;
	String << "Counter: " << Counter << std::endl;
	String << "Queued Jobs: " << JobCount << std::endl;
	String << "Line2." << std::endl;
	mLabel->SetValue(String.str());

	return true;
}
