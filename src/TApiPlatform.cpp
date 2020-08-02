#include "TApiPlatform.h"
#include <SoyShellExecute.h>
#include <regex>

namespace Platform
{
	//	some platforms need to continously poll, so we have a global for it
	class TStatsManager;
	std::shared_ptr<Platform::TStatsManager>	gStatsManager;
	TStatsManager&								GetStatsManager();
}


namespace ApiPlatform
{
	const char Namespace[] = "Pop.Platform";

	static void		WaitForStats(Bind::TCallback& Params);	//	GetPlatformStats could return latest stats
	
	//	system stuff
	DEFINE_BIND_FUNCTIONNAME(WaitForStats);
}


class Platform::TStatsManager
{
public:
	virtual Bind::TPromise	AddPromise(Bind::TCallback& Params)=0;
};


namespace Nvidia
{
	class TPlatformStat;
	class TPlatformStats;
	TPlatformStat		ParseStat(const std::string& StatString);
}

//	RAM 1901/3956MB (lfb 183x4MB) SWAP 0/1978MB (cached 0MB) CPU [8%@102,6%@102,off,off] EMC_FREQ 0% GR3D_FREQ 0% PLL@38.5C CPU@41.5C PMIC@100C GPU@41.5C AO@48C thermal@41.75C POM_5V_IN 1154/1544 POM_5V_GPU 0/43 POM_5V_CPU 119/205
class Nvidia::TPlatformStat
{
public:
	int				mCpu0Usage = -1;
	int				mCpu1Usage = -1;
	int				mCpu2Usage = -1;
	int				mCpu3Usage = -1;
	int				mCpu0ClockSpeed = -1;
	int				mCpu1ClockSpeed = -1;
	int				mCpu2ClockSpeed = -1;
	int				mCpu3ClockSpeed = -1;
	int				mGpuUsage = -1;
	int				mRamPhysicalUsedMb = -1;
	int				mRamPhysicalTotalMb = -1;
	int				mRamSwapUsedMb = -1;
	int				mRamSwapTotalMb = -1;
};


class Nvidia::TPlatformStats : Platform::TStatsManager
{
public:
	TPlatformStats();
	~TPlatformStats();

	virtual Bind::TPromise	AddPromise(Bind::TCallback& Params) override;
	void 					OnStatOutput(const std::string& Output);
	
public:
	Bind::TPromiseQueueObjects<TPlatformStat>	mStatsQueue;
	std::shared_ptr<Soy::TShellExecute>			mExe;
};





void ApiPlatform::Bind(Bind::TContext& Context)
{
	Context.CreateGlobalObjectInstance("", Namespace);

	Context.BindGlobalFunction<BindFunction::WaitForStats>( WaitForStats, Namespace );
}


Platform::TStatsManager& Platform::GetStatsManager()
{
	if ( gStatsManager )
		return *gStatsManager;
	
#if defined(TARGET_LINUX)
	//	try and create the nvidia one
	try
	{
		gStatsManager.reset( new Nvidia::TPlatformStats() );
		return *gStatsManager;
	}
	catch(std::exception& e)
	{
		std::Debug << "Failed to create Nvidia platform stats; " << e.what() << std::endl;
	}
		
	//	try and create generic linux one
#endif

	throw Soy::AssertException("PlatformStats not availible on this platform");
}


void ApiPlatform::WaitForStats(Bind::TCallback& Params)
{
	auto& StatsManager = Platform::GetStatsManager();
	auto Promise = StatsManager.AddPromise(Params);
	Params.Return(Promise);
}




