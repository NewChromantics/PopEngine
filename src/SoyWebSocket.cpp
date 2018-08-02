#include "SoyWebSocket.h"
#include <regex>
#include <SoyDebug.h>
#include <string>
#include <SoyString.h>
#include <smallsha1/sha1.h>
#include <SoyApp.h>
#include <SoyEnum.h>
#include <SoyBase64.h>



const std::string WebsocketMessageCommand = "WebsocketMessage";
const std::string WebsocketMaskKeyParam = "_ws_maskkey";
const std::string WebSocketFinParam = "_ws_fin";
const std::string WebsocketNextMessageParam = "_ws_nextmessage";
const std::string WebsocketPayloadParam = "_ws_payload";
const std::string WebsocketIsContinuationParam = "_ws_iscontinuation";


WebSocket::THandshakeMeta::THandshakeMeta() :
	mIsWebSocketUpgrade	( true )
{
}

std::string WebSocket::THandshakeMeta::GetReplyKey() const
{
	//	http://en.wikipedia.org/wiki/WebSocket
	//	The client sends a Sec-WebSocket-Key which is a random value that has been base64 encoded.
	//	To form a response, the magic string 258EAFA5-E914-47DA-95CA-C5AB0DC85B11 is appended to this (undecoded) key.
	//	The resulting string is then hashed with SHA-1, then base64 encoded.
	//	Finally, the resulting reply occurs in the header Sec-WebSocket-Accept.
	
	//	gr: RFC says this key IS NOT decoded...
	//	http://tools.ietf.org/html/rfc6455
	
	
	std::string key = mWebSocketKey;
	//key.append(websocketpp::processor::constants::handshake_guid);
	key.append( "258EAFA5-E914-47DA-95CA-C5AB0DC85B11");

	//	apply hashing
	BufferArray<char,20> Hash(20);
	sha1::calc( key.c_str(), size_cast<int>(key.length()), reinterpret_cast<uint8*>(Hash.GetArray()) );
	
	//	encode to base 64
	auto HashBridge = GetArrayBridge( Hash );
	Array<char> Encoded;
	auto EncodedBridge = GetArrayBridge( Encoded );
	Base64::Encode(EncodedBridge, HashBridge);
	std::string EncodedKey = Soy::ArrayToString( EncodedBridge );
	return EncodedKey;
}



bool WebSocket::TRequestProtocol::ParseSpecificHeader(const std::string& Key,const std::string& Value)
{
	//	extract web-socket special headers
	if ( Soy::StringMatches(Key,"Upgrade", false ) )
	{
		if ( Soy::StringMatches(Value,"websocket", false ) )
		{
			mHandshake.mIsWebSocketUpgrade = true;
			return true;
		}
	}
	
	if ( Soy::StringMatches(Key,"Sec-WebSocket-Key", false ) )
	{
		mHandshake.mWebSocketKey = Value;
		return true;
	}
	
	if ( Soy::StringMatches(Key,"Sec-WebSocket-Protocol", false ) )
	{
		mHandshake.mProtocol = Value;
		return true;
	}

	if ( Soy::StringMatches(Key,"Sec-WebSocket-Version", false ) )
	{
		mHandshake.mVersion = Value;
		return true;
	}
	
	return Http::TRequestProtocol::ParseSpecificHeader(Key,Value);
}




WebSocket::THandshakeResponseProtocol::THandshakeResponseProtocol(const THandshakeMeta& Handshake)
{
	//	setup http response for websocket acceptance
	
	//	add http headers we need to reply with
	if ( !Handshake.mProtocol.empty() )
		mHeaders.insert( {"Sec-WebSocket-Protocol", Handshake.mProtocol} );
	
	if ( Handshake.mIsWebSocketUpgrade )
	{
		mHeaders.insert( {"Sec-WebSocket-Accept", Handshake.GetReplyKey() } );
		mHeaders.insert( {"Upgrade", "websocket"} );
		mHeaders.insert( {"Connection", "Upgrade"} );
	}
	
	//	gr: need content length or chrome never displays...
	//	gr: ^^ I think this was related to png, let base class deal with that
	//Http.PushHeader( "Content-Length", std::stringstream() << DefaultParam.GetDataSize() );
	//Http.PushHeader( SoyHttpHeaderElement("Content-Type", SoyHttpHeader::FormatToHttpContentType(Reply.mData.mFormat).c_str() ) );
	
	mResponseCode = Http::Response_SwitchingProtocols;
}


