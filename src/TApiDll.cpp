#include "TApiDll.h"
#include "SoyRuntimeLibrary.h"

namespace ApiDll
{
	const char Namespace[] = "Pop.Dll";

	DEFINE_BIND_TYPENAME(Library);
	DEFINE_BIND_FUNCTIONNAME(BindFunction);

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
}



template<typename RETURNTYPE>
class TFunction_0 : public ApiDll::TFunctionBase
{
public:
	virtual void 	Call(Bind::TCallback& Params)
	{
		auto Result = mFunctor();
		Params.Return( Result );
	}
	
public:
	std::function<RETURNTYPE()>	mFunctor;
};


template<typename RETURNTYPE,typename A>
class TFunction_A : public ApiDll::TFunctionBase
{
public:
	virtual void 	Call(Bind::TCallback& Params)
	{
		auto a = Bind::FromValue<A>( Params.GetContextRef(), Params.GetArgumentValue(0) );
		auto Result = mFunctor(a);
		Params.Return( Result );
	}
	
public:
	std::function<RETURNTYPE(A)>	mFunctor;
};

template<typename RETURNTYPE,typename A,typename B>
class TFunction_AB : public ApiDll::TFunctionBase
{
public:
	virtual void 	Call(Bind::TCallback& Params)
	{
		auto a = Bind::FromValue<A>( Params.GetContextRef(), Params.GetArgumentValue(0) );
		auto b = Bind::FromValue<B>( Params.GetContextRef(), Params.GetArgumentValue(1) );
		auto Result = mFunctor(a,b);
		Params.Return( Result );
	}
	
public:
	std::function<RETURNTYPE(A,B)>	mFunctor;
};


template<typename RETURNTYPE,typename A,typename B,typename C>
class TFunction_ABC : public ApiDll::TFunctionBase
{
public:
	virtual void 	Call(Bind::TCallback& Params)
	{
		auto a = Bind::FromValue<A>( Params.GetContextRef(), Params.GetArgumentValue(0) );
		auto b = Bind::FromValue<B>( Params.GetContextRef(), Params.GetArgumentValue(1) );
		auto c = Bind::FromValue<C>( Params.GetContextRef(), Params.GetArgumentValue(2) );
		auto Result = mFunctor(a,b,c);
		Params.Return( Result );
	}
	
public:
	std::function<RETURNTYPE(A,B,C)>	mFunctor;
};


/*
template<typename RETURNTYPE,typename ... TYPES>
class TFunction : public TFunctionBase
{
	template<typename T>
	T Convert()//JSValueRef Value,Bind::TCallback& Params)
	{
		//auto CValue = Bind::FromValue<T>( Params.GetContextRef(), Value );
		//return CValue;
		return T();
	}

	void DoCall(TYPES... Types)
	{
		mFunctor( Types... );
	}
	virtual void 	Call(Bind::TCallback& Params) override
	{
		TYPES... x;
		
		auto* ArgumentValues = Params.mArguments.GetArray();
		//auto Result = mFunctor( Convert<TYPES...>( ArgumentValues, Params ) );
		DoCall( Convert<TYPES...>() );
		//	convert result back to Params.Return()
	}
	
	std::function<RETURNTYPE(TYPES...)>	mFunctor;
};

class TFunction_Void : public TFunctionBase
{
	std::function<void(void)>	mFunctor;
};
*/

/*
template<typename NEWTYPE,typename ...TYPES>
std::shared_ptr<TFunctionBase> GetNextTypeFunc(ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<TFunctionBase>( new TFunction<TYPES...,NEWTYPE>() );

	auto NextType = TypeStack.PopAt(0);
	
	if ( NextType == "uint8_t" )	{	return GetNextTypeFunc<uint8_t,TYPES... NEWTYPE>(NextType);	}
	if ( NextType == "uint16_t" )	{	return GetNextTypeFunc<uint16_t,TYPES... NEWTYPE>(NextType);	}
	if ( NextType == "uint32_t" )	{	return GetNextTypeFunc<uint32_t,TYPES...,NEWTYPE>(NextType);	}
	
	std::stringstream Error;
	Error << "Unhandled type " << NextType;
	throw Soy::AssertException(Error);
}

std::shared_ptr<TFunctionBase> GetFirstTypeFunc(ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<TFunctionBase>( new TFunction<int>() );
	
	auto NextType = TypeStack.PopAt(0);
	
	if ( NextType == "uint8_t" )	{	return GetNextTypeFunc<uint8_t>(TypeStack);	}
	if ( NextType == "uint16_t" )	{	return GetNextTypeFunc<uint16_t>(TypeStack);	}
	if ( NextType == "uint32_t" )	{	return GetNextTypeFunc<uint32_t>(TypeStack);	}

	std::stringstream Error;
	Error << "Unhandled type " << NextType;
	throw Soy::AssertException(Error);
}
*/



