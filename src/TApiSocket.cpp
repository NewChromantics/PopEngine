#include "TApiSocket.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoySocket.h"

namespace ApiSocket
{
	DEFINE_BIND_TYPENAME(UdpBroadcastServer);
	DEFINE_BIND_FUNCTIONNAME(GetAddress);
	DEFINE_BIND_FUNCTIONNAME(Send);
	DEFINE_BIND_FUNCTIONNAME(GetPeers);
}

void ApiSocket::Bind(Bind::TContext& Context)
{
	Context.BindObjectType<TUdpBroadcastServerWrapper>();
}



void TUdpBroadcastServerWrapper::Construct(Bind::TCallback& Params)
{
	auto& This = Params.This<TUdpBroadcastServerWrapper>();

	auto ListenPort = Params.GetArgumentInt(0);

	
	auto OnBinaryMessage = [this](const Array<uint8_t>& Message,SoyRef Sender)
	{
		this->OnMessage( Message, Sender );
	};
	mSocket.reset( new TUdpBroadcastServer(ListenPort, OnBinaryMessage ) );
}


void TUdpBroadcastServerWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiSocket::GetAddress_FunctionName>( GetAddress );
	Template.BindFunction<ApiSocket::Send_FunctionName>( Send );
	Template.BindFunction<ApiSocket::GetPeers_FunctionName>( GetPeers );
}




void TUdpBroadcastServerWrapper::OnMessage(const Array<uint8_t>& Message,SoyRef Sender)
{
	//Array<uint8_t> MessageCopy;
	
	auto SendJsMessage = [=](Bind::TLocalContext& Context)
	{
		auto This = GetHandle(Context);
		auto Func = This.GetFunction("OnMessage");

		Bind::TCallback CallbackParams( Context );
		CallbackParams.SetThis( This );
		CallbackParams.SetArgumentArray( 0, GetArrayBridge(Message) );
		CallbackParams.SetArgumentString( 1, Sender.ToString() );
		try
		{
			Func.Call( CallbackParams );
		}
		catch(std::exception& e)
		{
			std::Debug << __func__ << " callback exception: " << e.what() << std::endl;
		}
	};
	
	GetContext().Queue( SendJsMessage );
}


void TSocketWrapper::GetAddress(Bind::TCallback& Params)
{
	auto& This = Params.This<TSocketWrapper>();
	auto ThisSocket = This.GetSocket();
	if ( !ThisSocket )
		throw Soy::AssertException("Socket not allocated");

	//	we return all the addresses
	Array<Bind::TObject> Addresses;
	auto AppendAddress = [&](std::string& InterfaceName,SoySockAddr& InterfaceAddress)
	{
		std::stringstream AddressStr;
		AddressStr << InterfaceAddress;

		auto Address = Params.mContext.CreateObjectInstance( Params.mLocalContext );
		Address.SetString("Address", AddressStr.str());
		Address.SetString("Name", InterfaceName);
		Addresses.PushBack(Address);
	};
	ThisSocket->GetSocketAddresses( AppendAddress );
	
	Params.Return( GetArrayBridge(Addresses) );
}


void TSocketWrapper::Send(Bind::TCallback& Params)
{
	auto& This = Params.This<TSocketWrapper>();
	auto ThisSocket = This.GetSocket();
	if ( !ThisSocket )
		throw Soy::AssertException("Socket not allocated");

	auto SenderStr = Params.GetArgumentString(0);
	auto Sender = SoyRef( SenderStr );

	Array<uint8_t> Data;
	//v8::EnumArray<v8::Uint8Array>(DataHandle,GetArrayBridge(Data) );
	Params.GetArgumentArray( 1, GetArrayBridge(Data) );

	auto& Socket = *ThisSocket;
	auto Connection = Socket.GetConnection( Sender );
	auto DataChars = GetArrayBridge(Data).GetSubArray<char>(0,Data.GetSize());
	Connection.Send( GetArrayBridge(DataChars), Socket.IsUdp() );
}


void TSocketWrapper::GetPeers(Bind::TCallback& Params)
{
	auto& This = Params.This<TSocketWrapper>();
	auto ThisSocket = This.GetSocket();
	if ( !ThisSocket )
		throw Soy::AssertException("Socket not allocated");

	//	get connection references
	Array<std::string> PeerNames;
	auto& Socket = *ThisSocket;
	auto EnumPeer = [&](SoyRef ConnectionRef,SoySocketConnection Connection)
	{
		PeerNames.PushBack( ConnectionRef.ToString() );
	};
	Socket.EnumConnections( EnumPeer );
	
	Params.Return( GetArrayBridge(PeerNames) );
}



TUdpBroadcastServer::TUdpBroadcastServer(uint16_t ListenPort,std::function<void(const Array<uint8_t>&,SoyRef)> OnBinaryMessage) :
	SoyWorkerThread		( Soy::StreamToString(std::stringstream()<<"UdpBroadcastServer("<<ListenPort<<")"), SoyWorkerWaitMode::Sleep ),
	mOnBinaryMessage	( OnBinaryMessage )
{
	mSocket.reset( new SoySocket() );
	auto Broadcast = true;
	mSocket->CreateUdp(Broadcast);
	mSocket->ListenUdp(ListenPort);
	/*
	mSocket->mOnConnect = [=](SoyRef ClientRef)
	{
		AddClient(ClientRef);
	};
	mSocket->mOnDisconnect = [=](SoyRef ClientRef)
	{
		RemoveClient(ClientRef);
	};
	*/
	Start();
	
}
	

bool TUdpBroadcastServer::Iteration()
{
	if ( !mSocket )
		return false;
	
	if ( !mSocket->IsCreated() )
		return true;
	
	//	non blocking so lets just do everything in a loop
	Array<char> RecvBuffer;
	Array<uint8_t> RecvBuffer8;
	auto RecvFromConnection = [&](SoyRef ConnectionRef,SoySocketConnection Connection)
	{
		RecvBuffer.Clear();
		
		//	on udp ConnectionRef is us!
		auto Sender = Connection.Recieve( GetArrayBridge(RecvBuffer), *mSocket );
		if ( !Sender.IsValid() || RecvBuffer.IsEmpty() )
			return;
		
		//	cast buffer. Would prefer SoySocket to be uint8
		auto RecvBufferCastTo8 = GetArrayBridge(RecvBuffer).GetSubArray<uint8_t>( 0, RecvBuffer.GetSize() );
		RecvBuffer8.Copy( RecvBufferCastTo8 );
		
		this->mOnBinaryMessage( RecvBuffer8, Sender );
	};
	mSocket->EnumConnections( RecvFromConnection );

	return true;
}