/*

bool SoyWebSocketHeader::PushRawHeader(const std::string& Header)
{
	if ( !SoyHttpHeader::PushRawHeader( Header ) )
		return false;
	
	//	extract web-socket special headers
	if ( Soy::StringBeginsWith( Header, "Upgrade: websocket", true ) )
	{
		mIsWebSocketUpgrade = true;
		return true;
	}
	
	if ( Soy::StringBeginsWith( Header, "Sec-WebSocket-Key: ", true ) )
	{
		mWebSocketKey = Header.substr( std::string("Sec-WebSocket-Key: ").length() );
		return true;
	}
	
	return true;
}


bool TProtocolWebSocketImpl::FixParamFormat(TJobParam& Param,std::stringstream& Error)
{
	//	need to filter with subformats
	auto SubProtocol = const_cast<TProtocolWebSocketImpl&>(*this).GetSubProtocol();
	if ( SubProtocol )
	{
		if ( !SubProtocol->FixParamFormat( Param, Error ) )
			return false;
	}
	
	return true;
}
*/

/*
 
 TProtocolState::Type Http::TCommonProtocol::Decode(TStreamBuffer& Buffer)
 {
	auto HeaderState = DecodeHeaders(Buffer);
	if ( HeaderState != TProtocolState::Finished )
 return HeaderState;
	
	//	read data
	if ( mContentLength == 0 )
 return TProtocolState::Finished;
	
	if ( !Buffer.Pop( mContentLength, GetArrayBridge(mContent) ) )
 return TProtocolState::Waiting;
	
	if ( !mKeepAlive )
 return TProtocolState::Disconnect;
	
	return TProtocolState::Finished;
 }

TDecodeResult::Type TWebSocketClient::DecodeHandshake(TJob& Job,TStreamBuffer& Stream)
{
	//	keep reading data until we find the end of the http header block \n\n
	//	gr: allow grab a header at a time?
	std::string HeaderString;
	if ( !Stream.Pop( "\r\n\r\n", HeaderString, true ) )
		return TDecodeResult::Waiting;
	
	//	get the client meta
	
	//	now decode the headers
	SoyWebSocketHeader Header;
	for ( int i=0;	i<HeaderString.length();	i++ )
		Header.mData.PushBack( HeaderString[i] );
	if ( !Header.PopHeader( Header.mData ) )
		return TDecodeResult::Error;
	
	
	//	if a protocol was specified we need to store it
	auto* ProtocolHeader = Header.GetHeader("Sec-WebSocket-Protocol");
	if ( ProtocolHeader )
		mProtocol = *ProtocolHeader;
	auto* VersionHeader = Header.GetHeader("Sec-WebSocket-Version");
	if ( VersionHeader )
		mVersion = *VersionHeader;
	
	//	success parsing!
	mHasHandshaked = true;
	
	//	force an immediate reply using bounce
	Job.mParams.mCommand = "Handshake";
	
	//	add http headers we need to reply with
	if ( !mProtocol.empty() )
		Job.mParams.AddParam("Sec-WebSocket-Protocol", mProtocol );
	
	if ( Header.mIsWebSocketUpgrade )
	{
		Job.mParams.AddParam( "Sec-WebSocket-Accept", Header.GetReplyKey() );
		Job.mParams.AddParam( "Upgrade", "websocket" );
		Job.mParams.AddParam( "Connection", "Upgrade" );
	}
	
	
	//	need to force a reply here...
	return TDecodeResult::Bounce;
}

void PushParams(SoyLineFeedHeader& Http,const Array<TJobParam>& Params)
{
	for ( int p=0;	p<Params.GetSize();	p++ )
	{
		auto Key = Params[p].GetKey();
		auto Value = Params[p].Decode<std::string>();
		SoyHttpHeaderElement Element( Key.c_str(), Value.c_str() );
		Http.PushHeader( Element );
	}
}

bool TWebSocketClient::EncodeHandshake(const TJobReply& Reply,Array<char>& Output)
{
	SoyWebSocketHeader Http;

	//	gr: not expecting any default data in the handshake, so fail if we do
	auto DefaultParam = Reply.mParams.GetDefaultParam();
	if ( DefaultParam.IsValid() )
	{
		assert( !DefaultParam.IsValid() );
		DefaultParam = TJobParam();
	}
	
	//	gr: need content length or chrome never displays..
	Http.PushHeader( "Content-Length", std::stringstream() << DefaultParam.GetDataSize() );
	//Http.PushHeader( SoyHttpHeaderElement("Content-Type", SoyHttpHeader::FormatToHttpContentType(Reply.mData.mFormat).c_str() ) );

	//	add the original params, as they were filled up when we did the Handshake decoding
	PushParams( Http, Reply.mParams.mParams );
	PushParams( Http, Reply.mOrigParams.mParams );
	
	if ( !Http.GetResponseData( Output, 101, "Switching Protocols" ) )
	{
		//	http always needs a response! last chance error
		auto OutputBridge = GetArrayBridge(Output);
		Soy::StringToArray( "failed to get HTTP response", OutputBridge );
	}

	return true;
}
*/

