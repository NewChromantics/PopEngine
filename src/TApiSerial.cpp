#include "TApiSerial.h"
#include "SoyLib/src/SoyStream.h"
#include "SoyLib/src/SoyFilesystem.h"


namespace Serial
{
	class TComPort;
	class TFile;
	
	namespace TParity
	{
		enum Type
		{
			None,
			Odd,
			Even
		};
	}
	
	void		EnumPorts(std::function<void(const std::string&)> EnumPort);
}



//	synchronous serial file handle
class Serial::TFile
{
public:
	TFile(const std::string& PortName);	//	just open file
	TFile(const std::string& PortName,size_t BaudRate);	//	open, set rate, and start reading
	~TFile();
	
	void	Read(ArrayBridge<uint8_t>&& Buffer);
	bool	IsOpen()		{	return mFileDescriptor != 0;	}
	void	Close();
	void	SetBaudRate(size_t BaudRate);
	
	std::string	mFilename;
	
private:
#if defined(TARGET_WINDOWS)
	HANDLE	mFileDescriptor = INVALID_HANDLE_VALUE;
#else
	int		mFileDescriptor = 0;
#endif
	size_t	mBaudRate = 0;
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

	Context.BindGlobalFunction<BindFunction::EnumPorts>( EnumPorts, Namespace );

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
	Template.BindFunction<ApiSerial::BindFunction::Open>( Open );
	Template.BindFunction<ApiSerial::BindFunction::Close>( Close );
	Template.BindFunction<ApiSerial::BindFunction::Read>( Read );
}



