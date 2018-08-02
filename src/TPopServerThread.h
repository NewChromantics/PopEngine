#pragma once

//#include "main.h"
//#include "TBlob.h"
//#include "TPopOutputter.h"
//#include <SoySocket.h>
//#include "TChannelSocket.h"

#include <SoyThread.h>



class TPopServerThread : public SoyThread
{
public:
	TPopServerThread(TPopApp& App,uint16 ListenPort);

protected:
	virtual std::shared_ptr<SoySocket>	AllocSocket()=0;
	virtual void					OnClientJoin(const SoyAddress& Client)=0;
	virtual void					OnClientLeft(const SoyAddress& Client)=0;
	virtual bool					OnRecievePacket(SoyPacketContainer& ReplyPacket,const SoyPacketContainer& Packet)=0;	//	recieve packet, returns the reply
	bool							SendPacket(const SoyPacketContainer& Packet);

private:
	virtual bool					Init();
	virtual void					threadedFunction();
	void							OnSocketRecievePacket(SoyNet::TSocket*& Socket);

protected:
	uint16							mListenPort;
	ofPtr<SoyNet::TSocket>			mSocket;
};


//	templated intermediate class to merge server & protocol. 
//	Done this way so we can define protocol and server thread out of header
template<typename TPROTOCOLHANDLER>
class TPopServerProtocol : public TPopServerThread, public TPROTOCOLHANDLER
{
public:
	TPopServerProtocol(TPopApp& App,uint16 ListenPort) :
		TPopServerThread	( App, ListenPort )
	{
	}

	virtual void					OnClientJoin(const SoyNet::TAddress& Client)		{	TPROTOCOLHANDLER::OnClientJoin( Client );	}
	virtual void					OnClientLeft(const SoyNet::TAddress& Client)		{	TPROTOCOLHANDLER::OnClientLeft( Client );	}
	virtual bool					OnRecievePacket(SoyPacketContainer& ReplyPacket,const SoyPacketContainer& Packet)	{	return TPROTOCOLHANDLER::OnRecievePacket( ReplyPacket, Packet );	}
	
	virtual bool					SendPacket(const SoyPacketContainer& Packet)		{	return TPopServerThread::SendPacket( Packet );	}

	virtual ofPtr<SoyNet::TSocket>	AllocSocket()										{	return TPROTOCOLHANDLER::AllocSocket();	}
};


class TProtocolHandler
{
public:
	virtual ~TProtocolHandler()	{};

	virtual bool					SendPacket(const SoyPacketContainer& Packet)=0;
};


class TWebSocketClient
{
public:
	TWebSocketClient(SoyNet::TAddress Address=SoyNet::TAddress());
	
	inline bool			operator==(const SoyNet::TAddress& Address) const	{	return mAddress.Equals( Address, true );	}

	bool				OnHandshakePacket(SoyPacketContainer& ReplyPacket,const SoyPacketContainer& Packet);
	bool				OnMessagePacket(Array<char>& Data,bool& IsTextData,const SoyPacketContainer& Packet);	//	decode message and return data

	static bool			EncodeMessageData(Array<char>& EncodedData,const Array<char>& DecodedData,const bool DataIsText);
	static bool			DecodeMessageData(Array<char>& DecodedData,const Array<char>& EncodedData,bool& DataIsText);

public:
	SoyNet::TAddress	mAddress;
	bool				mHasHandshaked;
	BufferString<200>	mProtocol;			//	if it was supplied this is the protocol
};

class TProtocolWebSockets : public TProtocolHandler
{
public:
	ofPtr<SoyNet::TSocket>	AllocSocket()	{	return ofPtr<SoyNet::TSocket>( new SoyNet::TSocketTCP(false) );	}
	void					OnClientJoin(const SoyNet::TAddress& Client);
	void					OnClientLeft(const SoyNet::TAddress& Client);
	bool					OnRecievePacket(SoyPacketContainer& ReplyPacket,const SoyPacketContainer& Packet);

	virtual bool			OnTextMessage(SoyPacketContainer& ReplyPacket,const TString& Data,const TWebSocketClient& Client)=0;
	virtual bool			OnBinaryMessage(SoyPacketContainer& ReplyPacket,const Array<char>& Data,const TWebSocketClient& Client)=0;

