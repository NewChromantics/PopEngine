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


template<typename RETURNTYPE,typename A,typename B,typename C,typename D>
class TFunction_ABCD : public TFunction_N<RETURNTYPE>
{
public:
	TFunction_ABCD(void* FunctionAddress)
	{
		SetFunction( mFunctor, FunctionAddress );
	}
	
	virtual void 	Call(Bind::TCallback& Params)
	{
		auto a = Bind::FromValue<A>( Params.GetContextRef(), Params.GetArgumentValue(0) );
		auto b = Bind::FromValue<B>( Params.GetContextRef(), Params.GetArgumentValue(1) );
		auto c = Bind::FromValue<C>( Params.GetContextRef(), Params.GetArgumentValue(2) );
		auto d = Bind::FromValue<D>( Params.GetContextRef(), Params.GetArgumentValue(3) );
		
		TFunction_N<RETURNTYPE>::CallImpl( mFunctor, Params, a, b, c, d );
	}
	
public:
	std::function<RETURNTYPE(A,B,C,D)>	mFunctor;
};


template<typename RETURNTYPE,typename A,typename B,typename C,typename D,typename E>
class TFunction_ABCDE : public TFunction_N<RETURNTYPE>
{
public:
	TFunction_ABCDE(void* FunctionAddress)
	{
		SetFunction( mFunctor, FunctionAddress );
	}
	
	virtual void 	Call(Bind::TCallback& Params)
	{
		auto a = Bind::FromValue<A>( Params.GetContextRef(), Params.GetArgumentValue(0) );
		auto b = Bind::FromValue<B>( Params.GetContextRef(), Params.GetArgumentValue(1) );
		auto c = Bind::FromValue<C>( Params.GetContextRef(), Params.GetArgumentValue(2) );
		auto d = Bind::FromValue<D>( Params.GetContextRef(), Params.GetArgumentValue(3) );
		auto e = Bind::FromValue<E>( Params.GetContextRef(), Params.GetArgumentValue(4) );
		
		TFunction_N<RETURNTYPE>::CallImpl( mFunctor, Params, a, b, c, d, e );
	}
	
public:
	std::function<RETURNTYPE(A,B,C,D,E)>	mFunctor;
};


/*



template<typename RETURNTYPE,typename A,typename B,typename C,typename D>
std::shared_ptr<ApiDll::TFunctionBase> AllocFunction_ABCD(void* FunctionAddress,ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_ABCD<RETURNTYPE,A,B,C,D>( FunctionAddress ) );
	
	auto NextType = TypeStack.PopAt(0);
	
	throw Soy::AssertException("too many args");
}

template<typename RETURNTYPE,typename A,typename B,typename C>
std::shared_ptr<ApiDll::TFunctionBase> AllocFunction_ABC(void* FunctionAddress,ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_ABC<RETURNTYPE,A,B,C>( FunctionAddress ) );
	
	auto NextType = TypeStack.PopAt(0);
	
	
#define DEFINE_IF_TYPE(TYPE)	\
if ( NextType == #TYPE )	{	return AllocFunction_ABCD<RETURNTYPE,A,B,C,TYPE>( FunctionAddress, TypeStack );	}
	
	DEFINE_IF_TYPE(uint8_t);
	DEFINE_IF_TYPE(uint16_t);
	DEFINE_IF_TYPE(uint32_t);
	DEFINE_IF_TYPE(void*);
#undef DEFINE_IF_TYPE
	
	std::stringstream Error;
	Error << "Unhandled type " << NextType;
	throw Soy::AssertException(Error);
}


template<typename RETURNTYPE,typename A,typename B>
std::shared_ptr<ApiDll::TFunctionBase> AllocFunction_AB(void* FunctionAddress,ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_AB<RETURNTYPE,A,B>( FunctionAddress ) );
	
	auto NextType = TypeStack.PopAt(0);
	

#define DEFINE_IF_TYPE(TYPE)	\
if ( NextType == #TYPE )	{	return AllocFunction_ABC<RETURNTYPE,A,B,TYPE>( FunctionAddress, TypeStack );	}
	
	DEFINE_IF_TYPE(uint8_t);
	DEFINE_IF_TYPE(uint16_t);
	DEFINE_IF_TYPE(uint32_t);
	DEFINE_IF_TYPE(void*);
#undef DEFINE_IF_TYPE
	
	std::stringstream Error;
	Error << "Unhandled type " << NextType;
	throw Soy::AssertException(Error);
}

#define DEFINE_IF_TYPE(TYPE,NEXTFUNC)	\
if ( NextType == #TYPE )	{	return NEXTFUNC<RETURNTYPE,ABC...,TYPE>( FunctionAddress, TypeStack );	}

template<typename RETURNTYPE,typename ...ABC>
std::shared_ptr<ApiDll::TFunctionBase> AllocFunction_A(void* FunctionAddress,ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_A<RETURNTYPE,ABC...>( FunctionAddress ) );
	
	auto NextType = TypeStack.PopAt(0);

	DEFINE_IF_TYPE(uint8_t,AllocFunction_AB);
	DEFINE_IF_TYPE(uint16_t,AllocFunction_AB);
	DEFINE_IF_TYPE(uint32_t,AllocFunction_AB);
	DEFINE_IF_TYPE(void*,AllocFunction_AB);
	
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

#define DEFINE_IF_TYPE(TYPE)	\
if ( NextType == #TYPE )	{	return AllocFunction_A<RETURNTYPE,TYPE>( FunctionAddress, TypeStack );	}

	DEFINE_IF_TYPE(uint8_t);
	DEFINE_IF_TYPE(uint16_t);
	DEFINE_IF_TYPE(uint32_t);
	DEFINE_IF_TYPE(void*);
#undef DEFINE_IF_TYPE

	std::stringstream Error;
	Error << "Unhandled type " << NextType;
	throw Soy::AssertException(Error);
}
*/


