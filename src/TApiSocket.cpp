#include "TApiSocket.h"
#include "SoyOpenglWindow.h"
#include "TApiCommon.h"
#include "SoySocket.h"

const char UdpBroadcastServer_TypeName[] = "UdpBroadcastServer";

const char GetAddress_FunctionName[] = "GetAddress";
const char Send_FunctionName[] = "Send";
const char GetPeers_FunctionName[] = "GetPeers";


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
	Template.BindFunction<GetAddress_FunctionName>( GetAddress );
	Template.BindFunction<Send_FunctionName>( Send );
	Template.BindFunction<GetPeers_FunctionName>( GetPeers );
}




void TUdpBroadcastServerWrapper::OnMessage(const Array<uint8_t>& Message,SoyRef Sender)
{
	//Array<uint8_t> MessageCopy;
	
	auto SendJsMessage = [=](Bind::TContext& Context)
	{
		auto This = GetHandle();
		auto Func = This.GetFunction("OnMessage");

		Bind::TCallback CallbackParams( Context );
		CallbackParams.SetThis( This );
		CallbackParams.SetArgumentArray( 0, GetArrayBridge(Message) );
		CallbackParams.SetArgumentString( 1, Sender.ToString() );
		try
		{
			Context.Execute( CallbackParams );
		}
		catch(std::exception& e)
		{
			std::Debug << __func__ << " callback exception: " << e.what() << std::endl;
		}
	};
	
	mContext.Queue( SendJsMessage );
}


void TSocketWrapper::GetAddress(Bind::TCallback& Params)
{
	auto& This = Params.This<TSocketWrapper>();
	auto ThisSocket = This.GetSocket();
	if ( !ThisSocket )
		throw Soy::AssertException("Socket not allocated");

	//	we return all the addresses with , seperator
	//	high level can pick the best one
	std::stringstream Addresses;
	auto AppendAddress = [&](std::string& InterfaceName,SoySockAddr& InterfaceAddress)
	{
		Addresses << InterfaceAddress << ',';
	};
	ThisSocket->GetSocketAddresses( AppendAddress );
	auto AddressesStr = Addresses.str();
	Soy::StringTrimRight( AddressesStr, ',' );
	
	Params.Return( AddressesStr );
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
	
	//	return array of names
	auto GetHandle = [&](size_t Index)
	{
		return PeerNames[Index];
	};
	auto PeerNamesArray = Params.mContext.CreateArray( PeerNames.GetSize(), GetHandle );
	Params.Return( PeerNamesArray );
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


