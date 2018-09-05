#include "TApiFile.h"
#include <SoyFilesystem.h>

using namespace v8;


const char File_TypeName[] = "File";

const char GetString_FunctionName[] = "GetString";
const char GetBytes_FunctionName[] = "GetBytes";



void ApiFile::Bind(TV8Container& Container)
{
	Container.BindObjectType<TFileWrapper>();
}

static v8::Local<v8::FunctionTemplate>	CreateTemplate(TV8Container& Container);

virtual void 							Construct(const v8::CallbackInfo& Arguments) override;

static v8::Local<v8::Value>				GetString(const v8::CallbackInfo& Arguments);
static v8::Local<v8::Value>				GetBytes(const v8::CallbackInfo& Arguments);

const std::string						GetFilename()		{	return mFileHandle.mFileName;	}