namespace TWebSockets
{
	namespace TOpCode
	{
		enum Type
		{
			Invalid					= -1,
			ContinuationFrame		= 0,
			TextFrame				= 1,
			BinaryFrame				= 2,
			ConnectionCloseFrame	= 8,
			PingFrame				= 9,
			PongFrame				= 10,
		};
		DECLARE_SOYENUM( TWebSockets::TOpCode );
	}
}



std::map<TWebSockets::TOpCode::Type,std::string> TWebSockets::TOpCode::EnumMap =
{
	{ TWebSockets::TOpCode::Invalid,				"invalid" },
	{ TWebSockets::TOpCode::ContinuationFrame,		"ContinuationFrame" },
	{ TWebSockets::TOpCode::TextFrame,				"TextFrame" },
	{ TWebSockets::TOpCode::BinaryFrame,			"BinaryFrame" },
	{ TWebSockets::TOpCode::ConnectionCloseFrame,	"ConnectionCloseFrame" },
	{ TWebSockets::TOpCode::PingFrame,				"PingFrame" },
	{ TWebSockets::TOpCode::PongFrame,				"PongFrame" },
};





//	gr: make this serialiseable!
class TWebSocketMessageHeader
{
public:
	TWebSocketMessageHeader() :
		Length		( 0 ),
		Length16	( 0 ),
		LenMostSignificant	( 0 ),
		Length64	( 0 ),
		Fin			( 1 ),
		Reserved	( 0 ),
		OpCode		( TWebSockets::TOpCode::Invalid ),
		Masked		( false )
	{
	}
	int		Fin;
	int		Reserved;
	int		OpCode;
	int		Masked;
	int		Length;
	int		Length16;
	int		LenMostSignificant;
	uint64	Length64;
	BufferArray<unsigned char,4> MaskKey;	//	store & 32 bit int
	
	bool		IsText() const		{	return OpCode == TWebSockets::TOpCode::TextFrame;	}
	int			GetLength() const;
	std::string	GetMaskKeyString() const;
	bool		IsValid(std::stringstream& Error) const;
	bool		Decode(const ArrayBridge<char>& HeaderData,int& MoreDataRequired,std::stringstream& Error);
	bool		Encode(ArrayBridge<char>& Data,const ArrayBridge<char>& MessageData,std::stringstream& Error);
};


/*
#include "UnitTest++/src/UnitTest++.h"
TEST(EncodeAndDecodeWebSocketMessageHeader)
{
	std::stringstream Error;
	TWebSocketMessageHeader Header;
	Header.OpCode = TWebSockets::TOpCode::TextFrame;
	Array<char> MessageData;
	auto MessageDataBridge = GetArrayBridge( MessageData );
	Array<char> Buffer;
	auto BufferBridge = GetArrayBridge( Buffer );
	CHECK( Header.Encode( BufferBridge, MessageDataBridge, Error ) );
	
	//	decode again
	int MoreDataRequired = 0;
	TWebSocketMessageHeader DecodedHeader;
	CHECK( DecodedHeader.Decode( BufferBridge, MoreDataRequired, Error ) );
	CHECK( MoreDataRequired == 0 );
	CHECK( DecodedHeader.OpCode == Header.OpCode );
	CHECK( Header.GetLength() == DecodedHeader.GetLength() );
}
*/

int TWebSocketMessageHeader::GetLength() const
{
	if ( Length64 != 0 )
	{
		if ( Length16 != 0 || Length != 127 )
			return -1;
		
		//	currently limited cos we're using 32bit lengths in arrays and stuff
		if ( Length64 > std::numeric_limits<int>::max() )
			return -1;
		return static_cast<int>( Length64 );
	}
	
	if ( Length16 != 0 )
	{
		if ( Length64 != 0 || Length != 126 )
			return -1;
		return Length16;
	}

	auto Error = [this]
	{
		std::stringstream Error;
		Error << "Length (" << this->Length << ") expected < 127";
		return Error.str();
	};

	if ( !Soy::Assert( Length < 127, Error  ) )
		return -1;
	
//	if ( Length < 127 )
	{
		if ( Length64 != 0 || Length16 != 0 )
			return -1;
	}
	
	//	length must be 0?
	if ( Length < 0 )
		return -1;
	return Length;
}


template<typename TYPE>
class THex
{
public:
	TYPE	mValue;
	
