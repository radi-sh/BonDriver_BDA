//------------------------------------------------------------------------------
// File: BonDriver.cpp
//   Implementation of BonDriver.dll
//
// This code is borrowed from BonDriver_Shiro
//------------------------------------------------------------------------------

#include "BonTuner.h"

using namespace std;

// DllMain
/////////////////////////////////////////////
BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
					 )
{
    switch(ul_reason_for_call){
		case DLL_PROCESS_ATTACH:
			// モジュールハンドル保存
			CBonTuner::st_hModule = hModule;

			::InitializeCriticalSection(&CBonTuner::st_LockInstanceList);

			break;
	
		case DLL_PROCESS_DETACH:
			// 未解放のインスタンスが残っていれば解放
			list<CBonTuner*>::iterator it;
			while ((it = CBonTuner::st_InstanceList.begin()) != CBonTuner::st_InstanceList.end()) {
				SAFE_RELEASE(*it);
			}

			::DeleteCriticalSection(&CBonTuner::st_LockInstanceList);

			// デバッグログファイルのクローズ
			CloseDebugLog();

			break;
	}  
    return TRUE;
}
