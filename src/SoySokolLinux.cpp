#include <stdlib.h>
#include "LinuxDRM/esUtil.h"
#include "TApiSokol.h"
#include "PopMain.h"

#define SOKOL_IMPL
#define SOKOL_GLES2
#include "sokol/sokol_gfx.h"

class SokolLinuxContext : public ApiSokol::TSokolContext
{
public:
	SokolLinuxContext(std::shared_ptr<SoyWindow> 	mSoyWindow, int SampleCount );

	sg_context_desc								mContextDesc;

	sg_context_desc								GetSokolContext() override;
};

SokolLinuxContext::SokolLinuxContext(std::shared_ptr<SoyWindow> mSoyWindow, int SampleCount ) : ApiSokol::TSokolContext(mSoyWindow, SampleCount)
{
	mContextDesc = (sg_context_desc){ .sample_count = SampleCount };
}

sg_context_desc SokolLinuxContext::GetSokolContext()
{
	return mContextDesc;
}