#include "TApiSokol.h"
#include "PopMain.h"
#include "sokol/sokol_gfx.h"

class SokolLinuxContext : public ApiSokol::TSokolContext
{
public:
	SokolLinuxContext(std::shared_ptr<SoyWindow> 	mSoyWindow );

	// sg_context_desc								mContextDesc;

	sg_context_desc								GetSokolContext() override;
};

SokolLinuxContext::SokolLinuxContext(std::shared_ptr<SoyWindow> 	mSoyWindow ) : ApiSokol::TSokolContext(mSoyWindow)
{
	mContextDesc = (sg_context_desc){};
}

sg_context_desc SokolLinuxContext::GetSokolContext()
{
	return mContextDesc;
}

// ApiSokol::TSokolContext::TSokolContext(	std::shared_ptr<SoyWindow> 	mSoyWindow )
// {
// 	new SokolLinuxContext( mSoyWindow );
// }