class TDcFunction : public ApiDll::TFunctionBase
{
public:
	TDcFunction(ArrayBridge<std::string>& ArgumentTypes);
	~TDcFunction()
	{
		dcFree( mVm );
	}
	
	Array<std::function<void(Bind::TCallback&,size_t)>>	mSetArguments;
	Array<std::function<void(Bind::TCallback&)>>		mCall;
	
	virtual void 	Call(Bind::TCallback& Params) override
	{
		dcReset(mVm);
		
		for ( auto i=0;	i<Params.GetArgumentCount();	i++ )
		{
			auto& SetArgFunc = mSetArguments[i];
			SetArgFunc( Params, i );
		}
		
		mCall( Params );
		
		testCallValue<Value>(pc);
		
	}
};


std::function<void(Bind::TCallback&,size_t)>> GetSetArgumentFunction_uint8_t(TDcFunction* This)
{
	return [This](Bind::TCallback& Params,size_t ParamIndex)
	{
		auto ValueRef = Params.GetArgumentValue( ParamIndex );
		auto Value = JsCore::FromValue<uint8_t>( ValueRef );
		dcbArgUChar( This->mVm, Value );
	};
}


std::function<void(Bind::TCallback&,size_t)>> GetSetArgumentFunction(TDcFunction* This,const std::string& TypeName)
{
	if ( TypeName == "uint8_t" )	return GetSetArgumentFunction_uint8_t(This);
}

TDcFunction::TDcFunction()
{
	mVm = dcNewCallVM(4096);
	
	//	make functions
	mCall = [this](Bind::TCallback& Params)
	{
		dcCallVoid( mVm );
	};
	
	for ( auto i=0;	i<ArgumentTypes.GetSize();	i++ )
	{
		auto Func = GetSetArgumentFunction(ArgumentTypes[i]);
		mSetArguments.PushBack( Func );
	}
}

std::shared_ptr<ApiDll::TFunctionBase> AllocFunction(void* FunctionAddress,const std::string& ReturnType,ArrayBridge<std::string>& TypeStack)
{
/*
	if ( ReturnType == "uint32_t" )
	{
		
		auto Func = AllocFunction_ReturnType<void>( nullptr, TypeStack );
		Func.mFunctor = Function;
	}

	
	
 /*
	if ( ReturnType == "" )			{	return AllocFunction_ReturnType<void>( FunctionAddress, TypeStack );	}
	if ( ReturnType == "void" )		{	return AllocFunction_ReturnType<void>( FunctionAddress, TypeStack );	}
	if ( ReturnType == "uint8_t" )	{	return AllocFunction_ReturnType<uint8_t>( FunctionAddress, TypeStack );	}
	if ( ReturnType == "uint16_t" )	{	return AllocFunction_ReturnType<uint16_t>( FunctionAddress, TypeStack );	}
	if ( ReturnType == "uint32_t" )	{	return AllocFunction_ReturnType<uint32_t>( FunctionAddress, TypeStack );	}
	//if ( ReturnType == "uint8_t*" )	{	return AllocFunction_ReturnType<uint8_t*>( FunctionAddress, TypeStack );	}
	//if ( ReturnType == "uint16_t*" )	{	return AllocFunction_ReturnType<uint16_t*>( FunctionAddress, TypeStack );	}
	//if ( ReturnType == "uint32_t*" )	{	return AllocFunction_ReturnType<uint32_t*>( FunctionAddress, TypeStack );	}
*/
	std::stringstream Error;
	Error << "Unhandled return type " << ReturnType;
	throw Soy::AssertException(Error);
}

#include "Libs/dyncall/include/dyncall.h"



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

	//	this call sets the return value internally
	Function.Call( Params );
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