	void	GetString(std::stringstream& String) const;
};
/*
template<> bool TSerialisation<THex<uint32>>::ImportText(std::stringstream& String)
{
	mValue.mValue = 0;
	int TypeNibbles = 2*sizeof(mValue.mValue);
	for ( int i=0;	i<TypeNibbles;	i++ )
	{
		char hex;
		String >> hex;
		if ( String.fail() )
			return false;
		if ( hex >= 'a' )		hex = 0xA + (hex - 'a');
		else if ( hex >= 'A' )	hex = 0xA + (hex - 'A');
		else if ( hex >= '0' )	hex = 0x0 + (hex - '0');
		else
			return false;
		if ( hex > 0xf )
			return false;
		mValue.mValue |= hex << ((TypeNibbles-1-i)*4);
	}
	return true;
}
*/
template<>
void THex<uint32>::GetString(std::stringstream& String) const
{
	int TypeNibbles = 2*sizeof(mValue);
	for ( int i=0;	i<TypeNibbles;	i++ )
	{
		char hex = mValue >> ((TypeNibbles-1-i)*4);
		hex &= (1<<4)-1;
		if ( hex >= 10 )
			hex += 'A' - 10;
		else
			hex += '0';
		String << hex;
	}
}
/*
#include "UnitTest++/src/UnitTest++.h"
TEST(ReadHex)
{
	THex<uint32> h;
	TSerialisation<THex<uint32>> s( h );
	CHECK( s.ImportText( ("12345678") ) );
	CHECK( h.mValue == 0x12345678 );
}

TEST(WriteHex)
{
	THex<uint32> h;
	h.mValue = 0x12345678;
	TSerialisation<THex<uint32>> s( h );
	std::stringstream String;
	CHECK( s.ExportText( String ) );
	std::Debug << String.str() << std::endl;
	CHECK( String.str() == "12345678" );
}

*/
std::string TWebSocketMessageHeader::GetMaskKeyString() const
{
	if ( MaskKey.IsEmpty() )
		return std::string();
	
	//	represent as int32 (hex might be nicer)
	THex<uint32> Mask32;
	Mask32.mValue = *reinterpret_cast<const uint32*>( MaskKey.GetArray() );

	std::stringstream s;
	Mask32.GetString(s);
	return s.str();
}

bool TWebSocketMessageHeader::IsValid(std::stringstream& Error) const
{
	if ( Reserved != 0 )
	{
		Error << "Reserved(" << Reserved << ")!=0";
		return false;
	}
	
	//	most significant bit should always be 0 (ws can't handle full-64bit length)
	if ( LenMostSignificant != 0 )
	{
		Error << "LenMostSignificant(" << LenMostSignificant << ")!=0";
		return false;
	}

	if ( MaskKey.GetSize() != 0 && MaskKey.GetSize() != 4 )
	{
		Error << "MaskKey size(" << MaskKey.GetSize() << ") invalid";
		return false;
	}
	
	//	this checks the lengths for us
	int Length = GetLength();
	if ( Length < 0 )
	{
		Error << "Invalid length";
		return false;
	}
	
	//	we only support some opcodes atm
	switch ( OpCode )
	{
		case TWebSockets::TOpCode::TextFrame:
		case TWebSockets::TOpCode::BinaryFrame:
		case TWebSockets::TOpCode::ConnectionCloseFrame:
			return true;

		case TWebSockets::TOpCode::ContinuationFrame:
			return true;
			
		default:
			Error << "Unsupported opcode " << OpCode;
			return false;
	};
	
	return true;
}

bool TWebSocketMessageHeader::Decode(const ArrayBridge<char>& Data,int& MoreDataRequired,std::stringstream& Error)
{
	TBitReader BitReader( Data );

	//	return false & no data == error
	//	return false & MoreData == need more data

	//	if any of these fail, we need more data
	MoreDataRequired = 1;
	if ( !BitReader.Read( Fin, 1 ) )
		return false;
	if ( !BitReader.Read( Reserved, 3 ) )
		return false;
	if ( !BitReader.Read( OpCode, 4 ) )
		return false;
	
	MoreDataRequired = 1;
	if ( !BitReader.Read( Masked, 1 ) )
		return false;
	
	Length16 = 0;
	Length64 = 0;
	if ( !BitReader.Read( Length, 7 ) )
		return false;
	
	if ( Length == 126 )	//	length is 16bit
	{
		MoreDataRequired = 16/8;	//	gr: not quite right = 16 bits - bytes unread
		if ( !BitReader.Read( Length16, 16 ) )
			return false;
	}
	else if ( Length == 127 )	//	length is 64 bit
	{
		MoreDataRequired = 64/8;	//	gr: not quite right = 16 bits - bytes unread
		if ( !BitReader.Read( Length64, 64 ) )
			return false;
	}
	
	MaskKey.Clear();
	if ( Masked )
	{
		int m0,m1,m2,m3;
		MoreDataRequired = 1;
		if ( !BitReader.Read( m0, 8 ) )		return false;
		if ( !BitReader.Read( m1, 8 ) )		return false;
		if ( !BitReader.Read( m2, 8 ) )		return false;
		if ( !BitReader.Read( m3, 8 ) )		return false;
		MaskKey.PushBack( m0 );
		MaskKey.PushBack( m1 );
		MaskKey.PushBack( m2 );
		MaskKey.PushBack( m3 );
	}
	
	//	check theres enough data left...
	int BitPos = BitReader.BitPosition();
	//int BytesRead = BitPos / 8;
	//	gr: expecting this to align, no mentiuon in the RFC but pretty sure it does (and makes sense
	MoreDataRequired = 0;
	if ( BitPos % 8 != 0 )
		return false;

	//	check state of everything we read
	if ( !IsValid(Error) )
	{
		MoreDataRequired = 0;
		return false;
	}
	return true;
}


