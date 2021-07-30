#include "JavascriptConvertImports.h"

#include <SoyDebug.h>
#include <regex>
#include "HeapArray.hpp"
#include <SoyString.h>

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
auto Symbol = "([a-zA-Z0-9_]+)";
auto QuotedFilename = "(\"|')(.+\\.js)('|\")";
auto QuotedFilenamePopEngineJs = "(\"|')(.+\\/PopEngine\\.js)('|\")";
auto Whitespace = "\\s+";
auto OptionalWhitespace = "\\s*";
auto Keyword = "(const|var|let|class|function|extends|async\\sfunction)";	//	prefixes which break up export, variable name etc

//	must be other cases... like new line and symbol? maybe we can use ^symbol ?
//	symbol( <-- function
//	symbol= <-- var definition
//	symbol; <-- var declaration
//	symbol{ <-- class
auto VariableNameEnd = "(\\(|=|;|extends|\\{)";

void ReplacementPattern2(std::stringstream& Output,std::smatch& Match)
{
	//	import { $1 } from $2
	//	split symbols
	auto RawSymbolsString = Match[1].str();
	auto Filename = Match[2].str() + Match[3].str() + Match[4].str();
	
	Array<std::string> InputSymbols;
	Array<std::string> OutputSymbols;

	const std::string WhitespaceChars = " \t\n";
	

	auto AppendSymbol = [&](const std::string& Match,const char& Delin)
	{
		//	split `X as Y`
		//	to avoid matching as in class, split by whitespace, so we should have either 1 or 3 matches
		BufferArray<std::string,3> Input_As_Output;
		Soy::StringSplitByMatches( GetArrayBridge(Input_As_Output), Match, WhitespaceChars, false );

		//	no "as" in the middle
		if ( Input_As_Output.GetSize() == 1 )
		{
			Input_As_Output.PushBack( Input_As_Output[0] );
			Input_As_Output.PushBack( Input_As_Output[0] );
		}
		
		InputSymbols.PushBack( Input_As_Output[0] );
		//	as
		OutputSymbols.PushBack( Input_As_Output[2] );
		return true;
	};	
	
	Soy::StringSplitByMatches( AppendSymbol, RawSymbolsString, ",", false );

	//	generate module name
	std::stringstream ModuleName;
	ModuleName << "_Module_";
	for ( auto s=0;	s<OutputSymbols.GetSize();	s++ )
		ModuleName << "_" << OutputSymbols[s];
	
	//	add module
	Output << "const " << ModuleName.str() << " = require(" << Filename << ");\n";
	
	//	add symbols
	for ( auto s=0;	s<OutputSymbols.GetSize();	s++ )
	{
		auto& InputSymbol = InputSymbols[s];
		auto& OutputSymbol = OutputSymbols[s];
		Output << "const " << OutputSymbol << " = " << ModuleName.str() << "." << InputSymbol << ";\n";
	}
}

std::string regex_replace_callback(const std::string& Input,std::regex Regex,std::function<void(std::stringstream&,std::smatch&)> Replacement)
{
	// Make a local copy
	std::string PendingInput = Input;

	// Reset resulting value
	std::stringstream Output;

	std::smatch Matches;
	while (std::regex_search(PendingInput, Matches, Regex)) 
	{
		// Build resulting string
		Output << Matches.prefix();
		Replacement( Output, Matches );
		
		//	next search the rest
		PendingInput = Matches.suffix();
	}
	
	//	If there is still a suffix, add it
	//Output << Matches.suffix();	//	gr: seems to be empty?
	//	add the remaining string that didn't match
	Output << PendingInput;
	
	return Output.str();
}


void ConvertImports(std::string& Source)
{
	//	special case to catch PopEngine.js, which is how we import in web
	std::stringstream ImportPatternPop;	ImportPatternPop << "import" << Whitespace << "Pop" << Whitespace << "from" << Whitespace << QuotedFilenamePopEngineJs;
	std::string ReplacementPatternPop("/* import Pop from $1$2$3 */");
	
	//	import * as X from QUOTEFILENAMEQUOTE
	std::stringstream ImportPattern0;	ImportPattern0 << "import" << Whitespace << "\\*" << Whitespace << "as" << Whitespace << Symbol << Whitespace << "from" << Whitespace << QuotedFilename;
	std::string ReplacementPattern0("const $1 = require($2$3$4);");

	//	import X from QUOTEFILENAMEQUOTE
	std::stringstream ImportPattern1;	ImportPattern1 << "import" << Whitespace << Symbol << Whitespace << "from" << Whitespace << QuotedFilename;
	std::string ReplacementPattern1("const $1_Module = require($2$3$4); const $1 = $1_Module.default;");
	
	//	import {X} from QUOTEFILENAMEQUOTE
	std::stringstream ImportPattern2;	ImportPattern2 << "import" << OptionalWhitespace << "\\{([^}]*)\\}" << OptionalWhitespace << "from" << Whitespace << QuotedFilename;
	//	gr: needs special case to replaceop
	//std::string ReplacementPattern2("/* symbols: $1 */");
	
	//	$0 whole string match
	//	$1 capture group 0 etc
	Source = std::regex_replace(Source, std::regex(ImportPatternPop.str()), ReplacementPatternPop );
	Source = std::regex_replace(Source, std::regex(ImportPattern0.str()), ReplacementPattern0 );
	Source = std::regex_replace(Source, std::regex(ImportPattern1.str()), ReplacementPattern1 );
	Source = regex_replace_callback(Source, std::regex(ImportPattern2.str()), ReplacementPattern2 );
	
	//std::Debug << std::endl << std::endl << "new source; "  << std::endl << Source << std::endl<< std::endl;
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
		if ( DefaultExportSymbol.empty() )
			throw Soy::AssertException("Missing default export");
			
		NewExports << "\n\n//	Generated exports\n";
		
		//	will generate bad syntax if no default symbol
		if ( !DefaultExportSymbol.empty() )
			NewExports << "exports.default = " << DefaultExportSymbol << ";\n";
			
		for ( auto e=0;	e<ExportSymbols.GetSize();	e++ )
		{
			NewExports << "exports." << ExportSymbols[e] << " = " << ExportSymbols[e] << ";\n";
		}
	}

	//	now replace the matches (ie, strip out export & default)
	Source = std::regex_replace(Source, std::regex(ExportPattern0.str()), ReplacementPattern0 );
	Source = std::regex_replace(Source, std::regex(ExportPattern1.str()), ReplacementPattern1 );
	
	Source += NewExports.str();
	
	//std::Debug << "Replaced exports...\n\n" << Source << std::endl; 
}


void Javascript::ConvertImportsToRequires(std::string& Source)
{
	ConvertImports(Source);
	ConvertExports(Source);
		
	//std::Debug << "Replaced imports...\n\n" << Source << std::endl; 
}
