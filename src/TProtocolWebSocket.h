#pragma once

#include <SoyProtocol.h>
#include "TPopServerThread.h"


class SoyWebSocketHeader : public SoyHttpHeader
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

class TWebSocketClient
{
public:
	TWebSocketClient();
	
	TDecodeResult::Type DecodeHandshake(TJob& Job,TStreamBuffer& Stream);
	bool				EncodeHandshake(const TJobReply& Reply,Array<char>& Output);
/*
	bool				OnMessagePacket(Array<char>& Data,bool& IsTextData,const SoyPacketContainer& Packet);	//	decode message and return data
	
	static bool			EncodeMessageData(Array<char>& EncodedData,const Array<char>& DecodedData,const bool DataIsText);
	static bool			DecodeMessageData(Array<char>& DecodedData,const Array<char>& EncodedData,bool& DataIsText);
	*/
public:
	bool				mHasHandshaked;
	bool				mIsWebSocketUpgrade;
	std::string			mProtocol;			//	if it was supplied this is the protocol
	std::string			mVersion;			//	if it was supplied this is the version
};


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



template<class TSUBPROTOCOL>
class TProtocolWebSocket : public TProtocolWebSocketImpl
{
public:
	TProtocolWebSocket() :
		mSubProtocol	( new TSUBPROTOCOL )
	{
	}
	virtual std::shared_ptr<TProtocol>	GetSubProtocol() override	{	return mSubProtocol;	}
	
	virtual TProtocolMeta	GetMeta() const override
	{
		TProtocolMeta SubMeta = mSubProtocol ? mSubProtocol->GetMeta() : TProtocolMeta("null");
		std::stringstream Name;
		Name << "websocket(" << SubMeta.mName << ")";
		return TProtocolMeta( Name.str() );
	}

public:
	std::shared_ptr<TSUBPROTOCOL>		mSubProtocol;
};

