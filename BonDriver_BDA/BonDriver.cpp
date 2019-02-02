//------------------------------------------------------------------------------
// File: BonDriver.cpp
//   Implementation of BonDriver.dll
//
// This code is borrowed from BonDriver_Shiro
//------------------------------------------------------------------------------

#include "BonTuner.h"

// DllMain
/////////////////////////////////////////////
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID /* lpReserved */
					 )
{
    switch(ul_reason_for_call){
		case DLL_PROCESS_ATTACH:
			CBonTuner::Init(hModule);
			break;
	
		case DLL_PROCESS_DETACH:
			CBonTuner::Finalize();
			break;
	}  
    return TRUE;
}
