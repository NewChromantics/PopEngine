#pragma once
#include "TV8Container.h"

class SoyPixels;
class SoyPixelsImpl;

namespace ApiCommon
{
	void	Bind(TV8Container& Container);
}

namespace Opengl
{
	class TTexture;
	class TContext;
}


template<const char* TYPENAME,class TYPE>
class TObjectWrapper
{
public:
	TObjectWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>());

	static std::string						GetObjectTypeName()	{	return TYPENAME;	}

	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	static v8::Local<v8::Value>				Alloc(const v8::CallbackInfo& Arguments);
	
	v8::Local<v8::Object>					GetHandle() const;
	
protected:
	static void								OnFree(const v8::WeakCallbackInfo<TObjectWrapper>& data);
	
protected:
	v8::Persist<v8::Object>				mHandle;
	TV8Container&						mContainer;
	std::shared_ptr<TYPE>				mObject;
	
protected:
};


//	we can create this outside of a context, but maybe needs to be in isolate scope?
template<const char* TYPENAME,class TYPE>
TObjectWrapper<TYPENAME,TYPE>::TObjectWrapper(TV8Container& Container,v8::Local<v8::Object> This) :
	mContainer	( Container )
{
	if ( This.IsEmpty() )
	{
		This = Container.CreateObjectInstance( TYPENAME );
	}
	
	auto* Isolate = &Container.GetIsolate();
	This->SetInternalField( 0, v8::External::New( Isolate, this ) );

	mHandle.Reset( Isolate, This );

	//	make it weak
	//	https://itnext.io/v8-wrapped-objects-lifecycle-42272de712e0
	mHandle.SetWeak( this, OnFree, v8::WeakCallbackType::kInternalFields );
}


template<const char* TYPENAME,class TYPE>
inline void TObjectWrapper<TYPENAME,TYPE>::OnFree(const v8::WeakCallbackInfo<TObjectWrapper>& WeakCallbackData)
{
	std::Debug << "Freeing " << TYPENAME << "..." << std::endl;
	auto* ObjectWrapper = WeakCallbackData.GetParameter();
	delete ObjectWrapper;
}

template<const char* TYPENAME,class TYPE>
inline v8::Local<v8::Object> TObjectWrapper<TYPENAME,TYPE>::GetHandle() const
{
	auto* Isolate = &this->mContainer.GetIsolate();
	auto LocalHandle = v8::Local<v8::Object>::New( Isolate, mHandle );
	return LocalHandle;
}



//	an image is a generic accessor for pixels, opengl textures, etc etc
extern const char Image_TypeName[];
class TImageWrapper : public TObjectWrapper<Image_TypeName,SoyPixels>
{
public:
	TImageWrapper(TV8Container& Container,v8::Local<v8::Object> This=v8::Local<v8::Object>()) :
		TObjectWrapper			( Container, This ),
		mLinearFilter			( false ),
		mRepeating				( false ),
		mPixelsVersion			( 0 ),
		mOpenglTextureVersion	( 0 ),
		mPixels					( mObject )
	{
	}
	
	static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);
	
	static void								Constructor(const v8::FunctionCallbackInfo<v8::Value>& Arguments);
	static v8::Local<v8::Value>				Alloc(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				Flip(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				LoadFile(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				GetWidth(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				GetHeight(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				GetRgba8(const v8::CallbackInfo& Arguments);
	static v8::Local<v8::Value>				SetLinearFilter(const v8::CallbackInfo& Arguments);

	static TImageWrapper&					Get(v8::Local<v8::Value> Value)	{	return v8::GetInternalFieldObject<TImageWrapper>( Value, 0 );	}
	
	void									DoLoadFile(const std::string& Filename);
	void									DoSetLinearFilter(bool LinearFilter);
	void									GetTexture(std::function<void()> OnTextureLoaded,std::function<void(const std::string&)> OnError);
	const Opengl::TTexture&					GetTexture();
	SoyPixels&								GetPixels();

	//	we consider version 0 uninitisalised
	size_t									GetLatestVersion() const;
	void									OnOpenglTextureChanged();
	void									ReadOpenglPixels();
	void									SetPixels(const SoyPixelsImpl& NewPixels);
	void									SetPixels(std::shared_ptr<SoyPixels> NewPixels);

protected:
	std::shared_ptr<SoyPixels>&			mPixels = mObject;
	size_t								mPixelsVersion;			//	opengl texture changed
	std::shared_ptr<Opengl::TTexture>	mOpenglTexture;
	size_t								mOpenglTextureVersion;	//	pixels changed
	
	//	texture options
	bool								mLinearFilter;
	bool								mRepeating;
};