void TSerialComPortWrapper::Open(Bind::TCallback& Params)
{
	//auto& This = Params.This<TSerialComPortWrapper>();
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
	
	auto Promise = This.mReadPromises.AddPromise( Params.mLocalContext );
	
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
	
	auto SendData = [&](Bind::TLocalContext& LocalContext,Bind::TPromise& Promise)
	{
		if ( mDataAsString )
		{
			auto* cstr = reinterpret_cast<char*>( Data.GetArray() );
			std::string DataString( cstr, Data.GetSize() );
			Promise.Resolve( LocalContext, DataString );
		}
		else
		{
			Promise.Resolve( LocalContext, GetArrayBridge(Data) );
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

	//	on windows try and open each port
	for ( auto Com = 0; Com < 256; Com++ )
	{
		std::stringstream ComPortFilename;
		ComPortFilename << "\\\\.\\COM" << Com;
		auto Filename = ComPortFilename.str();

		try
		{
			TFile ComFile(Filename);
			EnumPort(Filename);
		} 
		catch ( std::exception& e )
		{
			//	failed, not an existing port
		}
	}

#endif
}

#if defined(TARGET_OSX)
#include <fcntl.h>
#include <termios.h>
#endif

Serial::TFile::TFile(const std::string& PortName) :
	mFilename	( PortName )
{
#if defined(TARGET_WINDOWS)
	auto Flags = FILE_ATTRIBUTE_NORMAL;
	//	overlapping is for async read&write capabilities
	//Flags = FILE_FLAG_OVERLAPPED;

	auto OpenMode = GENERIC_READ;	//	|GENERIC_WRITE
	DWORD ShareMode = 0;
	LPSECURITY_ATTRIBUTES Security = nullptr;
	mFileDescriptor = ::CreateFileA(PortName.c_str(),
		OpenMode,
		ShareMode,                           // (share) 0:cannot share the
									 // COM port
		Security,                           // security  (None)
		OPEN_EXISTING,               // creation : open_existing
		Flags,        // we want overlapped operation
		0                            // no templates file for
	);

	if ( mFileDescriptor == INVALID_HANDLE_VALUE )
		Platform::ThrowLastError( std::string("Open COM port ") + mFilename);

#else
	//	gr: when we try and open TTY (write ports) with readonly or readwrite open() blocks...
	auto OpenMode = O_RDONLY;
	//	no control seems to let us open it without blocking
	OpenMode |= O_NOCTTY;
	//	nonblocking, allows us to open write only ports without blocking...
	//	but we need to re-enable blocking reads
	//OpenMode |= O_NDELAY;
	mFileDescriptor = open( PortName.c_str(), OpenMode);
	//	0 is okay it seems.
	if ( mFileDescriptor < 0 )
		Platform::ThrowLastError( std::string("Failed to open ") + PortName );
#endif
}



Serial::TFile::TFile(const std::string& PortName, size_t BaudRate) :
	TFile(PortName)
{
	SetBaudRate(BaudRate);
}



void Serial::TFile::SetBaudRate(size_t BaudRate)
{
	auto Parity = TParity::None;
	const auto ByteSize = 8;
	const auto StopBits = 1;


#if defined(TARGET_WINDOWS)
	COMMTIMEOUTS Timeouts;
	//	A value of zero indicates that interval time-outs are not used.
	Timeouts.ReadIntervalTimeout = 20;

	//	return immediately if both dword
	Timeouts.WriteTotalTimeoutMultiplier = MAXDWORD; // in ms
	Timeouts.WriteTotalTimeoutConstant = MAXDWORD; // in ms
	Timeouts.ReadTotalTimeoutMultiplier = MAXDWORD;
	Timeouts.ReadTotalTimeoutConstant = MAXDWORD; // in ms

	if ( !SetCommTimeouts(mFileDescriptor, &Timeouts) )
		Platform::ThrowLastError("SetCommTimeouts error");


	DCB dcb = {0};
	dcb.DCBlength = sizeof(DCB);

	if (!::GetCommState( mFileDescriptor, &dcb ) )
		Platform::ThrowLastError(std::string("GetCommState ") + mFilename);

	dcb.BaudRate = BaudRate;
	dcb.ByteSize = ByteSize;
	
	switch ( Parity )
	{
		case TParity::None:	dcb.Parity = NOPARITY;	break;
		case TParity::Odd:	dcb.Parity = ODDPARITY;	break;
		case TParity::Even:	dcb.Parity = EVENPARITY;	break;
		default:
			throw Soy::AssertException("Unhandled parity type");
	}

	if ( StopBits == 1 )
		dcb.StopBits  = ONESTOPBIT;
	else if (StopBits == 2 )
		dcb.StopBits  = TWOSTOPBITS;
	else 
		dcb.StopBits  = ONE5STOPBITS;
	
	if (!::SetCommState( mFileDescriptor, &dcb ) )
		Platform::ThrowLastError(std::string("SetCommState ") + mFilename);

	mBaudRate = BaudRate;
#else
	//char port[20] = “/dev/ttyS0”;
	if ( BaudRate != 115200 )
		throw Soy::AssertException("todo: convert baudrate, currently only supporting 115200");
	speed_t baud = B115200;

	auto& fd = mFileDescriptor;

	//	cfgetispeed and cfgetospeed return baud rates
	//	everything else returns 0 on success or -1 on error

	//	set the other settings (in this case, 9600 8N1)
	struct termios settings;
	auto Error = tcgetattr(fd, &settings);
	if ( Error != 0 )
		Platform::ThrowLastError( std::string("tcgetattr failed on ") + mFilename );

	Error = cfsetospeed(&settings, baud);
	if ( Error != 0 )
		Platform::ThrowLastError( std::string("cfsetospeed failed on ") + mFilename );
	Error = cfsetispeed(&settings, baud);
	if ( Error != 0 )
		Platform::ThrowLastError( std::string("cfsetispeed failed on ") + mFilename );

	if ( ByteSize != 8 )
		throw Soy::AssertException("COM Currently only supporting 8 bit");

	//	clear settings
	settings.c_cflag &= ~PARENB;
	settings.c_cflag &= ~CSTOPB;
	settings.c_cflag &= ~CSIZE;
	
	//	apply new settings
	if ( ByteSize != 8 )
		throw Soy::AssertException("Currently must be 8bit");
	if ( ByteSize == 8 )
		settings.c_cflag |= CS8;
	
	//	default 1 stop bit
	if ( StopBits == 2 )
		settings.c_cflag |= CSTOPB;
	else if ( StopBits != 1 )
		throw Soy::AssertException("Unahdnled stop bits");
	
	//	parity defaults off
	if ( Parity == TParity::Odd )
		settings.c_cflag |= PARENB | PARODD;	//	enabled + odd
	if ( Parity == TParity::Even )
		settings.c_cflag |= PARENB;	//	enabled
	
	settings.c_cflag |= (CLOCAL | CREAD);

	//	todo: set these properly
	//settings.c_lflag = ICANON; //	canonical mode
	//settings.c_oflag &= ~OPOST; //	raw output

	//	apply the settings
	Error = tcsetattr(fd, TCSANOW, &settings);
	if ( Error != 0 )
		Platform::ThrowLastError( std::string("tcsetattr failed on ") + mFilename );

	Error = tcflush(fd, TCOFLUSH);
	if ( Error != 0 )
		Platform::ThrowLastError( std::string("tcflush failed on ") + mFilename );

	mBaudRate = BaudRate;
#endif
}

Serial::TFile::~TFile()
{
	try
	{
		Close();
	}
	catch ( std::exception& e )
	{
		std::Debug << e.what() << std::endl;
	}
}

void Serial::TFile::Close()
{
#if defined(TARGET_WINDOWS)
	if ( mFileDescriptor )
	{
		if ( !CloseHandle(mFileDescriptor) )
		{
			Platform::ThrowLastError(std::string("Closing COM port ") + mFilename);
		}
	}
#else
	if ( mFileDescriptor != 0 )
	{
		close( mFileDescriptor );
		mFileDescriptor = 0;
	}
#endif
}

void Serial::TFile::Read(ArrayBridge<uint8_t>&& OutputBuffer)
{
	//	throw if baudrate hasn't been set
	if ( mBaudRate == 0 )
		throw Soy::AssertException("Trying to read COM port but baudrate not set");

	if ( !IsOpen() )
		throw Soy::AssertException("Reading serial file that is closed");

#if defined(TARGET_OSX)
	//	this can read forever without exiting, so instead of looping
	//	just return after one read
	uint8_t ReadBuffer[1024*52];
	
	auto BytesRead = ::read( mFileDescriptor, ReadBuffer, sizeof(ReadBuffer) );
	if ( BytesRead < 0 )
		Platform::ThrowLastError("Reading Serial File failed");
#else

	uint8_t ReadBuffer[1024*52];
	DWORD BytesRead = 0;
	LPOVERLAPPED Overlapped = nullptr;
	auto Success = ::ReadFile(mFileDescriptor, ReadBuffer, sizeof(ReadBuffer), &BytesRead, Overlapped);
	if ( !Success )
		Platform::ThrowLastError(std::string("COM port ReadFile ") + mFilename);
	if ( BytesRead == 0 )
		return;
#endif

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
	mFile.reset();
	//	gr: may need to interrupt any blocking reading
	WaitToFinish();
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
