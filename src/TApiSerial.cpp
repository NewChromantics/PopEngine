#include "TApiSerial.h"
#include "SoyLib/src/SoyStream.h"
#include "SoyLib/src/SoyFilesystem.h"


namespace Serial
{
	class TComPort;
	class TFile;
	
	void		EnumPorts(std::function<void(const std::string&)> EnumPort);
}



//	synchronous serial file handle
class Serial::TFile
{
public:
	TFile(const std::string& PortName,size_t BaudRate);
	~TFile();
	
	void	Read(ArrayBridge<uint8_t>&& Buffer);
	bool	IsOpen()		{	return mFileDescriptor != 0;	}
	void	Close();
	
	std::string	mFilename;
	
private:
	int		mFileDescriptor = 0;
};


class Serial::TComPort : SoyWorkerThread
{
public:
	TComPort(const std::string& PortName,size_t BaudRate);
	~TComPort();
	
	bool		IsOpen();
	void		PopData(ArrayBridge<uint8_t>&& Data);
	
	std::function<void()>	mOnDataRecieved;
	std::function<void()>	mOnClosed;

private:
	virtual bool	Iteration() override;
	
private:
	std::shared_ptr<TFile>	mFile;
	TStreamBuffer			mRecvBuffer;
};






namespace ApiSerial
{
	const char Namespace[] = "Pop.Serial";

	DEFINE_BIND_FUNCTIONNAME(EnumPorts);

	DEFINE_BIND_TYPENAME(ComPort);
	DEFINE_BIND_FUNCTIONNAME(Read);
	DEFINE_BIND_FUNCTIONNAME(Open);
	DEFINE_BIND_FUNCTIONNAME(Close);

	void	EnumPorts(Bind::TCallback& Params);
	void	OnPortsChanged(Bind::TCallback& Params);

}


void ApiSerial::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<EnumPorts_FunctionName>( EnumPorts, Namespace );

	Context.BindObjectType<TSerialComPortWrapper>( Namespace );
}


void ApiSerial::EnumPorts(Bind::TCallback& Params)
{
	Array<std::string> PortNames;
	auto AddPort = [&](const std::string& PortName)
	{
		PortNames.PushBack(PortName);
	};
	Serial::EnumPorts(AddPort);

	Params.Return( GetArrayBridge(PortNames) );
}



void TSerialComPortWrapper::Construct(Bind::TCallback& Params)
{
	auto PortName = Params.GetArgumentString(0);
	auto BaudRate = Params.GetArgumentInt(1);
	
	if ( !Params.IsArgumentUndefined(2) )
		mDataAsString = Params.GetArgumentBool(2);
	
	//	auto open
	mComPort.reset( new Serial::TComPort(PortName,BaudRate) );
	
	mComPort->mOnDataRecieved = [this]()
	{
		this->OnDataReceived();
	};
	
	mComPort->mOnClosed = [this]()
	{
		this->OnDataReceived();
	};
}


void TSerialComPortWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiSerial::Open_FunctionName>( Open );
	Template.BindFunction<ApiSerial::Close_FunctionName>( Close );
	Template.BindFunction<ApiSerial::Read_FunctionName>( Read );
}



void TSerialComPortWrapper::Open(Bind::TCallback& Params)
{
	auto& This = Params.This<TSerialComPortWrapper>();
	//	close and open old com
	throw Soy::AssertException("todo open");
}


void TSerialComPortWrapper::Close(Bind::TCallback& Params)
{
	auto& This = Params.This<TSerialComPortWrapper>();
	This.mComPort.reset();
	
	//	flush promises (in case it's not done automatically)
	This.OnDataReceived();
}


void TSerialComPortWrapper::Read(Bind::TCallback& Params)
{
	auto& This = Params.This<TSerialComPortWrapper>();
	
	auto Promise = This.mReadPromises.AddPromise(Params.mContext);
	
	//	auto flush if we have data, or if the port is closed
	This.OnDataReceived();
	
	Params.Return(Promise);
}



