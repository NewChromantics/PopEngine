#pragma once
#include "TBind.h"
#include <SoyDirectx.h>

namespace Win32
{
	class TOpenglContext;
}

namespace Directx
{
	class TContext;
}

namespace ApiDirectx11
{
	void	Bind(Bind::TContext& Context);

	class TContextWrapper;
	
	DECLARE_BIND_TYPENAME(Context);
}

 

class ApiDirectx11::TContextWrapper : public Bind::TObjectWrapper<BindType::Context,Directx::TContext>
{
public:
	TContextWrapper(Bind::TContext& Context) :
		TObjectWrapper		( Context )
	{
	}
	~TContextWrapper();
	
	
	static void			CreateTemplate(Bind::TTemplate& Template);
	virtual void 		Construct(Bind::TCallback& Arguments) override;

	//	gr: this will need to pass in some camera? meta for openxr, and a rendertarget
	//		but, long term, we wanna change this to not be immediate and instead get render commands
	void				OnRender(std::function<void()> LockContext);

	//	temp hack for openxr
	std::shared_ptr<Win32::TOpenglContext>		GetWin32OpenglContext() { return nullptr; }
	std::shared_ptr < Directx::TContext>		GetDirectxContext() {	return mContext;}
	
public:
	std::shared_ptr<Directx::TContext>&	mContext = mObject;
};



