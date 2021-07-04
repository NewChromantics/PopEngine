#include "JavascriptConvertImports.h"

#include <SoyDebug.h>
#include <regex>
#include "HeapArray.hpp"

//	Popengine implements require() which makes an exports symbol for the module
//	and returns it (a module)
//	so imports need changing, and exports inside a file need converting

//	convert imports;
//		import * as Module from 'filename1'
//		import symbol from 'filename2'
//		import { symbol } from 'filename3'
//		import { symbol as NewSymbol } from 'filename4'
//	into 
//		const Module = require('filename1')
//
//		const ___PrivateModule = require('filename2')
//		const symbol = ___PrivateModule.symbol;
//
//		const ___PrivateModule = require('filename3')
//		const symbol = ___PrivateModule.symbol;
//
//		const ___PrivateModule = require('filename4')
//		const NewSymbol = ___PrivateModule.symbol;

//	make a pattern for valid js symbols
auto Symbol = "([a-zA-Z0-9]+)";
auto QuotedFilename = "(\"|')(.+).js('|\")";
auto Whitespace = "\\s+";
auto OptionalWhitespace = "\\s*";

void ConvertImports(std::string& Source)
{
	//	import * as X from QUOTEFILENAMEQUOTE
	std::stringstream ImportPattern0;	ImportPattern0 << "import" << Whitespace << "\\*" << Whitespace << "as" << Whitespace << Symbol << Whitespace << "from" << Whitespace << QuotedFilename;
	std::string ReplacementPattern0("const $1 = require($2$3.js$4);");

	//	import X from QUOTEFILENAMEQUOTE
	std::stringstream ImportPattern1;	ImportPattern1 << "import" << Whitespace << Symbol << Whitespace << "from" << Whitespace << QuotedFilename;
	std::string ReplacementPattern1("const $1_Module = require($2$3.js$4); const $1 = $1_Module.default;");
	
	//	$0 whole string match
	//	$1 capture group 0 etc
	Source = std::regex_replace(Source, std::regex(ImportPattern0.str()), ReplacementPattern0 );
	Source = std::regex_replace(Source, std::regex(ImportPattern1.str()), ReplacementPattern1 );
}


//	export let A = B;		let A = ... exports.A = A;
//	export function C(...
//	export const D;
//	export 
void ConvertExports(std::string& Source)
{
	//	moving export to AFTER the declaration is hard.
	//	so instead, find all the exports, declare them all at the end 
	//	of the file, and then just clean the declarations
	auto Keyword = "(const|var|let|function)";
	//	must be other cases... like new line and symbol?
	//	symbol( <-- function
	//	symbol=
	//	symbol;
	//	maybe ^symbol? 
	auto VariableNameEnd = "(\\(|=|;)";
	auto DefaultMaybe = "\\s*(default)?";

	//	export DECL VAR=
	std::stringstream ExportPattern0;	ExportPattern0 << "export" << DefaultMaybe << Whitespace << Keyword << Whitespace << Symbol << OptionalWhitespace << VariableNameEnd;
	std::string ReplacementPattern0("$2 $3 $4");

	//	export Symbol;
	std::stringstream ExportPattern1;	ExportPattern1 << "export" << DefaultMaybe << Whitespace << Symbol << OptionalWhitespace << ";";
	std::string ReplacementPattern1("/*export$1 $2;*/");

	//	get all the export symbols
	Array<std::string> ExportSymbols;
	std::string DefaultExportSymbol;
	
	auto ExtractSymbolsFromRegex = [&](std::stringstream& RegexPattern,int SymbolMatchIndex)
	{
		int DefaultMatchIndex = 1;
		std::smatch SearchMatch;
		std::string SearchingSource = Source;
		auto PatternString = RegexPattern.str();
		while ( std::regex_search( SearchingSource, SearchMatch, std::regex(PatternString) ) )
		{
			//auto IsDefault = SearchMatch[DefaultMatchIndex].str().length() > 0;
			auto IsDefault = SearchMatch[DefaultMatchIndex].matched;
			auto Symbol = SearchMatch[SymbolMatchIndex].str();
			auto Matched = SearchMatch[0].matched;
			//	gr: this is sometimes matching empty groups (per line?)
			if ( Symbol.length() )
				ExportSymbols.PushBack( Symbol );
			if ( IsDefault )
				DefaultExportSymbol = Symbol;
				
			SearchingSource = SearchMatch.suffix();
		}
	};
	ExtractSymbolsFromRegex(ExportPattern0,3);
	ExtractSymbolsFromRegex(ExportPattern1,2);
	

	std::stringstream NewExports;
	if ( !ExportSymbols.IsEmpty() )
	{
		NewExports << "\n\n//	Generated exports\n";
		NewExports << "exports.default = " << DefaultExportSymbol << ";\n";
		for ( auto e=0;	e<ExportSymbols.GetSize();	e++ )
		{
			NewExports << "exports." << ExportSymbols[e] << " = " << ExportSymbols[e] << ";\n";
		}
	}

	Source = std::regex_replace(Source, std::regex(ExportPattern0.str()), ReplacementPattern0 );
	Source = std::regex_replace(Source, std::regex(ExportPattern1.str()), ReplacementPattern1 );
	
	Source += NewExports.str();
	
	std::Debug << "Replaced exports...\n\n" << Source << std::endl; 
}


void Javascript::ConvertImportsToRequires(std::string& Source)
{
	ConvertImports(Source);
	ConvertExports(Source);
		
	//std::Debug << "Replaced imports...\n\n" << Source << std::endl; 
}