template<typename RETURNTYPE,typename A,typename B,typename C>
std::shared_ptr<ApiDll::TFunctionBase> GetFunction_ABC(ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_ABC<RETURNTYPE,A,B,C>() );
	
	throw Soy::AssertException("Need to handle more args");
}


template<typename RETURNTYPE,typename A,typename B>
std::shared_ptr<ApiDll::TFunctionBase> GetFunction_AB(ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_AB<RETURNTYPE,A,B>() );
	
	auto NextType = TypeStack.PopAt(0);
	
	if ( NextType == "uint8_t" )	{	return GetFunction_ABC<RETURNTYPE,A,B,uint8_t>(TypeStack);	}
	if ( NextType == "uint16_t" )	{	return GetFunction_ABC<RETURNTYPE,A,B,uint16_t>(TypeStack);	}
	if ( NextType == "uint32_t" )	{	return GetFunction_ABC<RETURNTYPE,A,B,uint32_t>(TypeStack);	}
	
	std::stringstream Error;
	Error << "Unhandled type " << NextType;
	throw Soy::AssertException(Error);
}


template<typename RETURNTYPE,typename A>
std::shared_ptr<ApiDll::TFunctionBase> GetFunction_A(ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_A<RETURNTYPE,A>() );
	
	auto NextType = TypeStack.PopAt(0);
	
	if ( NextType == "uint8_t" )	{	return GetFunction_AB<RETURNTYPE,A,uint8_t>(TypeStack);	}
	if ( NextType == "uint16_t" )	{	return GetFunction_AB<RETURNTYPE,A,uint16_t>(TypeStack);	}
	if ( NextType == "uint32_t" )	{	return GetFunction_AB<RETURNTYPE,A,uint32_t>(TypeStack);	}
	
	std::stringstream Error;
	Error << "Unhandled type " << NextType;
	throw Soy::AssertException(Error);
}
	
template<typename RETURNTYPE>
std::shared_ptr<ApiDll::TFunctionBase> GetFunction(ArrayBridge<std::string>& TypeStack)
{
	if ( TypeStack.GetSize() == 0 )
		return std::shared_ptr<ApiDll::TFunctionBase>( new TFunction_0<RETURNTYPE>() );
	
	auto NextType = TypeStack.PopAt(0);
	
	if ( NextType == "uint8_t" )	{	return GetFunction_A<RETURNTYPE,uint8_t>(TypeStack);	}
	if ( NextType == "uint16_t" )	{	return GetFunction_A<RETURNTYPE,uint16_t>(TypeStack);	}
	if ( NextType == "uint32_t" )	{	return GetFunction_A<RETURNTYPE,uint32_t>(TypeStack);	}
	
	std::stringstream Error;
	Error << "Unhandled type " << NextType;
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
		Params.GetArgumentString( 2 );
	
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
	auto Bridge = GetArrayBridge(ArgTypes);
	auto Func = GetFunction<int>(Bridge);

	//	todo: check function doesn't already exist in map
	This.mFunctions[SymbolName] = Func;

	//	now generate a function in javascript, which just calls our wrapper (CallFunction)
	//	then return that as the function that the user can call
	
	//	test call!
	Func->Call( Params );
	
	throw Soy::AssertException("found symbol. todo: make func");
	
	//	make a new function to return back to javascript
	//	something like a bridge that knows how to reinterpret each type, but
	//	we want to be efficient when it comes to things like uint8 arrays
	//	
}

