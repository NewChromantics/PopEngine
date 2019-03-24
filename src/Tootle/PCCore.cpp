/*------------------------------------------------------
	PC Platform core

	contains the program entry

------------------------------------------------------*/
#include "PCCore.h"
#include "PCDebug.h"
#include "../TLCore.h"
#include "../TLTypes.h"
#include "../TString.h"
#include "PCTime.h"
#include "../TCoreManager.h"

#include "cpuid.h"

namespace TLCore
{
	namespace Platform
	{
		// detailed cpu info class
		class CPUINFOEX
		{
		public:
			u16 NUMAnodes;
			u16 numProcessorPackages;
			u16 numProcessorCores;
			u16 numLogicalProcessors;

			u16 numL1Caches;
			u16 numL2Caches;
			u16 numL3Caches;
			u16 pad;
		};

		Bool	GetVerboseProcessorInformation(CPUINFOEX& cpudata);

	}



void expand(int avail, int mask)
{
	TTempString cpustr;

    if (mask & _CPU_FEATURE_MMX) 
	{
		cpustr.Appendf("\t%s\t_CPU_FEATURE_MMX", avail & _CPU_FEATURE_MMX ? "yes" : "no");
	    TLDebug_Print(cpustr);
		cpustr.Empty();

    }
    if (mask & _CPU_FEATURE_SSE) 
	{
		cpustr.Appendf("\t%s\t_CPU_FEATURE_SSE", avail & _CPU_FEATURE_SSE ? "yes" : "no");
	    TLDebug_Print(cpustr);
		cpustr.Empty();
    }
    if (mask & _CPU_FEATURE_SSE2) 
	{
		cpustr.Appendf("\t%s\t_CPU_FEATURE_SSE2", avail & _CPU_FEATURE_SSE2 ? "yes" : "no");
	    TLDebug_Print(cpustr);
		cpustr.Empty();
    }
    if (mask & _CPU_FEATURE_3DNOW) 
	{
		cpustr.Appendf("\t%s\t_CPU_FEATURE_3DNOW", avail & _CPU_FEATURE_3DNOW ? "yes" : "no");
	    TLDebug_Print(cpustr);
		cpustr.Empty();
    }
}


typedef BOOL (WINAPI *LPFN_GLPI)(
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, 
    PDWORD);

// Helper function to count set bits in the processor mask.
WORD CountSetBits(ULONG_PTR bitMask)
{
    DWORD LSHIFT = sizeof(ULONG_PTR)*8 - 1;
    WORD bitSetCount = 0;
    ULONG_PTR bitTest = (ULONG_PTR)1 << LSHIFT;    
    DWORD i;
    
    for (i = 0; i <= LSHIFT; ++i)
    {
        bitSetCount += ((bitMask & bitTest)?1:0);
        bitTest/=2;
    }

    return bitSetCount;
}

Bool TLCore::Platform::GetVerboseProcessorInformation(CPUINFOEX& cpudata)
{
	LPFN_GLPI glpi;
    BOOL done = FALSE;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buffer = NULL;
    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION ptr = NULL;
    DWORD returnLength = 0;
    
	WORD logicalProcessorCount = 0;
    WORD numaNodeCount = 0;
    WORD processorCoreCount = 0;
    WORD processorL1CacheCount = 0;
    WORD processorL2CacheCount = 0;
    WORD processorL3CacheCount = 0;
    WORD processorPackageCount = 0;

    DWORD byteOffset = 0;
    PCACHE_DESCRIPTOR Cache;

    glpi = (LPFN_GLPI) GetProcAddress(
                            GetModuleHandle(TLCharString("kernel32")),
                            "GetLogicalProcessorInformation");
    if (NULL == glpi) 
    {
        TLDebug_Print("GetLogicalProcessorInformation is not supported.");
        return FALSE;
    }

    while (!done)
    {
        DWORD rc = glpi(buffer, &returnLength);

        if (FALSE == rc) 
        {
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) 
            {
                if (buffer) 
                    free(buffer);

                buffer = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(
                        returnLength);

                if (NULL == buffer) 
                {
                    TLDebug_Print("Error: Allocation failure in ");
                    return FALSE;
                }
            } 
            else 
            {
#ifdef _DEBUG
				TTempString errstr;
				errstr.Appendf("Error %d", GetLastError());
                TLDebug_Print(errstr);
#endif
                return FALSE;
            }
        } 
        else
        {
            done = TRUE;
        }
    }

