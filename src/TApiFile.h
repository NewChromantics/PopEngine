#pragma once
#include "TV8Container.h"
#include "TV8ObjectWrapper.h"


namespace ApiFile
{
	void	Bind(TV8Container& Container);
}


//	file watcher
class TFileHandle
{
public:
	TFileHandle(const std::string& Filename,std::function<void()> OnChanged) :
		mFilename		( Filename ),
		mOnChanged		( OnChanged )
	{
	}
	
	void					GetFileContents(std::ostream& Contents);
	void					GetFileContents(ArrayBridge<uint8_t>& Contents);
	void					GetFileContents(ArrayBridge<uint8_t>&& Contents)	{	GetFileContents( Contents );	}

	std::string				mFilename;
	std::function<void()>	mOnChanged;
};


//	an image is a generic accessor for pixels, opengl textures, etc etc
extern const char File_TypeName[];
class TFileWrapper : public TObjectWrapper<File_TypeName,TFileHandle>
{
public:
	TFileWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This )
	{
	}
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

	virtual void 							Construct(const v8::CallbackInfo& Arguments) override;

	static v8::Local<v8::Value>				GetString(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				GetBytes(const v8::CallbackInfo& Arguments);

	void									OnFileChanged();
	const std::string						GetFilename()		{	return mFileHandle->mFilename;	}
	TFileHandle&							GetFileHandle()		{	return *mFileHandle;	}

protected:
	std::shared_ptr<TFileHandle>&			mFileHandle = mObject;
};

