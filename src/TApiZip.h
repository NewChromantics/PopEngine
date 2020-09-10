#pragma once
#include "TBind.h"


namespace ApiZip
{
	void	Bind(Bind::TContext& Context);

	class TArchiveWrapper;
	DECLARE_BIND_TYPENAME(Archive);
}

class TZipFile;


class ApiZip::TArchiveWrapper : public Bind::TObjectWrapper<BindType::Archive, TZipFile>
{
public:
	TArchiveWrapper(Bind::TContext& Context) :
		TObjectWrapper	( Context )
	{
	}
	~TArchiveWrapper();
	
	static void					CreateTemplate(Bind::TTemplate& Template);
	virtual void 				Construct(Bind::TCallback& Params) override;

	//	asynchronously add file
	void						AddFile(Bind::TCallback& Params);
	void						Close(Bind::TCallback& Params);
	
protected:
	void						OnWriteFinished(const std::string& Error);

public:
	std::shared_ptr<TZipFile>&	mZipFile = mObject;

	//	currently only supporting one write at a time, this could turn into a queue though
	std::shared_ptr<SoyThread>		mWriteThread;
	std::shared_ptr<Bind::TPromise>	mWritePromise;
};


