#pragma once

#include <SoyProtocol.h>
#include <SoyHttp.h>


namespace WebSocket
{
	class TRequestProtocol;
	class THandshakeMeta;
	class THandshakeResponseProtocol;
	class TMessageHeader;
	class TMessage;
	
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
		DECLARE_SOYENUM( WebSocket::TOpCode );
	}

}


/*

 
class SoyWebSocketHeader : public Http::TCommonProtocol
{
public:
	virtual bool			IsValid() override
	{
		if ( mWebSocketKey.empty() )
			return false;
		return SoyHttpHeader::IsValid();
	}
	
	std::string				GetReplyKey() const;
	virtual bool			PushRawHeader(const std::string& Header) override;
	
public:
	bool					mIsWebSocketUpgrade;
	std::string				mWebSocketKey;
	
};

*/



//	gr: name is a little misleading, it's the websocket connection meta
class WebSocket::THandshakeMeta
{
public:
	THandshakeMeta();
	
//	TDecodeResult::Type DecodeHandshake(TJob& Job,TStreamBuffer& Stream);
//	bool				EncodeHandshake(const TJobReply& Reply,Array<char>& Output);
/*
	bool				OnMessagePacket(Array<char>& Data,bool& IsTextData,const SoyPacketContainer& Packet);	//	decode message and return data
	
	static bool			EncodeMessageData(Array<char>& EncodedData,const Array<char>& DecodedData,const bool DataIsText);
	static bool			DecodeMessageData(Array<char>& DecodedData,const Array<char>& EncodedData,bool& DataIsText);
	*/
	
	std::string			GetReplyKey() const;
	bool				IsCompleted() const	{	return mIsWebSocketUpgrade && mWebSocketKey.length()!=0 && mVersion.length()!=0;	}
	
public:
	//	protocol and version are optional
	std::string			mProtocol;
	std::string			mVersion;
	bool				mIsWebSocketUpgrade;
	std::string			mWebSocketKey;
};


class WebSocket::TMessage
{
public:
	TMessage(std::function<void(const std::string&)> OnTextMessage,std::function<void(const Array<uint8_t>&)> OnBinaryMessage) :
		mOnTextMessage		( OnTextMessage ),
		mOnBinaryMessage	( OnBinaryMessage )
	{
	}

	void				PushMessageData(TOpCode::Type PayloadFormat,bool IsLastPayload,const ArrayBridge<char>&& Payload);
	void				PushTextMessageData(const ArrayBridge<char>& Payload,bool IsLastPayload);
	void				PushBinaryMessageData(const ArrayBridge<char>& Payload,bool IsLastPayload);
	
public:
	std::function<void(const std::string&)>		mOnTextMessage;
	std::function<void(const Array<uint8_t>&)>	mOnBinaryMessage;
	Array<uint8_t>		mBinaryData;	//	binary message
	std::string			mTextData;		//	text message
};

class WebSocket::TMessageHeader
{
public:
	TMessageHeader() :
		Length		( 0 ),
		Length16	( 0 ),
		LenMostSignificant	( 0 ),
		Length64	( 0 ),
		Fin			( 1 ),
		Reserved	( 0 ),
		OpCode		( WebSocket::TOpCode::Invalid ),
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
	
	TOpCode::Type	GetOpCode() const	{	return TOpCode::Validate( OpCode );	}
	bool		IsText() const			{	return OpCode == TOpCode::TextFrame;	}
	size_t		GetLength() const;
	std::string	GetMaskKeyString() const;
	bool		IsValid(std::stringstream& Error) const;
	bool		Decode(TStreamBuffer& Data);		//	returns false if not got enough data. throws on error
	bool		Encode(ArrayBridge<char>& Data,const ArrayBridge<char>& MessageData,std::stringstream& Error);
};



//	a websocket client
class WebSocket::TRequestProtocol : public Http::TRequestProtocol
{
public:
	TRequestProtocol() : mHandshake(* new THandshakeMeta() ), mMessage( *new TMessage(nullptr,nullptr) )	{	throw Soy::AssertException("Should not be called");	}
	TRequestProtocol(THandshakeMeta& Handshake,TMessage& Message) :
		mHandshake	( Handshake ),
		mMessage	( Message )
	{
	}

	virtual TProtocolState::Type	Decode(TStreamBuffer& Buffer) override;
	virtual bool					ParseSpecificHeader(const std::string& Key,const std::string& Value) override;
	
protected:
	TProtocolState::Type	DecodeBody(TMessageHeader& Header,TStreamBuffer& Buffer);

public:
	THandshakeMeta&		mHandshake;	//	persistent handshake data etc
	TMessage&			mMessage;	//	persistent message for multi-frame messages
};


class WebSocket::THandshakeResponseProtocol : public Http::TResponseProtocol
{
public:
	THandshakeResponseProtocol(const THandshakeMeta& Handshake);
};



/*
class TProtocolWebSocketImpl : public TProtocol
{
public:
	TProtocolWebSocketImpl(std::string RootCommand="help") :
		mRootCommand	( RootCommand )
	{
	}

	virtual TDecodeResult::Type	DecodeHeader(TJob& Job,TStreamBuffer& Stream) override;
	virtual TDecodeResult::Type	DecodeData(TJob& Job,TStreamBuffer& Stream) override;

	virtual bool		Encode(const TJobReply& Reply,std::stringstream& Output) override;
	virtual bool		Encode(const TJobReply& Reply,Array<char>& Output) override;
	virtual bool		Encode(const TJob& Job,std::stringstream& Output) override;
	virtual bool		Encode(const TJob& Job,Array<char>& Output) override;

	virtual bool		FixParamFormat(TJobParam& Param,std::stringstream& Error) override;
	
protected:
	virtual std::shared_ptr<TProtocol>	GetSubProtocol()		{	return std::shared_ptr<TProtocol>();	}
	std::shared_ptr<TWebSocketClient>	GetClientMeta(SoyRef ClientRef);
	
public:
	std::map<SoyRef,std::shared_ptr<TWebSocketClient>>	mClientMeta;
	std::string					mRootCommand;	//	if root url is requested (no command) then we issue this
};
*/

