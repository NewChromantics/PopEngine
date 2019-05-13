#include "TApiDll.h"
#include "SoyRuntimeLibrary.h"

namespace ApiDll
{
	const char Namespace[] = "Pop.Dll";

	DEFINE_BIND_TYPENAME(Library);
	DEFINE_BIND_FUNCTIONNAME(BindFunction);
	DEFINE_BIND_FUNCTIONNAME(CallFunction);
}


void ApiDll::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindObjectType<TDllWrapper>( Namespace );
}


void TDllWrapper::Construct(Bind::TCallback& Params)
{
	auto Filename = Params.GetArgumentFilename(0);
	
	mLibrary.reset( new Soy::TRuntimeLibrary(Filename) );
}


void TDllWrapper::CreateTemplate(Bind::TTemplate& Template)
{
	Template.BindFunction<ApiDll::BindFunction_FunctionName>( BindFunction );
	Template.BindFunction<ApiDll::CallFunction_FunctionName>( CallFunction );
}


template<typename RETURNTYPE>
class TFunction_N : public ApiDll::TFunctionBase
{
public:
	template<typename FUNCTYPE>
	static void		SetFunction(std::function<FUNCTYPE>& FunctionPtr,void* FunctionAddress)
	{
		//	cast & assign
		FUNCTYPE* ff = reinterpret_cast<FUNCTYPE*>(FunctionAddress);
		FunctionPtr = ff;
	}
	
	template<typename FUNCTIONTYPE,typename ...ARGUMENTS,typename X=RETURNTYPE> typename std::enable_if<std::is_same<X,void>::value>::type
	CallImpl(FUNCTIONTYPE& Function,Bind::TCallback& Params,ARGUMENTS...Arguments)
	{
		if ( Function == nullptr )
			throw Soy::AssertException("Functor is null");
		Function( Arguments... );
	}
	
	template<typename FUNCTIONTYPE,typename ...ARGUMENTS,typename X=RETURNTYPE> typename std::enable_if<!std::is_same<X,void>::value>::type
	CallImpl(FUNCTIONTYPE& Function,Bind::TCallback& Params,ARGUMENTS...Arguments)
	{
		if ( Function == nullptr )
			throw Soy::AssertException("Functor is null");
		auto Return = Function( Arguments... );
		Params.Return(Return);
	}
};

template<typename RETURNTYPE>
class TFunction_0 : public TFunction_N<RETURNTYPE>
{
public:
	TFunction_0(void* FunctionAddress)
	{
		SetFunction( mFunctor, FunctionAddress );
	}
	
	virtual void 	Call(Bind::TCallback& Params)
	{
		TFunction_N<RETURNTYPE>::CallImpl( mFunctor, Params );
	}
	
	
public:
	std::function<RETURNTYPE()>	mFunctor;
};


template<typename RETURNTYPE,typename A>
class TFunction_A : public TFunction_N<RETURNTYPE>
{
public:
	TFunction_A(void* FunctionAddress)
	{
		SetFunction( mFunctor, FunctionAddress );
	}
	
	virtual void 	Call(Bind::TCallback& Params)
	{
		auto a = Bind::FromValue<A>( Params.GetContextRef(), Params.GetArgumentValue(0) );
		
		TFunction_N<RETURNTYPE>::CallImpl( mFunctor, Params, a );
	}
	
	
public:
	std::function<RETURNTYPE(A)>	mFunctor;
};

template<typename RETURNTYPE,typename A,typename B>
class TFunction_AB : public TFunction_N<RETURNTYPE>
{
public:
	TFunction_AB(void* FunctionAddress)
	{
		SetFunction( mFunctor, FunctionAddress );
	}
	
