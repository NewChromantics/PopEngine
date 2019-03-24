/*------------------------------------------------

	Base class for creating/controlling win32 windows

-------------------------------------------------*/
#pragma once

#include "PCWinControl.h"



namespace Win32
{
	class GWindow;
};


class Win32::GWindow : public Win32::GWinControl
{
public:
	GWindow() : GWinControl	(InstanceRef)			{}
	~GWindow()					{}

	virtual u32					DefaultFlags()						{	return GWinControlFlags::ClientEdge|GWinControlFlags::OverlappedWindow;	};

	virtual const TChar*		ClassName()							{	return TLCharString("Window");	};
	virtual Bool				Init(TPtr<GWinControl>& pOwner, u32 Flags);	//	window is overloaded to create class
};