void TSerialComPortWrapper::OnDataReceived()
{
	//	flush data first,
	//	then fail if closed
	//	fail all read promises if closed
	if ( !mComPort )
	{
		mReadPromises.Reject("COM port is closed");
		return;
	}
	
	//	nothing waiting for data, let com port keep buffering
	if ( !mReadPromises.HasPromises() )
		return;
	
	//	flush any data
	//	todo: if data is coming out as a string, split at zeros/termniators
	Array<uint8_t> Data;
	mComPort->PopData(GetArrayBridge(Data));
	
	//	if there's no data, don't do anything, unless port is closed in which case throw
	if ( Data.IsEmpty() )
	{
		//	todo: get last error
		if ( !mComPort->IsOpen() )
			mReadPromises.Reject("COM port is closed");
		return;
	}
	
	auto SendData = [&](Bind::TPromise& Promise)
	{
		if ( mDataAsString )
		{
			auto* cstr = reinterpret_cast<char*>( Data.GetArray() );
			std::string DataString( cstr, Data.GetSize() );
			Promise.Resolve( DataString );
		}
		else
		{
			Promise.Resolve( GetArrayBridge(Data) );
		}
	};
	mReadPromises.Flush(SendData);
}




void Serial::EnumPorts(std::function<void(const std::string&)> EnumPort)
{
#if defined(TARGET_OSX)
	//	get a directory listing of /dev/
	//	then.. filter
	//	all devices are cu. and tty. ? everything else is virtual?
	auto OnFilename = [&](const std::string& OrigFilename,SoyPathType::Type FileType)
	{
		//	on osx at least, they're special files!
		if ( FileType != SoyPathType::Special )
			return true;
		
		auto Filename = OrigFilename;
		
		if ( Soy::StringTrimLeft(Filename, "/dev/cu.", true ) )
		{
			EnumPort(OrigFilename);
		}
		else if ( Soy::StringTrimLeft(Filename, "/dev/tty.", true ) )
		{
			EnumPort(OrigFilename);
		}
		else
		{
			//	doesn't pass filter
			//std::Debug << "Ignoring device " << Filename << std::endl;
		}
		return true;
	};
	Platform::EnumDirectory("/dev/", OnFilename );
#else
	throw Soy::AssertException("Serial::EnumPorts not implemented on this platform");
#endif
}

#include <fcntl.h>
#include <termios.h>
Serial::TFile::TFile(const std::string& PortName,size_t BaudRate) :
	mFilename	( PortName )
{
	//char port[20] = “/dev/ttyS0”;
	if ( BaudRate != 115200 )
		throw Soy::AssertException("todo: convert baudrate, currently only supporting 115200");
	speed_t baud = B115200;
	
	//	0 is okay it seems.
	//auto OpenMode = O_RDWR;
	//	no control seems to let us open it without blocking
	auto OpenMode = O_RDONLY| O_NOCTTY;
	//	nonblocking
	//OpenMode |= O_NDELAY;
	mFileDescriptor = open( PortName.c_str(), OpenMode);
	if ( mFileDescriptor < 0 )
		Platform::ThrowLastError( std::string("Failed to open ") + PortName );

	auto& fd = mFileDescriptor;
	
	try
	{
		//	cfgetispeed and cfgetospeed return baud rates
		//	everything else returns 0 on success or -1 on error
		
		//	set the other settings (in this case, 9600 8N1)
		struct termios settings;
		auto Error = tcgetattr(fd, &settings);
		if ( Error != 0 )
			Platform::ThrowLastError( std::string("tcgetattr failed on ") + PortName );
	
		Error = cfsetospeed(&settings, baud);
		if ( Error != 0 )
			Platform::ThrowLastError( std::string("cfsetospeed failed on ") + PortName );
		Error = cfsetispeed(&settings, baud);
		if ( Error != 0 )
			Platform::ThrowLastError( std::string("cfsetispeed failed on ") + PortName );

		settings.c_cflag &= ~PARENB;
		settings.c_cflag &= ~CSTOPB;
		settings.c_cflag &= ~CSIZE;
		settings.c_cflag |= CS8;
		settings.c_cflag |= (CLOCAL | CREAD);
		/*
		settings.c_cflag &= ~PARENB; //	no parity
		settings.c_cflag &= ~CSTOPB; //	1 stop bit
		
		settings.c_cflag &= ~CSIZE;
		settings.c_cflag |= CS8 | CLOCAL; //	8 bits
		settings.c_lflag = ICANON; //	canonical mode
		settings.c_oflag &= ~OPOST; //	raw output
		*/
		//	apply the settings
		Error = tcsetattr(fd, TCSANOW, &settings);
		if ( Error != 0 )
			Platform::ThrowLastError( std::string("tcsetattr failed on ") + PortName );
		
		Error = tcflush(fd, TCOFLUSH);
		if ( Error != 0 )
			Platform::ThrowLastError( std::string("tcflush failed on ") + PortName );
	}
	catch(...)
	{
		//	cleanup before throwing
		Close();
		throw;
	}
}