	bool					SendTextMessage(const std::string& Text);

	bool					HasClients() const	{	return !mClients.IsEmpty();	}

public:
	Array<TWebSocketClient>	mClients;
};


class TPopServerWebSockets : public TPopServerProtocol<TProtocolWebSockets>
{
public:
	TPopServerWebSockets(TPopApp& App,uint16 ListenPort) :
		TPopServerProtocol	( App, ListenPort )
	{
	}

	virtual bool			OnRequest(TString& Reply,const SoyPacketMeta& Meta,const TString& Content);

protected:
	virtual bool			OnTextMessage(SoyPacketContainer& ReplyPacket,const TString& Data,const TWebSocketClient& Client);
	virtual bool			OnBinaryMessage(SoyPacketContainer& ReplyPacket,const Array<char>& Data,const TWebSocketClient& Client);
};



class SoyHttpHeaderElement
{
public:
	SoyHttpHeaderElement(const char* Key=nullptr,const char* Value=nullptr) :
		mKey	( Key ),
		mValue	( Value )
	{
	}

public:
	BufferString<200>	mKey;
	BufferString<200>	mValue;
};

DECLARE_NONCOMPLEX_TYPE( SoyHttpHeaderElement );


class SoyLineFeedHeader
{
public:
	virtual ~SoyLineFeedHeader()	{}

	bool			PopHeader(Array<char>& HttpResponse);				//	return false on error
	virtual bool	PushHeader(const SoyHttpHeaderElement& Element)		{	mElements.PushBack( Element );	return true;	}
	virtual bool	PushRawHeader(const BufferString<200>& Header)
	{
		auto& NewElement = mElements.PushBack();
		BufferArray<BufferString<200>,2> Parts;
		Header.Split( Parts, ':', true, 2 );
		NewElement.mKey = Parts[0];
		if ( Parts.GetSize() > 1 )
			NewElement.mValue = Parts[1];
		return true;
	}

	virtual bool	IsValid()
	{
		if ( mElements.IsEmpty() )
			return false;
		return true;
	}

	BufferString<200>*	GetHeader(const char* Key) 
	{
		for ( int i=0;	i<mElements.GetSize();	i++ )
		{
			auto& Element = mElements[i];
			if ( Element.mKey.StartsWith( Key ) )
				return &Element.mValue;
		}
		return nullptr;
	}

protected:
	TString					GetString() const;


public:
	Array<SoyHttpHeaderElement>	mElements;
	Array<char>					mData;		//	not in the header
};


class SoyHttpHeader : public SoyLineFeedHeader
{
public:
	virtual bool	IsValid()
	{
		if ( !SoyLineFeedHeader::IsValid() )
			return false;

		//	should have found an url and method
		//	gr: using method as url of "/" is empty
		if ( mMethod.empty() )
			return false;

		return true;
	}

	bool			GetResponseData(Array<char>& ResponseData,int ResponseCode,const char* ResponseString) const
	{
		TString Http;
		Http << "HTTP/1.1 " << ResponseCode << " " << ResponseString << "\r\n";
		Http << GetString();

		//	note: this is DATA because we MUST NOT have a trailing terminator. Fine for HTTP, not for websocket/binary data!
		ResponseData = Http.GetArray();
		assert( ResponseData.GetBack() == '\0' );
		ResponseData.PopBack();
		return true;
	}

protected:
	virtual bool	PushRawHeader(const BufferString<200>& Header);

public:
	std::string			mUrl;
	std::string			mMethod;	//	get/post
	Array<SoyPair<TString>>	mUrlParams;
};


class SoyWebSocketHeader : public SoyHttpHeader
{
public:
	virtual bool			IsValid()
	{
		if ( mWebSocketKey.IsEmpty() )
			return false;
		return SoyHttpHeader::IsValid();
	}

	BufferString<200>		GetReplyKey() const;
	virtual bool			PushRawHeader(const BufferString<200>& Header);

public:
	bool					mIsWebSocketUpgrade;
	BufferString<200>		mWebSocketKey;

};
