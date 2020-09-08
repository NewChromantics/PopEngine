#include "TApiSokol.h"
#include "PopMain.h"
#include "sokol/sokol_gfx.h"

ApiSokol::TSokolContext::TSokolContext(	std::shared_ptr<SoyWindow> 	mSoyWindow )
{
	mContextDesc = (sg_context_desc){};
}