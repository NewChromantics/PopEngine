#include "TPopServerThread.h"
#include "TPopApp.h"
#include <regex>



TPopServerThread::TPopServerThread(TPopApp& App,uint16 ListenPort) :
	SoyThread	( "TPopServerThread" ),
	mApp		( App ),
	mListenPort	( ListenPort )
{
}


void TPopServerThread::threadedFunction()
{
	if ( !Init() )
	{
		assert( false );
		return;
	}

	while ( isThreadRunning() )
	{

		sleep(0);
	}
}

bool TPopServerThread::Init()
{
	mSocket = AllocSocket();
	if ( !mSocket )
		return false;
	
	if ( !mSocket->Listen( mListenPort ) )
		return false;

	//	setup event listeners
	ofAddListener( mSocket->mOnClientJoin, this, &TPopServerThread::OnClientJoin );
	ofAddListener( mSocket->mOnClientLeft, this, &TPopServerThread::OnClientLeft );
	ofAddListener( mSocket->mOnRecievePacket, this, &TPopServerThread::OnSocketRecievePacket );

	return true;
}

	
void TPopServerThread::OnSocketRecievePacket(SoyNet::TSocket*& Socket)
{
	//	shouldnt be null...
	assert( Socket );
	if ( !Socket )
		return;

	//	pop latest packets
	SoyPacketContainer Packet;
	while( Socket->mPacketsIn.PopPacket( Packet ) )
	{
		//	call overloaded handler and get the reply....
		SoyPacketContainer ReplyPacket;
		if ( !OnRecievePacket( ReplyPacket, Packet ) )
		{
			//	disconnect client
			//Socket->DisconnectClient( Packet.mSender );
			continue;
		}

		//	send back to sender
		ReplyPacket.mDestination = Packet.mSender;

		//	send the reply back
		SendPacket( ReplyPacket );
	}
}
	
bool TPopServerThread::SendPacket(const SoyPacketContainer& Packet)
{
	if ( !mSocket )
		return false;
	mSocket->mPacketsOut.PushPacket( Packet );
	return true;
}


void TProtocolWebSockets::OnClientJoin(const SoyNet::TAddress& Client)
{
	//	alloc meta data
	if ( mClients.Find( Client ) )
	{
		assert(false);
		OnClientLeft( Client );
	}

	mClients.PushBack( Client );
}

void TProtocolWebSockets::OnClientLeft(const SoyNet::TAddress& Client)
{
	if ( !mClients.Remove( Client ) )
	{
		//assert( false );
	}
}
	

	
/*
#define _WEBSOCKETPP_CPP11_SYSTEM_ERROR_
#define _WEBSOCKETPP_CPP11_MEMORY_
#include <websocketpp\sha1\sha1.hpp>
#include <websocketpp\base64\base64.hpp>
#include <websocketpp\processors\base.hpp>
*/
#pragma warning( disable: 4250 )
#include <Poco/Base64Decoder.h>
#include <Poco/Base64Encoder.h>
#include <Poco/SHA1Engine.h>
#include <Poco/DigestStream.h>


BufferString<200> SoyWebSocketHeader::GetReplyKey() const
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

	//	run a hash on the appended key
	//websocketpp::sha1::calc(key.c_str(),key.length(),message_digest);
	std::vector<unsigned char> HashedKey;
	{
		Poco::SHA1Engine Sha1;
		Poco::DigestOutputStream outstr(Sha1);
		outstr << key;
		outstr.flush(); //to pass everything to the digest engine
		
		HashedKey = Sha1.digest();
	}

	//	encode to base 64
	//key = websocketpp::base64_encode(message_digest,20);
	{
		std::stringstream Key64;
		Poco::Base64Encoder Encoder( Key64 );
		for( int i=0;	i<HashedKey.size();	i++ )
			Encoder << HashedKey[i];
		//	gr: flush() doesn't work, need to close the input to finish
		Encoder.close();
		key = Key64.str();
	}
	

	return key.c_str();
}