	virtual void 	Call(Bind::TCallback& Params)
	{
		auto a = Bind::FromValue<A>( Params.GetContextRef(), Params.GetArgumentValue(0) );
		auto b = Bind::FromValue<B>( Params.GetContextRef(), Params.GetArgumentValue(1) );
		
		TFunction_N<RETURNTYPE>::CallImpl( mFunctor, Params, a, b );
	}
	
	
public:
	std::function<RETURNTYPE(A,B)>	mFunctor;
};


template<typename RETURNTYPE,typename A,typename B,typename C>
class TFunction_ABC : public TFunction_N<RETURNTYPE>
{
public:
	TFunction_ABC(void* FunctionAddress)
	{
		SetFunction( mFunctor, FunctionAddress );
	}
	
	virtual void 	Call(Bind::TCallback& Params)
	{
		auto a = Bind::FromValue<A>( Params.GetContextRef(), Params.GetArgumentValue(0) );
		auto b = Bind::FromValue<B>( Params.GetContextRef(), Params.GetArgumentValue(1) );
		auto c = Bind::FromValue<C>( Params.GetContextRef(), Params.GetArgumentValue(2) );
		
		TFunction_N<RETURNTYPE>::CallImpl( mFunctor, Params, a, b, c );
	}
	
public:
	std::function<RETURNTYPE(A,B,C)>	mFunctor;
};


template<typename RETURNTYPE,typename A,typename B,typename C>
std::shared_ptr<ApiDll::TFunctionBase> AllocFunction_ABC(void* FunctionAddress,ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_ABC<RETURNTYPE,A,B,C>(FunctionAddress) );
	
	throw Soy::AssertException("Need to handle more args");
}


template<typename RETURNTYPE,typename A,typename B>
std::shared_ptr<ApiDll::TFunctionBase> AllocFunction_AB(void* FunctionAddress,ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_AB<RETURNTYPE,A,B>( FunctionAddress ) );
	
	auto NextType = TypeStack.PopAt(0);
	
	if ( NextType == "uint8_t" )	{	return AllocFunction_ABC<RETURNTYPE,A,B,uint8_t>( FunctionAddress, TypeStack );	}
	if ( NextType == "uint16_t" )	{	return AllocFunction_ABC<RETURNTYPE,A,B,uint16_t>( FunctionAddress, TypeStack );	}
	if ( NextType == "uint32_t" )	{	return AllocFunction_ABC<RETURNTYPE,A,B,uint32_t>( FunctionAddress, TypeStack );	}
	
	std::stringstream Error;
	Error << "Unhandled type " << NextType;
	throw Soy::AssertException(Error);
}


template<typename RETURNTYPE,typename A>
std::shared_ptr<ApiDll::TFunctionBase> AllocFunction_A(void* FunctionAddress,ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_A<RETURNTYPE,A>( FunctionAddress ) );
	
	auto NextType = TypeStack.PopAt(0);
	
	if ( NextType == "uint8_t" )	{	return AllocFunction_AB<RETURNTYPE,A,uint8_t>( FunctionAddress, TypeStack );	}
	if ( NextType == "uint16_t" )	{	return AllocFunction_AB<RETURNTYPE,A,uint16_t>( FunctionAddress, TypeStack );	}
	if ( NextType == "uint32_t" )	{	return AllocFunction_AB<RETURNTYPE,A,uint32_t>( FunctionAddress, TypeStack );	}
	
	std::stringstream Error;
	Error << "Unhandled type " << NextType;
	throw Soy::AssertException(Error);
}

template<typename RETURNTYPE>
std::shared_ptr<ApiDll::TFunctionBase> AllocFunction_ReturnType(void* FunctionAddress,ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_0<RETURNTYPE>( FunctionAddress ) );
	
	auto NextType = TypeStack.PopAt(0);
	
	if ( NextType == "uint8_t" )	{	return AllocFunction_A<RETURNTYPE,uint8_t>( FunctionAddress, TypeStack );	}
	if ( NextType == "uint16_t" )	{	return AllocFunction_A<RETURNTYPE,uint16_t>( FunctionAddress, TypeStack );	}
	if ( NextType == "uint32_t" )	{	return AllocFunction_A<RETURNTYPE,uint32_t>( FunctionAddress, TypeStack );	}
	
	std::stringstream Error;
	Error << "Unhandled type " << NextType;
	throw Soy::AssertException(Error);
}