bool TWebSocketMessageHeader::Encode(ArrayBridge<char>& OutputData,const ArrayBridge<char>& MessageData,std::stringstream& Error)
{
	//	should be valid here. with zero length
	if ( !IsValid(Error) )
		return false;
	if ( GetLength() != 0 )
	{
		Error << "expected length(" << GetLength() << ") to be zero";
		return false;
	}
	
	//	write header to it's own array first
	BufferArray<char,10> HeaderData;
	auto HeaderDataBridge = GetArrayBridge( HeaderData );
	
	//	write message header
	TBitWriter BitWriter( HeaderDataBridge );
	BitWriter.WriteBit( Fin );		//	fin
	BitWriter.Write( (uint8)Reserved, 3 );	//	reserved
	
	BitWriter.Write( (uint8)OpCode, 4 );
	BitWriter.WriteBit(Masked);	//	masked
	
	uint64 PayloadLength = MessageData.GetDataSize();
	//	write length
	if ( PayloadLength > 0xffff )
	{
		//	64 bit length
		BitWriter.Write( 127u, 7 );	//	"theres another 64 bit length"
		BitWriter.Write( PayloadLength, 64 );
	}
	else if ( PayloadLength > 125 )
	{
		BitWriter.Write( 126u, 7 );	//	"theres another 16 bit length"
		BitWriter.Write( PayloadLength, 16 );
	}
	else
	{
		BitWriter.Write( PayloadLength, 7 );
	}
	
	if ( Masked )
	{
		unsigned char _MaskKey[4] = {1,2,3,4};
		BufferArray<unsigned char,4> MaskKey(_MaskKey);
		BitWriter.Write( MaskKey[0], 8 );
		BitWriter.Write( MaskKey[1], 8 );
		BitWriter.Write( MaskKey[2], 8 );
		BitWriter.Write( MaskKey[3], 8 );
	}
	
	//	data left over should align!
	int Remainder = BitWriter.BitPosition() % 8;
	assert( Remainder == 0 );
	if ( Remainder != 0 )
		return false;
	
	//	mask all the data
	OutputData.PushBackArray(MessageData);
	if ( Masked )
	{
		//MakeMessageData( OutputData );
		//	todo: encode data!
		assert( !Masked );
		return false;
	}
	
	
	static bool TestForUtf8 = false;
	if ( TestForUtf8 && this->IsText() )
	{
		std::stringstream OutputString;
		Soy::ArrayToString( OutputData, OutputString );
		bool IsUtf8 = Soy::IsUtf8String( OutputString.str() );
		if ( !Soy::Assert( IsUtf8, "Unexpected non-UTF8 char in websocket text message" ) )
			return false;
	}
	
	//	insert header at the begininig
	OutputData.InsertArray( HeaderData,0 );
	return true;
}
/*
TDecodeResult::Type TProtocolWebSocketImpl::DecodeHeader(TJob& Job,TStreamBuffer& Stream)
{
	//	get the client meta
	auto pClientMeta = GetClientMeta( Job.mChannelMeta.mClientRef );
	if ( !pClientMeta )
	{
		assert( pClientMeta );
		return TDecodeResult::Error;
	}
	auto& ClientMeta = *pClientMeta;

	//	if we haven't handshaked, do that
	if ( !ClientMeta.mHasHandshaked )
	{
		return ClientMeta.DecodeHandshake( Job, Stream );
	}

	//	gr: todo: just read the websocket header, that will tell us length
	//			then DecodeData reads X bytes, decodes and reads tail
	//			THEN subprotocol reads header & data...
	//	problems:	* ws header won't give us a job/command
	//				* doing sub protocol header decoding in decode data (doesn't matter?)
	//				* websocket header is a VARIABLE LENGTH
	//	gr: websocket fixed-length part is
	//	1+3+4 +1+7 +16=32
	//	+16 if length 16 bit
	//	+64 if length is 64 bit
	//	+4 if masked
	Array<char> HeaderData;
	auto HeaderBridge = GetArrayBridge( HeaderData );
	if ( !Stream.Pop( 32/8, HeaderBridge ) )
		return TDecodeResult::Waiting;

	TWebSocketMessageHeader Header;
	std::stringstream Error;
	int MoreDataRequired = 0;
	while ( !Header.Decode( HeaderBridge, MoreDataRequired, Error ) )
	{
		//	don't need more data, some error
		if ( MoreDataRequired == 0 )
		{
			//	todo: HTTP connections should bounce on error?
			std::Debug << "failed to decode websocket message: " << Error.str() << std::endl;
			return TDecodeResult::Error;
		}

		//	read the extra data we need
		Array<char> AdditionalData;
		auto AdditionalDataBridge = GetArrayBridge( AdditionalData );
		if ( !Stream.Pop( MoreDataRequired, AdditionalDataBridge ) )
		{
			//	not availible, push all the data back!
			Stream.UnPop( HeaderBridge );
			return TDecodeResult::Waiting;
		}
		HeaderData.PushBackArray( AdditionalDataBridge );
	}
	
	//	gr: not sure how, but check the continuation packet type and make sure it's only set when we're a child/nextmessage
	//		don't currently have knowledge of parent.
	
	//	work out the size of the data we still need to read
	int MessageDataSize = Header.GetLength();
//	std::Debug << "Websocket header was " << HeaderData.GetSize() << " bytes" << std::endl;
//	std::Debug << "Websocket length: " << Header.GetLength() << " bytes" << std::endl;
//	std::Debug << "MessageDataSize: " << MessageDataSize << " bytes " << std::endl;
	assert( MessageDataSize >= 0 );
	if ( MessageDataSize < 0 )
		return TDecodeResult::Error;
	
	//	setup the job and the meta we need (todo: serialise TWebSocketMessageHeader into a single param?)
	Job.mParams.mCommand = WebsocketMessageCommand;
	Job.mParams.AddParam("payloadsize", Soy::StreamToString(std::stringstream()<<MessageDataSize) );
	//	set format if it's text, otherwise binary
	if ( Header.IsText() )
		Job.mParams.AddParam("format", TJobFormat( "text" ).GetFormatString() );
	if ( Header.Masked )
		Job.mParams.AddParam(WebsocketMaskKeyParam, Header.GetMaskKeyString() );
	Job.mParams.AddParam(WebSocketFinParam, (Header.Fin==1) );
	Job.mParams.AddParam(WebsocketIsContinuationParam, (Header.OpCode == TWebSockets::TOpCode::ContinuationFrame) );

	//	close connection
	if ( Header.OpCode == TWebSockets::TOpCode::ConnectionCloseFrame )
		return TDecodeResult::Disconnect;
	
	return TDecodeResult::Success;
}


//	gr: copy & paste of TProtocolCli::Decodedata.... 
TDecodeResult::Type TProtocolWebSocketImpl::DecodeData(TJob& Job,TStreamBuffer& Stream)
{
	//	currently expecting all jobs to be called this
	auto Error = [&Job]
	{
		std::stringstream Error;
		Error << "Expected job(" << Job.mParams.mCommand << " to only have the command name " << WebsocketMessageCommand;
		return Error.str();
	};
	if ( !Soy::Assert( Job.mParams.mCommand == WebsocketMessageCommand, Error ) )
		return TDecodeResult::Error;

	//	gr: to handle mulitple messages, the data all goes into the payload param
	//	if the param is missing, we haven't loaded our payload yet
	if ( !Job.mParams.HasParam( WebsocketPayloadParam ) )
	{
		//	how much data are we expecting?
		auto PayloadParam = Job.mParams.GetParam("payloadsize");
		if ( !PayloadParam.IsValid() )
			return TDecodeResult::Success;
		int PayloadSize;
		if ( !PayloadParam.Decode(PayloadSize) )
			return TDecodeResult::Error;
		if ( PayloadSize == 0 )
			return TDecodeResult::Success;

		//	read the data we're expecting
		std::shared_ptr<SoyData_Stack<Array<char>>> DataData( new SoyData_Stack<Array<char>>() );
		auto DataBridge = GetArrayBridge( DataData->mValue );
		if ( !Stream.Pop( PayloadSize, DataBridge ) )
			return TDecodeResult::Waiting;

		//	unmask the data
		auto MaskKeyParam = Job.mParams.GetParam(WebsocketMaskKeyParam);
		if ( MaskKeyParam.IsValid() )
		{
			std::string MaskKeyStr = MaskKeyParam.Decode<std::string>();
			THex<uint32> MaskKey32;
			TSerialisation<THex<uint32>> Serial( MaskKey32 );
			if ( !Serial.ImportText( MaskKeyStr ) )
				return TDecodeResult::Error;
			//	decode masked data
			BufferArray<unsigned char,4> MaskKey;
			MaskKey.PushBackReinterpret( MaskKey32.mValue );
			for ( int i=0;	i<DataBridge.GetSize();	i++ )
			{
				char Encoded = DataBridge[i];
				char Decoded = Encoded ^ MaskKey[i%4];
				DataBridge[i] = Decoded;
			}
		}
		
		//	save the data in our payload (we need to save it so we can append next messages to it)
		std::shared_ptr<SoyData> DataDataRaw( DataData );
		Job.mParams.AddParam( WebsocketPayloadParam, DataDataRaw );
		//std::Debug << "Added " << Soy::FormatSizeBytes(DataBridge.GetDataSize()) << " to job param " << WebsocketPayloadParam << "..." << std::endl;
	}
	
	//	if we already have a next-message param, then we're waiting for the next (current job has fin=0)
	//	decode that next one. When it finishes, append the data to our data.
	//	this is essentially recursive, so when it finishes, we should never come back here again so
	//	maybe dont need to set any "we've got the next messages" flag
	auto NextMessageParam = Job.mParams.GetParam( WebsocketNextMessageParam );
	if ( NextMessageParam.IsValid() )
	{
		//	continue to decode header
		//	gr: this is a bit hacky, something that probably needs to be added to SoyData!
		//		but we want to edit the job in place (else copy out, modify then write abck)
		auto pNextJob = std::dynamic_pointer_cast<SoyData_Stack<TJob>>( NextMessageParam.mSoyData );
		if ( !Soy::Assert( pNextJob!=nullptr, "expected job param" ) )
		{
			//	to debug
			std::dynamic_pointer_cast<SoyData_Stack<TJob>>( NextMessageParam.mSoyData );
			return TDecodeResult::Error;
		}
		
		auto& Protocol = *this;
		
		//	gr: copy of code from TChannelIncomingJobHandler::Update
		auto& NextJob = pNextJob->mValue;
		//	if job is invalid, we're still waiting for header
		if ( !NextJob.IsValid() )
		{
			auto Result = Protocol.DecodeHeader( NextJob, Stream );
			if ( Result != TDecodeResult::Success )
				return Result;
		}
	
		//	decode data
		auto Result = Protocol.DecodeData( NextJob, Stream );
		if ( Result != TDecodeResult::Success )
			return Result;
		
		//	append the payload data to our data
		//	gr: another hacky cast
		auto ParentPayloadParam = Job.mParams.GetParam( WebsocketPayloadParam );
		if ( !Soy::Assert( ParentPayloadParam.IsValid(), "expected parent payload param" ) )
			return TDecodeResult::Error;
		
		auto ChildPayloadParam = NextJob.mParams.GetParam( WebsocketPayloadParam );
		if ( !Soy::Assert( ChildPayloadParam.IsValid(), "expected child payload param" ) )
			return TDecodeResult::Error;

		auto pParentData = std::dynamic_pointer_cast<SoyData_Stack<Array<char>>>( ParentPayloadParam.mSoyData );
		auto pChildData = std::dynamic_pointer_cast<SoyData_Stack<Array<char>>>( ChildPayloadParam.mSoyData );
		
		//	append child data to parent data
		std::Debug << "Appending next-message data (" << Soy::FormatSizeBytes(pChildData->mValue.GetDataSize()) << ") to parent (" << Soy::FormatSizeBytes(pParentData->mValue.GetDataSize()) << ")" << std::endl;
		pParentData->mValue.PushBackArray( pChildData->mValue );
	}
	else
	{
		//	if there are more packets on the way, process the next message...
		bool Fin = Job.mParams.GetParamAsWithDefault<bool>(WebSocketFinParam,true);
		if ( !Fin )
		{
			std::Debug << "Adding next-message param to catch continuation message" << std::endl;

			//	add a "next message" param, if this is found when decoding, we process that next until we've hit the fin one
			//	gr: add something to check for the continuation opcode?
			std::shared_ptr<SoyData_Stack<TJob>> NextJobData( new SoyData_Stack<TJob>() );
			
			//	Next job MUST have client meta set to succeed with handshaking check
			//	gr: for this case (sub job), should we skip the check entirely? make it clear this isn't a normal job we're decoding?
			auto& NextJob = NextJobData->mValue;
			NextJob.mChannelMeta = Job.mChannelMeta;
		
			std::shared_ptr<SoyData> NextJobDataData(NextJobData);
			auto NextJobParam = Job.mParams.AddParam( WebsocketNextMessageParam, NextJobDataData );
			if ( !Soy::Assert(NextJobParam.IsValid(),"Failed to add job param" ) )
				return TDecodeResult::Error;

			return TDecodeResult::Waiting;
		}
	}
	
	//	if I'm a continuation message, I'm finished, don't decode with subprotocol yet (root job does that)
	bool IsContinuation = Job.mParams.GetParamAsWithDefault<bool>( WebsocketIsContinuationParam, false );
	if ( IsContinuation )
	{
		std::Debug << "Continuation message finished" << std::endl;
		auto ParentPayloadParam = Job.mParams.GetParam( WebsocketPayloadParam );
		if ( !Soy::Assert( ParentPayloadParam.IsValid(), "expected parent payload param" ) )
			return TDecodeResult::Error;
		
		return TDecodeResult::Success;
	}

	//	do sub-protocol decoding
	auto pSubProtocol = GetSubProtocol();
	if ( pSubProtocol )
	{
		Array<char> Data;
		if ( !Job.mParams.GetParamAs( WebsocketPayloadParam, Data ) )
		{
			Soy::Assert(false, "expected payload data");
			return TDecodeResult::Error;
		}
		auto DataBridge = GetArrayBridge(Data);

		auto& SubProtocol = *pSubProtocol;
		TStreamBuffer SubStream;
		if ( !SubStream.Push( DataBridge ) )
			return TDecodeResult::Error;
		
		//	prepare job/data so sub protocol can write to it
		Job = TJob();
		auto SubDecodeHeaderResult = SubProtocol.DecodeHeader( Job, SubStream );
		//	we have all the data so this shouldn't be anything but success!
		if ( SubDecodeHeaderResult != TDecodeResult::Success )
			return TDecodeResult::Error;
		auto SubDecodeDataResult = SubProtocol.DecodeData( Job, SubStream );
		if ( SubDecodeDataResult != TDecodeResult::Success )
			return TDecodeResult::Error;
	}
	
	return TDecodeResult::Success;
}


bool TProtocolWebSocketImpl::Encode(const TJobReply& Reply,Array<char>& Output)
{
	//	gr: currently for ease, handshake is reply only
	//		if moved, we NEED to make sure upgrade and other header keys are still present
	//	we send HTTP style reply if it's the handshake reply
	if ( Reply.mParams.mCommand == "re:Handshake" )
	{
		//	get the client meta
		auto pClientMeta = GetClientMeta( Reply.mChannelMeta.mClientRef );
		if ( !pClientMeta )
		{
			assert( pClientMeta );
			return false;
		}
		auto& ClientMeta = *pClientMeta;
		return ClientMeta.EncodeHandshake( Reply, Output );
	}

	return Encode( static_cast<const TJob&>( Reply ), Output );
}

bool TProtocolWebSocketImpl::Encode(const TJob& Reply,Array<char>& Output)
{
	
	//	everything else is websocket-encoded
	//	encode in sub protocol first...
	Array<char> MessageData;
	auto pSubProtocol = GetSubProtocol();
	if ( pSubProtocol )
	{
		if ( !pSubProtocol->Encode( Reply, MessageData ) )
			return false;
	}
	else
	{
		//	gr: errr we kinda NEED a sub protocol. otherwise... I don't know how to send the data.
		//		incoming is okay, but going out, not so much
		assert(false);
//		MessageData = Reply.mData.mData;
	}
	
	//	now encode for websocket
	TWebSocketMessageHeader Header;
	Header.OpCode = TWebSockets::TOpCode::TextFrame;
	auto MessageDataBridge = GetArrayBridge( MessageData );
	auto OutputBridge = GetArrayBridge( Output );
	std::stringstream Error;
	if ( !Header.Encode( OutputBridge, MessageDataBridge, Error) )
		return false;
	
	return true;
}



bool TProtocolWebSocketImpl::Encode(const TJob& Command,std::stringstream& Output)
{
	assert(false);
	return false;
}


bool TProtocolWebSocketImpl::Encode(const TJobReply& Reply,std::stringstream& Output)
{
	assert(false);
	return false;
}

std::shared_ptr<TWebSocketClient> TProtocolWebSocketImpl::GetClientMeta(SoyRef ClientRef)
{
	if ( !Soy::Assert( ClientRef.IsValid(), "Expected valid client ref when getting websocket client meta" ) )
		return nullptr;

	auto& ClientMeta = mClientMeta[ClientRef];
	if ( !ClientMeta )
	{
		if ( ClientRef.IsValid() )
			ClientMeta.reset( new TWebSocketClient );
	}
	return ClientMeta;
}
*/