    ptr = buffer;

    while (byteOffset + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= returnLength) 
    {
        switch (ptr->Relationship) 
        {
        case RelationNumaNode:
            // Non-NUMA systems report a single record of this type.
            numaNodeCount++;
            break;

        case RelationProcessorCore:
            processorCoreCount++;

            // A hyperthreaded core supplies more than one logical processor.
            logicalProcessorCount += CountSetBits(ptr->ProcessorMask);
            break;

        case RelationCache:
            // Cache data is in ptr->Cache, one CACHE_DESCRIPTOR structure for each cache. 
            Cache = &ptr->Cache;
            if (Cache->Level == 1)
            {
                processorL1CacheCount++;
            }
            else if (Cache->Level == 2)
            {
                processorL2CacheCount++;
            }
            else if (Cache->Level == 3)
            {
                processorL3CacheCount++;
            }
            break;

        case RelationProcessorPackage:
            // Logical processors share a physical package.
            processorPackageCount++;
            break;

        default:
            TLDebug_Print("Error: Unsupported LOGICAL_PROCESSOR_RELATIONSHIP value");
            break;
        }
        byteOffset += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
        ptr++;
    }

	// Update the structure
	cpudata.NUMAnodes = numaNodeCount;
	cpudata.numProcessorPackages = processorPackageCount;
	cpudata.numProcessorCores = processorCoreCount;
	cpudata.numLogicalProcessors = logicalProcessorCount;

	cpudata.numL1Caches = processorL1CacheCount;
	cpudata.numL2Caches = processorL2CacheCount;
	cpudata.numL3Caches = processorL3CacheCount;
    
    free(buffer);

	return TRUE;
}


void TLCore::Platform::QueryHardwareInformation(TBinaryTree& Data)	
{
	TLDebug_Print("Device Information:");

	/////////////////////////////////////////////////////////////
	// Device ID, OS and type
	/////////////////////////////////////////////////////////////	

	// General system info
	// NOTE: Should use GetNativeSystemInfo if possible and will also return the OS info
	// without having to do it separately.  Windows is a mess in how this info is retrieved! :(

	SYSTEM_INFO SysInfo;
	GetSystemInfo(&SysInfo);
    TLDebug_Print( TString( "Physical CPU Count:\t\t%d", SysInfo.dwNumberOfProcessors ) );

	CPUINFOEX morecpudata;

	if(GetVerboseProcessorInformation(morecpudata))
	{
		TLDebug_Print( TString("Core count:\t\t%d", morecpudata.numProcessorCores) );
		TLDebug_Print( TString("Logical Processor count:\t\t%d", morecpudata.numLogicalProcessors) );
		
		// Number of processors
		TLDebug_Print( TString("CPU_", morecpudata.numLogicalProcessors) );
	}
	else
	{
		// Number of processors
		Data.ExportData("CPU_", SysInfo.dwNumberOfProcessors);
	}

	//More detailed CPU info
	_p_info info;

	_cpuid(&info);
    TLDebug_Print( TString("Name:\t\t%s", info.v_name) );
	TLDebug_Print( TString("Model:\t\t%s", info.model_name) );
	TLDebug_Print( TString("Family:\t\t%d", info.family) );
	TLDebug_Print( TString("Model:\t\t%d", info.model) );
	TLDebug_Print( TString("Stepping:\t%d", info.stepping) );
	TLDebug_Print( TString("Feature:\t%08x", info.feature) );

    expand(info.feature, info.checks);
	TLDebug_Print( TString("OS Support:\t%08x", info.os_support ) );

    expand(info.os_support, info.checks);
	TLDebug_Print( TString("Checks:\t\t%08x", info.checks ) );

	// Can also use IsProcessorFeaturePresent() to test for specific features of the processor if required.

	// OS Version info

	OSVERSIONINFO OSInfo;
	OSInfo.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
	GetVersionEx(&OSInfo);

	TTempString OS("Unknown");
	TTempString OSVer("Unknown");

	switch(OSInfo.dwPlatformId)
	{
		case VER_PLATFORM_WIN32s:
			{
				// A version of windows
				OS = "Windows";
				OSVer.Appendf("%d.%d", OSInfo.dwMajorVersion, OSInfo.dwMinorVersion);
			}
			break;
		case VER_PLATFORM_WIN32_WINDOWS:
			{
				OS = "Windows";

				switch(OSInfo.dwMinorVersion)
				{
					case 0:	
						// Windows 95
						OSVer = "95";
						break;
					case 10:
						// Windows 98
						OSVer = "98";
						break;
					case 90:
						// Windows Me
						OSVer = "Me";
						break;
				}
			}
			break;
		case VER_PLATFORM_WIN32_NT:
			{
				OS = "Windows";

				if(OSInfo.dwMajorVersion == 5)
				{
					if(OSInfo.dwMinorVersion == 0)
					{
						// Windows 2000
						OSVer = "2000";
					}
					else if(OSInfo.dwMinorVersion == 1)
					{
						// Windows XP
						OSVer = "XP";
					}
				}
				else if(OSInfo.dwMajorVersion <= 4)
				{
					OSVer = "NT";
				}
				else
				{
					// Unknown version so append the version info
					OSVer.Appendf("%d.%d", OSInfo.dwMajorVersion, OSInfo.dwMinorVersion);
				}
			}
			break;
			/*
		case VER_PLATFORM_WIN32_CE:
			{
				OS = "Windows";
				OSVer = "CE";
			}
			break;
			*/
	}

	// Print the OS details
	TLDebug_Print(OS);
	TLDebug_Print(OSVer);

	// Print out the major and minor version anyway
	TLDebug_Print( TString("Major: %d \nMinor:%d", OSInfo.dwMajorVersion, OSInfo.dwMinorVersion) );

	// Print out any extra data such as service pack info
	TLDebug_Print(OSInfo.szCSDVersion);

	// Add the OS to the data
	Data.ExportData("OS", OS);
	Data.ExportData("OSVer", OSVer);

	/////////////////////////////////////////////////////////////
	TLDebug_Print("End Device Information");
}