bool SoyWebSocketHeader::PushRawHeader(const BufferString<200>& Header)
{
	if ( !SoyHttpHeader::PushRawHeader( Header ) )
		return false;

	//	extract web-socket special headers
	if ( Header.StartsWith("Upgrade: websocket" ) )
	{
		mIsWebSocketUpgrade = true;
		return true;
	}

	if ( Header.StartsWith("Sec-WebSocket-Key: " ) )
	{
		mWebSocketKey = Header;
		mWebSocketKey.RemoveAt( 0, Soy::StringLen("Sec-WebSocket-Key: ",-1,mWebSocketKey.GetArray().MaxAllocSize()-1) );
		return true;
	}

	return true;
}



SoyPacketContainer GetHttpErrorPacket(int HttpCode,const char* Error)
{
	SoyPacketContainer Packet;
	TString Http;
	Http << "HTTP/1.1 " << HttpCode << " OK\r\n";
	Http << "\r\n";
	Http << Error;
		
	Packet.Set( Http.GetArray(), SoyPacketMeta(), SoyNet::TAddress() );
	return Packet;
}

bool TProtocolWebSockets::OnRecievePacket(SoyPacketContainer& ReplyPacket,const SoyPacketContainer& Packet)
{
	//	find client meta, MUST have connected first... 
	auto* ClientMeta = mClients.Find( Packet.mSender );
	assert( ClientMeta );
	if ( !ClientMeta )
		return false;

	auto& Client = *ClientMeta;

	//	not handshaked, so process the http handshake first
	if ( !Client.mHasHandshaked )
		return Client.OnHandshakePacket( ReplyPacket, Packet );
	
	Array<char> MessageData;
	bool IsText = false;
	if ( !Client.OnMessagePacket( MessageData, IsText, Packet ) )
		return false;
	
	if ( IsText )
	{
		TString MessageString;
		MessageString.CopyString( MessageData.GetArray(), MessageData.GetSize() );
		if ( !OnTextMessage( ReplyPacket, MessageString, Client ) )
			return false;
	}
	else
	{
		if ( !OnBinaryMessage( ReplyPacket, MessageData, Client ) )
			return false;
	}
	
	return true;
}


bool TProtocolWebSockets::SendTextMessage(const std::string& Text)
{
	//	make up packet
	SoyPacketContainer Packet;

	TString TextString = Text;
	if ( !TWebSocketClient::EncodeMessageData( Packet.mData, TextString.GetArray(), true ) )
		return false;

	return SendPacket( Packet );
}

TWebSocketClient::TWebSocketClient(SoyNet::TAddress Address) :
	mAddress		( Address ),
	mHasHandshaked	( false )
{
}
	
bool TWebSocketClient::OnHandshakePacket(SoyPacketContainer& ReplyPacket,const SoyPacketContainer& Packet)
{
	SoyWebSocketHeader Header;
	Header.mData = Packet.mData;
	if ( !Header.PopHeader( Header.mData ) )
		return false;

	for ( int i=0;	i<Header.mElements.GetSize();	i++ )
	{
		auto& Element = Header.mElements[i];
		BufferString<1000> Debug;
		Debug << Element.mKey << " = " << Element.mValue;
		ofLogNotice( Debug.c_str() );
	}

	SoyWebSocketHeader Reply;
	{
		auto* Element = Header.GetHeader("Sec-WebSocket-Protocol");
		if ( Element )
		{
			mProtocol = *Element;
			SoyHttpHeaderElement NewHeader("Sec-WebSocket-Protocol", *Element );
			Reply.PushHeader( NewHeader );
		}
	}
	if ( Header.mIsWebSocketUpgrade )
	{
		Reply.PushHeader( SoyHttpHeaderElement("Sec-WebSocket-Accept", Header.GetReplyKey() ) );
		Reply.PushHeader( SoyHttpHeaderElement("Upgrade", "websocket" ) );
		Reply.PushHeader( SoyHttpHeaderElement("Connection", "Upgrade" ) );
	}
	
	//	success parsing!
	mHasHandshaked = true;
	

	Reply.GetResponseData( ReplyPacket.mData, 101, "Switching Protocols" );
	return true;
}




namespace TWebSockets
{
	namespace TOpCode
	{
		enum TYPE
		{
			ContinuationFrame		= 0,
			TextFrame				= 1,
			BinaryFrame				= 2,
			ConnectionCloseFrame	= 8,
			PingFrame				= 9,
			PongFrame				= 10,
		};
	}
}


