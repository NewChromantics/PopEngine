#include "TApiSerial.h"
#include "SoyLib/src/SoyStream.h"
#include "SoyLib/src/SoyFilesystem.h"


namespace Serial
{
	class TComPort;
	class TFile;
	
	void		EnumPorts(std::function<void(std::string&)> EnumPort);
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
	
private:
	int		mFileDescriptor = 0;
};


class Serial::TComPort : SoyWorkerThread
{
public:
	TComPort(const std::string& PortName,size_t BaudRate);
	~TComPort();
	
	bool		IsOpen();
	
	std::function<void()>	mOnDataRecieved;
	
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
	auto AddPort = [&](std::string& PortName)
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
	
	//	auto open
	mComPort.reset( new Serial::TComPort(PortName,BaudRate) );
	
	mComPort->mOnDataRecieved = [this]()
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
		
	}
}




void Serial::EnumPorts(std::function<void(std::string&)> EnumPort)
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
		Soy::StringTrimLeft(Filename,"/dev/",true);
							
		if ( Soy::StringTrimLeft(Filename, "cu.", true ) )
		{
			EnumPort(Filename);
		}
		else if ( Soy::StringTrimLeft(Filename, "tty.", true ) )
		{
			EnumPort(Filename);
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
Serial::TFile::TFile(const std::string& PortName,size_t BaudRate)
{
	//char port[20] = “/dev/ttyS0”;
	if ( BaudRate != 115200 )
		throw Soy::AssertException("todo: convert baudrate, currently only supporting 115200");
	speed_t baud = B115200;
	
	mFileDescriptor = open( PortName.c_str(), O_RDWR);
	if ( mFileDescriptor == -1 )
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
	
		Error = cfsetospeed(&settings, baud); /* baud rate */
		if ( Error != 0 )
			Platform::ThrowLastError( std::string("cfsetospeed failed on ") + PortName );
		
		settings.c_cflag &= ~PARENB; /* no parity */
		settings.c_cflag &= ~CSTOPB; /* 1 stop bit */
		settings.c_cflag &= ~CSIZE;
		settings.c_cflag |= CS8 | CLOCAL; /* 8 bits */
		settings.c_lflag = ICANON; /* canonical mode */
		settings.c_oflag &= ~OPOST; /* raw output */
		
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
	if ( IsOpen() )
		throw Soy::AssertException("Reading serial file that is closed");

	//	we'd like to block, so keep small for small strings
	uint8_t ReadBuffer[1024];
	
	while ( true )
	{
		auto BytesRead = ::read( mFileDescriptor, ReadBuffer, sizeof(ReadBuffer) );
		if ( BytesRead < 0 )
			Platform::ThrowLastError("Reading Serial File failed");
	
		//	no data pending
		if ( BytesRead == 0 )
			break;
		
		for ( auto i=0;	i<BytesRead;	i++ )
			OutputBuffer.PushBackReinterpret( ReadBuffer, BytesRead );
	}
}

	

//	todo: nowait, as the file should block the thread
Serial::TComPort::TComPort(const std::string& PortName,size_t BaudRate) :
	SoyWorkerThread	( std::string("Com Port ") + PortName, SoyWorkerWaitMode::Sleep )
{
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
	Array<uint8_t> NewData;
	File.Read( GetArrayBridge(NewData) );
	mRecvBuffer.Push( GetArrayBridge(NewData) );
	
	//	notify if we have new data
	if ( !NewData.IsEmpty() )
	{
		if ( mOnDataRecieved )
			mOnDataRecieved();
	}
	
	return true;
}
