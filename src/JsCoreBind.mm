#import <JavascriptCore/JSContext.h>
#include "SoyDebug.h"
/*
@interface ModLod : JSModuleLoaderDelegate
{
};
*/
void JSInitModuleLoader(JSGlobalContextRef GlobalContext)
{
	//	gr: we can get the objective-c context here
	//	the moduleLoaderDelegate exists, but not set (as expected)
	//	but, don't seem to have a class name for the delegate
	//	(lldb) print ObjcContext.moduleLoaderDelegate
	//	(id) $3 = nil
	https://github.com/WebKit/WebKit/blob/96d76e5e19c03039994ff2cc9c040daa673c39ea/Source/JavaScriptCore/API/JSContext.mm#L304
	JSContext* ObjcContext =  [JSContext contextWithJSGlobalContextRef:GlobalContext];
	//JSContext* ObjcContext = JSContext.currentContext;
	std::Debug << "objc context" << std::endl;
}