void TLCore::Platform::QueryLanguageInformation(TBinaryTree& Data)	
{
	TLDebug_Print("Language Information:");

	/////////////////////////////////////////////////////////////
	// Langauge
	/////////////////////////////////////////////////////////////

	LANGID UserLanguage = GetUserDefaultUILanguage();

	WORD PrimaryLanguage = PRIMARYLANGID(UserLanguage);
	WORD SubLanguage = SUBLANGID(UserLanguage);

	// Print the primary and sub anguage ID's
	TLDebug_Print( TString("Primary: %d", PrimaryLanguage ) );
	TLDebug_Print( TString("Secondary: %d", SubLanguage ) );

	// Convert the language ID into a TRef we can use
	// Default to english
	TRef LanguageRef = "eng";

	switch(PrimaryLanguage)
	{
		case LANG_ENGLISH:
			{
				switch(SubLanguage)
				{
					default:
						TLDebug_Print("Hardware langauge not supported - defaulting to english");
					case SUBLANG_ENGLISH_UK:	
						LanguageRef = "eng";	
						break;
					
					case SUBLANG_ENGLISH_US: 	
						LanguageRef = "usa";	
						break;
				}
			}
			break;
		case LANG_FRENCH:
			LanguageRef = "fre";
			break;
		case LANG_GERMAN:
			LanguageRef = "ger";
			break;
		case LANG_ITALIAN:
			LanguageRef = "ita";
			break;
		case LANG_SPANISH:
			LanguageRef = "spa";
			break;
		case SUBLANG_DUTCH:
			LanguageRef = "ned";
			break;
		case LANG_JAPANESE:
			LanguageRef = "jap";
			break;
		default:
			TLDebug_Print("Hardware langauge not supported - defaulting to english");
			TRef LanguageRef = "eng";
			break;
	}
	
	// Export the users language to the data - actual language selection will be done 
	// via the core manager
	Data.ExportData("Language", LanguageRef);
}





void TLCore::Platform::DoQuit()
{
	// Send a message to the core manager telling it to quit
	TLMessaging::TMessage Message("Quit");

	TLCore::g_pCoreManager->QueueMessage(Message);
}