Serial::TFile::~TFile()
{
	Close();
}

void Serial::TFile::Close()
{
	if ( mFileDescriptor != 0 )
	{
		close( mFileDescriptor );
		mFileDescriptor = 0;
	}
}

void Serial::TFile::Read(ArrayBridge<uint8_t>&& OutputBuffer)
{
	if ( !IsOpen() )
		throw Soy::AssertException("Reading serial file that is closed");

	//	this can read forever without exiting, so instead of looping
	//	just return after one read
	uint8_t ReadBuffer[1024*52];
	
	auto BytesRead = ::read( mFileDescriptor, ReadBuffer, sizeof(ReadBuffer) );
	if ( BytesRead < 0 )
		Platform::ThrowLastError("Reading Serial File failed");

	auto ReadArray = GetRemoteArray( ReadBuffer, BytesRead, BytesRead );
	OutputBuffer.PushBackArray(ReadArray);
}

	

//	todo: nowait, as the file should block the thread
Serial::TComPort::TComPort(const std::string& PortName,size_t BaudRate) :
	SoyWorkerThread	( std::string("Com Port ") + PortName, SoyWorkerWaitMode::NoWait )
{
	mFile.reset( new TFile(PortName,BaudRate) );
	Start();
}

Serial::TComPort::~TComPort()
{
	//	gr: may need to interrupt any blocking reading
	Stop();
}

bool Serial::TComPort::IsOpen()
{
	if ( !mFile )
		return false;
	
	return mFile->IsOpen();
}

void Serial::TComPort::PopData(ArrayBridge<uint8_t>&& Data)
{
	mRecvBuffer.Pop( mRecvBuffer.GetBufferedSize(), Data );
}

bool Serial::TComPort::Iteration()
{
	if ( !mFile )
		return false;
	
	//	save for thread safety
	auto pFile = mFile;
	auto& File = *pFile;
	
	if ( !File.IsOpen() )
		return false;
	
	//	read from file into buffer
	try
	{
		Array<uint8_t> NewData;
		File.Read( GetArrayBridge(NewData) );
		mRecvBuffer.Push( GetArrayBridge(NewData) );
	
		//	notify if we have new data
		if ( !NewData.IsEmpty() )
		{
			if ( mOnDataRecieved )
				mOnDataRecieved();
		}
		
	}
	catch(std::exception& e)
	{
		//	close the file on error!
		std::Debug << File.mFilename << ": " << e.what() << " closing file." << std::endl;
		mFile.reset();
		if ( mOnClosed )
			mOnClosed();
		return false;
	}
	
	return true;
}