bool TWebSocketClient::OnMessagePacket(Array<char>& Data,bool& IsTextData,const SoyPacketContainer& Packet)
{
	return DecodeMessageData( Data, Packet.mData, IsTextData );
}

bool TWebSocketClient::DecodeMessageData(Array<char>& DecodedData,const Array<char>& EncodedData,bool& IsTextData)
{
	//	todo: decode packet data into Data
	DecodedData.Clear();
	TBitReader BitReader( GetArrayBridge( EncodedData ) );
	
	int Fin;
	int Reserved;
	if ( !BitReader.Read( Fin, 1 ) )		
		return false;
	if ( Fin != 1 )
		return false;
	if ( !BitReader.Read( Reserved, 3 ) )	
		return false;
	if ( Reserved != 0 )
		return false;

	int OpCode;
	int Masked;
	if ( !BitReader.Read( OpCode, 4 ) )		
		return false;
	IsTextData = (OpCode == TWebSockets::TOpCode::TextFrame);
	if ( !BitReader.Read( Masked, 1 ) )		
		return false;

	int Length;
	uint64 Length64 = 0;
	if ( !BitReader.Read( Length, 7 ) )		
		return false;

	if ( Length == 126 )	//	length is 16bit
	{
		if ( !BitReader.Read( Length, 16 ) )		
			return false;
		Length64 = Length;
	}
	else if ( Length == 127 )	//	length is 64 bit
	{
		int LenMostSignificant;
		if ( !BitReader.Read( LenMostSignificant, 1 ) )
			return false;
		if ( LenMostSignificant != 0 )
			return false;
		if ( !BitReader.Read( Length64, 63 ) )		
			return false;
	}
	else
	{
		Length64 = Length;
	}

	//	something has gone wrong - probably length encoding is wrong, or split message
	if ( Length64 > EncodedData.GetSize() )
	{
		BufferString<100> Debug;
		Debug << "Decoded length; " << Length64 << ", data length " << EncodedData.GetSize() <<". aborting websocket packet.";
		ofLogError( Debug.c_str() );
		return false;
	}

	BufferArray<unsigned char,4> MaskKey;
	if ( Masked )
	{
		int m0,m1,m2,m3;
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
	int BytesRead = BitPos / 8;
	//	gr: expecting this to align, no mentiuon in the RFC but pretty sure it does (and makes sense
	if ( BitPos % 8 != 0 )
		return false;

	//	copy non-masked data
	if ( MaskKey.IsEmpty() )
	{
		DecodedData = EncodedData;
		DecodedData.RemoveBlock( 0, BytesRead );
		return true;
	}

	//	decode masked data
	auto& Frame = EncodedData;
    int j = 0;
	int DecodeEnd = ofMin( BytesRead+static_cast<int>(Length64), Frame.GetSize() );
    for ( int i=BytesRead;	i<DecodeEnd;	i++,j++ )
	{
		char Decoded = Frame[i] ^ MaskKey[j%4];
        DecodedData.PushBack( Decoded );
	}

	return true;
}



bool TWebSocketClient::EncodeMessageData(Array<char>& EncodedData,const Array<char>& DecodedData,const bool DataIsText)
{
	EncodedData.Clear();
	auto& MessageData = EncodedData;
	auto& Data = DecodedData;

	bool Masked = false;

	//	write message header
	TBitWriter BitWriter( GetArrayBridge( MessageData ) );
	BitWriter.WriteBit(1);		//	fin
	BitWriter.Write(0u,3);	//	reserved
	
	uint8 OpCode = DataIsText ? TWebSockets::TOpCode::TextFrame : TWebSockets::TOpCode::BinaryFrame;
	BitWriter.Write( OpCode, 4 );
	BitWriter.WriteBit(Masked);	//	masked

	uint64 PayloadLength = Data.GetSize();
	//	write length
	if ( PayloadLength > 0xffff )
	{
		//	64 bit length
		BitWriter.Write( 127u, 7 );	//	"theres another 64 bit length"
		BitWriter.Write( PayloadLength, 64 );
	}
	else if ( Data.GetSize() > 125 )
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
		BufferArray<unsigned char,4> MaskKey;
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

	//	now push all the data
	if ( Masked )
	{
		//	todo: encode data!
		assert( !Masked );
		return false;
	}

	static bool TestForUtf8 = true;
	if ( TestForUtf8 && DataIsText )
	{
		for ( int i=0;	i<Data.GetSize();	i++ )
		{
			char DataChar = Data[i];
			if ( !Soy::IsUtf8Char(DataChar) )
			{
				BufferString<100> Debug;
				Debug << "Invalid char in UTF8 message: #" << static_cast<int>(DataChar);
				ofLogNotice( Debug.c_str() );
			}
		}
		//Array<char> NewData( Data.GetSize() );
		//NewData.SetAll('X');
		//MessageData.PushBackArray( NewData );
	}
	
	MessageData.PushBackArray( Data );
	return true;
}


bool TPopServerWebSockets::OnTextMessage(SoyPacketContainer& ReplyPacket,const TString& Data,const TWebSocketClient& Client)
{
	//	"decode" the text into a soypacket. the header is \n seperated like a http header
	SoyLineFeedHeader Header;
	TString Content = Data;
	if ( !Header.PopHeader( const_cast<Array<char>&>( Content.GetArray() ) ) )
		return false;
	if ( Header.mElements.IsEmpty() )
		return false;

	SoyPacketMeta Meta;

	//	make the request simpler by not REQUIRING too much
	BufferArray<SoyRef*,3> HeaderRefs;
	if ( Header.mElements.GetSize() == 1 )
	{
		HeaderRefs.PushBack( &Meta.mType );
	}
	else if ( Header.mElements.GetSize() == 2 )
	{
		HeaderRefs.PushBack( &Meta.mSender );
		HeaderRefs.PushBack( &Meta.mType );
	}
	else if ( Header.mElements.GetSize() > 2 )
	{
		HeaderRefs.PushBack( &Meta.mSoylentProof );
		HeaderRefs.PushBack( &Meta.mSender );
		HeaderRefs.PushBack( &Meta.mType );
	}
	for ( int i=0;	i<HeaderRefs.GetSize();	i++ )
		*HeaderRefs[i] = SoyRef( Header.mElements[i].mKey );
	
	if ( !Meta.IsValid() )
		return false;

	ofLogNotice( Content.c_str() );

	TString ReplyString;
	if ( !OnRequest( ReplyString, Meta, Content ) )
		return false;
	
	//	send reply back
	if ( !TWebSocketClient::EncodeMessageData( ReplyPacket.mData, ReplyString.GetArray(), true ) )
		return false;

	//	test decode of the data we just encoded
	static bool TestSelfDecode = false;
	if ( TestSelfDecode )
	{
		Array<char> TestData;
		bool TestIsTextData;
		TWebSocketClient::DecodeMessageData( TestData, ReplyPacket.mData, TestIsTextData );
		auto& OrigData = ReplyString.GetArray();
		assert( TestData.GetSize() == OrigData.GetSize() );
		assert( TestIsTextData == true );
		for ( int i=0;	i<ofMin(TestData.GetSize(),OrigData.GetSize());	i++ )
		{
			assert( OrigData[i] == TestData[i] );
		}

	}

	return true;
}


bool TPopServerWebSockets::OnBinaryMessage(SoyPacketContainer& ReplyPacket,const Array<char>& Data,const TWebSocketClient& Client)
{
	//	not currently handling any binary messages
	return false;
}


bool TPopServerWebSockets::OnRequest(TString& Reply,const SoyPacketMeta& Meta,const TString& Content)
{
	if ( Meta.GetType() == SoyRef("GetFrames") )
	{
		Array<SoyTime> Frames;
		mApp.mFramePixelContainer.GetTimestamps( Frames );

		{
			//	export xml data
			ofxXmlSettings xml;
			if ( !xml.pushTag( "Frames", xml.addTag( "Frames" ) ) )
				return false;
			for ( int f=0;	f<Frames.GetSize();	f++ )
				Soy::WriteXmlData( xml, "Frame", Frames[f] );
			xml.popTag();
			std::string xmlString;
			xml.copyXmlToString(xmlString);
			Reply = xmlString;
		}

		return true;
	}
	

	//	send error
	Reply << "Unknown request [" << Meta.GetType() << "]";
	return true;
}

