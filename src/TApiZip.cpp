#include "TApiZip.h"
#include "Zip/src/zip.h"


namespace ApiZip
{
	const char Namespace[] = "Pop.Zip";

	DEFINE_BIND_TYPENAME(Archive);
	DEFINE_BIND_FUNCTIONNAME(AddFile);
	DEFINE_BIND_FUNCTIONNAME(Close);
}

void ApiZip::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);
	Context.BindObjectType<TArchiveWrapper>(Namespace);
}


class TZipFile
{
public:
	static void		IsOkay(int Error, const char* Context);
	static void		IsOkay(int Error, const std::string& Context) { IsOkay(Error, Context.c_str()); }
	static void		IsValidZipFilename(const std::string& Filename);

public:
	TZipFile(const std::string& Filename);
	~TZipFile();

	void			AddFile(const std::string& Filename, const std::string& ZipFilename);
	
	std::mutex		mLock;
	struct zip_t*	mZip = nullptr;
};



void ApiZip::TArchiveWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<BindFunction::AddFile>(&TArchiveWrapper::AddFile);
	Template.BindFunction<BindFunction::Close>(&TArchiveWrapper::Close);
}


void ApiZip::TArchiveWrapper::Construct(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);

	//	if this exists, it should throw if not a valid file
	//	if it doesn't exist, TZipFile should know we're creating an archive
	mZipFile.reset(new TZipFile(Filename));
}

ApiZip::TArchiveWrapper::~TArchiveWrapper()
{
	//	worry here that JS cleans up this, before finishing resolving a queued
	//	promise resolve, which would be queued after this cleanup...
	mWriteThread.reset();
}


void ApiZip::TArchiveWrapper::Close(Bind::TCallback& Params)
{
	if (mWriteThread)
		throw Soy::AssertException("Zip.Archive is still writing");
	
	mZipFile.reset();
}

void ApiZip::TArchiveWrapper::AddFile(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);
	auto ZipFilename = Params.GetArgumentString(1);

	if (mWriteThread)
		throw Soy::AssertException("Zip.Archive currently only supports one async operation at a time");

	if (mWritePromise)
		throw Soy::AssertException("Zip.Archive has a dangling WritePromise");

	//	make a promise to return
	mWritePromise = Params.mLocalContext.mGlobalContext.CreatePromisePtr(Params.mLocalContext, __PRETTY_FUNCTION__);
	Params.Return(*mWritePromise);

	//	spin up thread to write, then flush when done
	auto WriteFunc = [this, Filename, ZipFilename]()->bool
	{
		try
		{
			mZipFile->AddFile(Filename, ZipFilename);
			this->OnWriteFinished(std::string());
		}
		catch (std::exception& e)
		{
			this->OnWriteFinished(e.what());
		}
		return false;
	};
	mWriteThread.reset(new SoyThreadLambda(__PRETTY_FUNCTION__, WriteFunc));
}

void ApiZip::TArchiveWrapper::OnWriteFinished(const std::string& Error)
{
	if (!mWritePromise)
		throw Soy::AssertException("OnWriteFinished but missing a promise");
		
	//	gr: we can't currently reset the thread whilst we're being called from it, (race condition)
	//		so reset it in the JS callback (erk)

	//	flush the promise
	auto Flush = [this, Error](Bind::TLocalContext& Context)
	{
		auto Promise = mWritePromise;
		//	gr: clear variables now, as some javascript (jsc) runs 
		//	and might queue another write BEFORE we reach the end of
		//	this lambda
		mWritePromise.reset();
		mWriteThread.reset();
		if (Error.length())
		{
			Promise->Reject(Context, Error);
		}
		else
		{
			//	should we send some meta?
			Promise->ResolveUndefined(Context);
		}		
	};
	auto& Context = mWritePromise->GetContext();
	Context.Queue(Flush);
	
}


void TZipFile::IsOkay(int Error, const char* Context)
{
	if (Error == 0)
		return;

	throw Soy::AssertException(std::string("Zip error in ") + Context);
}

void TZipFile::IsValidZipFilename(const std::string& Filename)
{
	auto Throw = [&](const char* Reason)
	{
		std::stringstream Error;
		Error << "Invalid filename for zip [" << Filename << "] " << Reason;
		throw Soy::AssertException(Error);
	};

	if (Filename.length() == 0)
		Throw("Zero length");

	for (auto i = 0; i < Filename.length(); i++)
	{
		auto Char = Filename[i];
		if (Char == '\\')
			Throw("Path seperators must be /backslashes, not \\forwardslash");

		//	check other chars
	}

	if (Filename[0] == '/')
		Throw("Cannot start with slash");
}


TZipFile::TZipFile(const std::string& Filename)
{
	//	'a' lets us append
	mZip = zip_open(Filename.c_str(), ZIP_DEFAULT_COMPRESSION_LEVEL, 'w');
	if ( !mZip )
		throw Soy::AssertException(std::string("Failed to open zip file ") + Filename);
}

TZipFile::~TZipFile()
{
	//	wait for operations to finish
	std::lock_guard<std::mutex> Lock(mLock);
	zip_close(mZip);
	mZip = nullptr;
}

void TZipFile::AddFile(const std::string& Filename, const std::string & ZipFilename)
{
	std::lock_guard<std::mutex> Lock(mLock);

	if (!mZip)
		throw Soy::AssertException("Zip is not open");

	//	check zip filename
	//	this should be in the zip lib!
	IsValidZipFilename(ZipFilename);

	auto Error = zip_entry_open(mZip, ZipFilename.c_str());
	IsOkay(Error, "zip_entry_open");

	//	open 
	Error = zip_entry_fwrite(mZip, Filename.c_str());
	IsOkay(Error, "zip_entry_fwrite");

	Error = zip_entry_close(mZip);
	IsOkay(Error, "zip_entry_close");
}