Nvidia::TPlatformStat Nvidia::ParseStat(const std::string& StatString)
{
	enum StatParts
	{
		RAM_Used = 0,
		RAM_Max,
		lfbused,
		lfbmax,
		swapused,
		swapmax,
		cached,
		cpu0str,
		cpu1str,
		cpu2str,
		cpu3str,
		EMC_FREQ,
		GR3D_FREQ,
		PLL_temp_major,
		PLL_temp_minor,
		cpu_temp_major,
		cpu_temp_minor,
		PMIC,
		GPU_temp_major,
		gpu_temp_minor,
		ao_temp,
		thermal_temp_major,
		thermal_temp_minor,
		POM_5V_IN_used,
		POM_5V_IN_max,
		POM_5V_GPU_used,
		POM_5V_GPU_max,
		POM_5V_CPU_used,
		POM_5V_CPU_max,
	};
	//	RAM 1901/3956MB (lfb 183x4MB) SWAP 0/1978MB (cached 0MB) CPU [8%@102,6%@102,off,off] EMC_FREQ 0% GR3D_FREQ 0% PLL@38.5C CPU@41.5C PMIC@100C GPU@41.5C AO@48C thermal@41.75C POM_5V_IN 1154/1544 POM_5V_GPU 0/43 POM_5V_CPU 119/205
	std::regex ResponsePattern("^RAM ([0-9]+)/([0-9]+)MB (lfb ([0-9]+)x([0-9]+)MB) SWAP ([0-9]+)/([0-9]+)MB (cached ([0-9]+)MB) CPU [(.+),(.+),(.+),(.+)] EMC_FREQ ([0-9]+)% GR3D_FREQ ([0-9]+)% PLL@([0-9]+).([0-9]+)C CPU@([0-9]+).([0-9]+)C PMIC@([0-9]+)C GPU@([0-9]+).([0-9]+)C AO@([0-9]+)C thermal@([0-9]+).([0-9]+)C POM_5V_IN ([0-9]+)/([0-9]+) POM_5V_GPU ([0-9]+)/([0-9]+) POM_5V_CPU ([0-9]+)/([0-9]+)$");
	std::smatch Match;
	if ( !std::regex_match( StatString, Match, ResponsePattern ) )
	{
		std::Debug << "Failed to parse stat; " << StatString << std::endl;
	}
	
	auto GetInt = [&](int& ValueOut,StatParts PartIndex)
	{
		auto& ValueString = Match[1+PartIndex].str();
		Soy::StringToType( ValueOut, ValueString );
	};

	auto GetFloat = [&](float& ValueOut,StatParts PartMajor,StatParts PartMinor)
	{
		auto& MajorString = Match[1+PartMajor].str();
		auto& MinorString = Match[1+PartMinor].str();
		auto FloatString = MajorString + "." + MinorString;
		Soy::StringToType( ValueOut, FloatString );
	};
	
	auto GetCpuInt = [&](int& Usage,int& ClockSpeed,StatParts CpuPart)
	{
		//	get the string
		//	which might be [8%@102]
		//	or [off]
		auto CpuStr = Match[1+CpuPart].str();
		if ( CpuStr == "off" )
		{
			Usage = 0;
			ClockSpeed = 0;
			return;
		}
		auto UsageString = Soy::StringPopUntil(CpuStr,'%',false,false);
		auto ClockString = CpuStr.substr(1);
		Soy::StringToType(Usage,UsageString);
		Soy::StringToType(ClockSpeed,ClockString);
	};

	Nvidia::TPlatformStat Stat;

	GetInt( Stat.mRamPhysicalUsedMb, RAM_Used );
	GetInt( Stat.mRamPhysicalMaxMb, RAM_Max );
	GetInt( Stat.GpuUsage, GR3D_FREQ );
	GetInt( Stat.mRamSwapUsedMb, swapused );
	GetInt( Stat.mRamSwapMaxMb, swapmax );
	GetCpuInt( Stat.mCpu0Usage, Stat.mCpu0ClockSpeed, cpu0str );
	GetCpuInt( Stat.mCpu1Usage, Stat.mCpu1ClockSpeed, cpu1str );
	GetCpuInt( Stat.mCpu2Usage, Stat.mCpu2ClockSpeed, cpu2str );
	GetCpuInt( Stat.mCpu3Usage, Stat.mCpu3ClockSpeed, cpu3str );

	return Stat;
}

Nvidia::TPlatformStats::TPlatformStats()
{
	//	try and open the nvidia stats executable which (blockingly) pumps out stats
	BufferArray<std::string,1> Arguments;
	std::function<void(int)> OnExit;
	std::function<void(const std::string&)> OnStdOut = [this](const std::string& Output)
	{
		this->OnStatOutput(Output);
	};
	std::function<void(const std::string&)> OnStdErr;
	mExe.reset( new Soy::TShellExecute("tegrastats", GetArrayBridge(Arguments), OnExit, OnStdOut, OnStdErr ) );
}

Nvidia::TPlatformStats::~TPlatformStats()
{
	mExe.reset();
}

Bind::TPromise Nvidia::TPlatformStats::AddPromise(Bind::TCallback& Params)
{
	mStatsQueue.AddPromise( Params.mLocalContext );
}

void Nvidia::TPlatformStats::OnStatOutput(const std::string& Output)
{
	auto Stat = ParseStat(OutputCopy);
	mStatsQueue.Push(Stat);
}