std::shared_ptr<ApiDll::TFunctionBase> AllocFunction(void* FunctionAddress,const std::string& ReturnType,ArrayBridge<std::string>& TypeStack)
{
	if ( ReturnType == "" )			{	return AllocFunction_ReturnType<void>( FunctionAddress, TypeStack );	}
	if ( ReturnType == "void" )		{	return AllocFunction_ReturnType<void>( FunctionAddress, TypeStack );	}
	if ( ReturnType == "uint8_t" )	{	return AllocFunction_ReturnType<uint8_t>( FunctionAddress, TypeStack );	}
	if ( ReturnType == "uint16_t" )	{	return AllocFunction_ReturnType<uint16_t>( FunctionAddress, TypeStack );	}
	if ( ReturnType == "uint32_t" )	{	return AllocFunction_ReturnType<uint32_t>( FunctionAddress, TypeStack );	}

	std::stringstream Error;
	Error << "Unhandled return type " << ReturnType;
	throw Soy::AssertException(Error);
}

void TDllWrapper::BindFunction(Bind::TCallback& Params)
{
	auto& This = Params.This<TDllWrapper>();
	
	auto SymbolName = Params.GetArgumentString(0);
	Array<std::string> ArgTypes;
	if ( !Params.IsArgumentUndefined(1) )
		Params.GetArgumentArray( 1, GetArrayBridge(ArgTypes) );
	
	std::string ReturnType;
	if ( !Params.IsArgumentUndefined(2) )
		ReturnType = Params.GetArgumentString( 2 );
	
	auto& Library = *This.mLibrary;
	
	//	get symbol
	auto* SymbolAddress = Library.GetSymbol( SymbolName.c_str() );
	if ( !SymbolAddress )
	{
		std::stringstream Error;
		Error << "Didn't find symbol " << SymbolName << " in " << Library.mLibraryName;
		throw Soy::AssertException(Error);
	}
	
	//	create a new function wrapper
	auto ArgTypesBridge = GetArrayBridge(ArgTypes);
	auto Func = AllocFunction( SymbolAddress, ReturnType, ArgTypesBridge );

	//	todo: check function doesn't already exist in map
	This.mFunctions[SymbolName] = Func;

	//	now generate a function in javascript, which just calls our wrapper (CallFunction)
	//	then return that as the function that the user can call
	
	//	todo:
	Params.Return("Todo: return a callable function bound to this. For now: This.Call( FunctionName, arg0, arg1 )");
}



void TDllWrapper::CallFunction(Bind::TCallback& Params)
{
	auto& This = Params.This<TDllWrapper>();
	
	auto FunctionName = Params.GetArgumentString(0);
	
	auto& Function = This.GetFunction( FunctionName );
	
	//	this passes all the arguments, and the return
	//	but for now, as a hack, we pop off argument 0 which is the function name
	//	need a nicer bridge for this as mArguments should be private
	Params.mArguments.RemoveBlock(0,1);
	
	try
	{
		Function.Call( Params );
	}
	catch(std::exception& e)
	{
		std::Debug << e.what() << std::endl;
		throw;
	}
}

ApiDll::TFunctionBase& TDllWrapper::GetFunction(const std::string& FunctionName)
{
	auto it = mFunctions.find( FunctionName );
	if ( it == mFunctions.end() )
	{
		std::stringstream Error;
		Error << this->mLibrary->mLibraryName << " has no bound function \"" << FunctionName << "\"";
		throw Soy::AssertException(Error);
	}
	
	auto pFunction = it->second;
	return *pFunction;
}




