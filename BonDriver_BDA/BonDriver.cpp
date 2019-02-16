//------------------------------------------------------------------------------
// File: BonDriver.cpp
//   Implementation of BonDriver.dll
//
// This code is borrowed from BonDriver_Shiro
//------------------------------------------------------------------------------

#ifdef _DEBUG
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>
#endif

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
#ifdef _DEBUG
			::_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
			CBonTuner::Init(hModule);
			break;
	
		case DLL_PROCESS_DETACH:
			CBonTuner::Finalize();
			break;
	}  
    return TRUE;
}
