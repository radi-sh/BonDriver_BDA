// BonTuner.cpp: CBonTuner �N���X�̃C���v�������e�[�V����
//
//////////////////////////////////////////////////////////////////////

#include "common.h"

#include "BonTuner.h"

#include <Windows.h>
#include <string>
#include <regex>

#include <DShow.h>

#include "tswriter.h"

// KSCATEGORY_...
#include <ks.h>
#pragma warning (push)
#pragma warning (disable: 4091)
#include <ksmedia.h>
#pragma warning (pop)
#include <bdatypes.h>
#include <bdamedia.h>

// bstr_t
#include <comdef.h>

#include "CIniFileAccess.h"
#include "WaitWithMsg.h"

#ifdef _DEBUG
#pragma comment(lib, "strmbasd.lib")
#else
#pragma comment(lib, "strmbase.lib")
#endif

#pragma comment(lib, "winmm.lib")

FILE *g_fpLog = NULL;

//////////////////////////////////////////////////////////////////////
// �ÓI�����o�ϐ�
//////////////////////////////////////////////////////////////////////

// Dll�̃��W���[���n���h��
HMODULE CBonTuner::st_hModule = NULL;

// �쐬���ꂽCBontuner�C���X�^���X�̈ꗗ
std::list<CBonTuner*> CBonTuner::st_InstanceList;

// st_InstanceList����p
CRITICAL_SECTION CBonTuner::st_LockInstanceList;

// �K�v�ȐÓI�ϐ�������
void CBonTuner::Init(HMODULE hModule)
{
	st_hModule = hModule;
	::InitializeCriticalSection(&st_LockInstanceList);
	return;
}

// �ÓI�ϐ��̉��
void CBonTuner::Finalize(void)
{
	// ������̃C���X�^���X���c���Ă���Ή��
	for (auto it = st_InstanceList.begin(); it != st_InstanceList.end();) {
		SAFE_RELEASE(*it);
		it = st_InstanceList.erase(it);
	}

	::DeleteCriticalSection(&CBonTuner::st_LockInstanceList);

	// �f�o�b�O���O�t�@�C���̃N���[�Y
	CloseDebugLog();
	return;
}

//////////////////////////////////////////////////////////////////////
// �C���X�^���X�������\�b�h
//////////////////////////////////////////////////////////////////////
#pragma warning(disable : 4273)
extern "C" __declspec(dllexport) IBonDriver * CreateBonDriver()
{
	return (IBonDriver *) new CBonTuner;
}
#pragma warning(default : 4273)

//////////////////////////////////////////////////////////////////////
// �\�z/����
//////////////////////////////////////////////////////////////////////
CBonTuner::CBonTuner()
	: m_nToneWait(100),
	m_nLockWait(2000),
	m_nLockWaitDelay(0),
	m_nLockWaitRetry(0),
	m_bLockTwice(FALSE),
	m_nLockTwiceDelay(100),
	m_nWatchDogSignalLocked(0),
	m_nWatchDogBitRate(0),
	m_nReOpenWhenGiveUpReLock(0),
	m_bTryAnotherTuner(FALSE),
	m_bBackgroundChannelLock(FALSE),
	m_nSignalLevelCalcType(eSignalLevelCalcTypeSSStrength),
	m_bSignalLevelGetTypeSS(FALSE),
	m_bSignalLevelGetTypeTuner(FALSE),
	m_bSignalLevelGetTypeDemodSS(FALSE),
	m_bSignalLevelGetTypeBR(FALSE),
	m_bSignalLevelNeedStrength(FALSE),
	m_bSignalLevelNeedQuality(FALSE),
	m_fStrengthCoefficient(1),
	m_fQualityCoefficient(1),
	m_fStrengthBias(0),
	m_fQualityBias(0),
	m_fStrength(0),
	m_fQuality(0),
	m_nSignalLockedJudgeType(eSignalLockedJudgeTypeSS),
	m_bSignalLockedJudgeTypeSS(FALSE),
	m_bSignalLockedJudgeTypeTuner(FALSE),
	m_bSignalLockedJudgeTypeDemodSS(FALSE),
	m_nBuffSize(188 * 1024),
	m_nMaxBuffCount(512),
	m_nWaitTsCount(1),
	m_nWaitTsSleep(100),
	m_bDeleteNullPackets(FALSE),
	m_bAlwaysAnswerLocked(FALSE),
	m_nThreadPriorityCOM(THREAD_PRIORITY_ERROR_RETURN),
	m_nThreadPriorityDecode(THREAD_PRIORITY_ERROR_RETURN),
	m_nThreadPriorityStream(THREAD_PRIORITY_ERROR_RETURN),
	m_nPeriodicTimer(0),
	m_sIniFilePath(L""),
	m_hOnStreamEvent(NULL),
	m_hOnDecodeEvent(NULL),
	m_LastBuff(NULL),
	m_bRecvStarted(FALSE),
	m_hStreamThread(NULL),
	m_bIsSetStreamThread(FALSE),
	m_hSemaphore(NULL),
	m_pDSFilterEnumTuner(NULL),
	m_pDSFilterEnumCapture(NULL),
	m_nNetworkProvider(eNetworkProviderAuto),
	m_nDefaultNetwork(eDefaultNetworkSPHD),
	m_bOpened(FALSE),
	m_dwTargetSpace(CBonTuner::SPACE_INVALID),
	m_dwCurSpace(CBonTuner::SPACE_INVALID),
	m_dwTargetChannel(CBonTuner::CHANNEL_INVALID),
	m_dwCurChannel(CBonTuner::CHANNEL_INVALID),
	m_nCurTone(CBonTuner::TONE_UNKNOWN),
	m_bIsEnabledTSMF(FALSE),
	m_lResetFilter(0),
	m_PacketSize(0),
	m_hModuleTunerSpecials(NULL),
	m_pIBdaSpecials(NULL),
	m_pIBdaSpecials2(NULL)
{
	// �C���X�^���X���X�g�Ɏ��g��o�^
	::EnterCriticalSection(&st_LockInstanceList);
	st_InstanceList.push_back(this);
	::LeaveCriticalSection(&st_LockInstanceList);

	setlocale(LC_CTYPE, "ja_JP.SJIS");

	HANDLE h = ::GetCurrentProcess();
	::DuplicateHandle(h, h, h, &m_hProcess, 0, FALSE, DUPLICATE_SAME_ACCESS);

	ReadIniFile();

	m_TsBuff.SetSize(m_nBuffSize, m_nMaxBuffCount);
	m_DecodedTsBuff.SetSize(0, m_nMaxBuffCount);

	// COM������p�X���b�h�N��
	m_aCOMProc.hThread = ::CreateThread(NULL, 0, CBonTuner::COMProcThread, this, 0, NULL);

	// �X���b�h�v���C�I���e�B
	if (m_aCOMProc.hThread != NULL && m_nThreadPriorityCOM != THREAD_PRIORITY_ERROR_RETURN) {
		::SetThreadPriority(m_aCOMProc.hThread, m_nThreadPriorityCOM);
	}

	// timeBeginPeriod()�Őݒ肷��Windows�̍ŏ��^�C�}����\(msec)
	if (m_nPeriodicTimer != 0) {
		if (timeBeginPeriod(m_nPeriodicTimer) == TIMERR_NOCANDO) {
			m_nPeriodicTimer = 0;
		}
	}
}

CBonTuner::~CBonTuner()
{
	OutputDebug(L"~CBonTuner called.\n");
	CloseTuner();

	// timeBeginPeriod()�̌�n��
	if (m_nPeriodicTimer != 0) {
		timeEndPeriod(m_nPeriodicTimer);
	}

	// COM������p�X���b�h�I��
	if (m_aCOMProc.hThread) {
		::SetEvent(m_aCOMProc.hTerminateRequest);
		::WaitForSingleObject(m_aCOMProc.hThread, INFINITE);
		SAFE_CLOSE_HANDLE(m_aCOMProc.hThread);
	}

	SAFE_CLOSE_HANDLE(m_hStreamThread);
	SAFE_CLOSE_HANDLE(m_hProcess);

	// �C���X�^���X���X�g���玩�g���폜
	::EnterCriticalSection(&st_LockInstanceList);
	st_InstanceList.remove(this);
	::LeaveCriticalSection(&st_LockInstanceList);
}

/////////////////////////////////////
//
// IBonDriver2 APIs
//
/////////////////////////////////////

const BOOL CBonTuner::OpenTuner(void)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	BOOL ret = FALSE;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqOpenTuner;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.OpenTuner;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const BOOL CBonTuner::_OpenTuner(void)
{
	if (m_bOpened)
		return TRUE;

	HRESULT hr;

	do {
		// �t�B���^�O���t�̍쐬
		if (FAILED(hr = InitializeGraphBuilder()))
			break;

		// �`���[�j���O�X�y�[�X�̓Ǎ�
		if (FAILED(hr = CreateTuningSpace()))
			break;

		// �l�b�g���[�N�v���o�C�_
		if (FAILED(hr = LoadNetworkProvider()))
			break;

		// �`���[�j���O�X�y�[�X������
		if (FAILED(hr = InitTuningSpace()))
			break;

		// ���[�h���ׂ��`���[�i�E�L���v�`���̃��X�g�쐬
		if (m_UsableTunerCaptureList.empty() && FAILED(hr = InitDSFilterEnum()))
			break;

		// �`���[�i�E�L���v�`���Ȍ�̍\�z�Ǝ��s
		if (FAILED(hr = LoadAndConnectDevice()))
			break;

		OutputDebug(L"Build graph Successfully.\n");

		// �`���[�i�̐M����Ԏ擾�p�C���^�[�t�F�[�X�̎擾�i���s���Ă����s�j
		if (m_bSignalLockedJudgeTypeSS || m_bSignalLevelGetTypeSS) {
			hr = LoadTunerSignalStatisticsTunerNode();
		}
		if (m_bSignalLockedJudgeTypeDemodSS || m_bSignalLevelGetTypeDemodSS) {
			hr = LoadTunerSignalStatisticsDemodNode();
		}

		// TS��M�C�x���g�쐬
		m_hOnStreamEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

		// Decode�C�x���g�쐬
		m_hOnDecodeEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);

		// Decode������p�X���b�h�N��
		m_aDecodeProc.hThread = ::CreateThread(NULL, 0, CBonTuner::DecodeProcThread, this, 0, NULL);

		// �X���b�h�v���C�I���e�B
		if (m_aDecodeProc.hThread != NULL && m_nThreadPriorityDecode != THREAD_PRIORITY_ERROR_RETURN) {
			::SetThreadPriority(m_aDecodeProc.hThread, m_nThreadPriorityDecode);
		}

		// �R�[���o�b�N�֐��Z�b�g
		StartRecv();

		m_bOpened = TRUE;

		return TRUE;

	} while(0);

	// �����ɓ��B�����Ƃ������Ƃ͉��炩�̃G���[�Ŏ��s����
	_CloseTuner();

	return FALSE;
}

void CBonTuner::CloseTuner(void)
{
	if (m_aCOMProc.hThread == NULL)
		return;

	DWORD dw;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqCloseTuner;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return;
}

void CBonTuner::_CloseTuner(void)
{
	m_bOpened = FALSE;

	// �O���t��~
	StopGraph();

	// �R�[���o�b�N�֐���~
	StopRecv();

	// Decode������p�X���b�h�I��
	if (m_aDecodeProc.hThread) {
		::SetEvent(m_aDecodeProc.hTerminateRequest);
		WaitForSingleObjectWithMessageLoop(m_aDecodeProc.hThread, INFINITE);
		SAFE_CLOSE_HANDLE(m_aDecodeProc.hThread);
	}

	// Decode�C�x���g�J��
	SAFE_CLOSE_HANDLE(m_hOnDecodeEvent);

	// TS��M�C�x���g���
	SAFE_CLOSE_HANDLE(m_hOnStreamEvent);

	// �o�b�t�@���
	PurgeTsStream();
	SAFE_DELETE(m_LastBuff);

	// �`���[�i�̐M����Ԏ擾�p�C���^�[�t�F�[�X���
	UnloadTunerSignalStatistics();

	// �O���t���
	CleanupGraph();

	m_dwTargetSpace = m_dwCurSpace = CBonTuner::SPACE_INVALID;
	m_dwTargetChannel = m_dwCurChannel = CBonTuner::CHANNEL_INVALID;
	m_nCurTone = CBonTuner::TONE_UNKNOWN;

	if (m_hSemaphore) {
		try {
			::ReleaseSemaphore(m_hSemaphore, 1, NULL);
			SAFE_CLOSE_HANDLE(m_hSemaphore);
		} catch (...) {
			OutputDebug(L"Exception in ReleaseSemaphore.\n");
		}
	}

	return;
}

const BOOL CBonTuner::SetChannel(const BYTE byCh)
{
	// IBonDriver (not IBonDriver2) �p�C���^�[�t�F�[�X; obsolete?
	return SetChannel(0UL, DWORD(byCh));
}

const float CBonTuner::GetSignalLevel(void)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	float ret = 0.0F;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqGetSignalLevel;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.GetSignalLevel;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const float CBonTuner::_GetSignalLevel(void)
{
	if (!m_bOpened)
		return -1.0F;

	HRESULT hr;
	float f = 0.0F;

	// �r�b�g���[�g��Ԃ��ꍇ
	if (m_bSignalLevelGetTypeBR) {
		return (float)m_BitRate.GetRate();
	}

	// IBdaSpecials2�ŗL�֐�������Ίۓ���
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->GetSignalStrength(&f)) != E_NOINTERFACE) {
		return f;
	}

	//   get_SignalQuality �M���̕i�������� 1 �` 100 �̒l���擾����B
	//   get_SignalStrength �f�V�x���P�ʂ̐M���̋��x�������l���擾����B 
	int nStrength;
	int nQuality;
	int nLock;

	if (m_dwTargetChannel == CBonTuner::CHANNEL_INVALID)
		// SetChannel()����x���Ă΂�Ă��Ȃ��ꍇ��0��Ԃ�
		return 0.0F;

	GetSignalState(&nStrength, &nQuality, &nLock);
	if (!nLock)
		// Lock�o���Ă��Ȃ��ꍇ��0��Ԃ�
		return 0.0F;
	if (nStrength < 0 && m_bSignalLevelNeedStrength)
		// Strength��-1��Ԃ��ꍇ������
		return (float)nStrength;

	m_fStrength = (double)nStrength;
	m_fQuality = (double)nQuality;

	try {
		f = (float)m_muParser.Eval();
	}
	catch (...) {
	}
	return f;
}

const DWORD CBonTuner::WaitTsStream(const DWORD dwTimeOut)
{
	if( m_hOnDecodeEvent == NULL ){
		return WAIT_ABANDONED;
	}

	DWORD dwRet;
	if (m_nWaitTsSleep) {
		// WaitTsSleep ���w�肳��Ă���ꍇ
		dwRet = ::WaitForSingleObject(m_hOnDecodeEvent, 0);
		// �C�x���g���V�O�i����ԂłȂ���Ύw�莞�ԑҋ@����
		if (dwRet != WAIT_TIMEOUT)
			return dwRet;

		::Sleep(m_nWaitTsSleep);
	}

	// �C�x���g���V�O�i����ԂɂȂ�̂�҂�
	dwRet = ::WaitForSingleObject(m_hOnDecodeEvent, (dwTimeOut)? dwTimeOut : INFINITE);
	return dwRet;
}

const DWORD CBonTuner::GetReadyCount(void)
{
	return (DWORD)m_DecodedTsBuff.Size();
}

const BOOL CBonTuner::GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	BYTE *pSrc = NULL;
	if (GetTsStream(&pSrc, pdwSize, pdwRemain)) {
		if (*pdwSize)
			memcpy(pDst, pSrc, *pdwSize);
		return TRUE;
	}
	return FALSE;
}

const BOOL CBonTuner::GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain)
{
	SAFE_DELETE(m_LastBuff);
	BOOL bRet = TRUE;
	m_LastBuff = m_DecodedTsBuff.Get();
	if (m_LastBuff) {
		*pdwSize = (DWORD)m_LastBuff->Size;
		*ppDst = m_LastBuff->pbyBuff;
		*pdwRemain = (DWORD)m_DecodedTsBuff.Size();
	}
	else {
		*pdwSize = 0;
		*ppDst = NULL;
		*pdwRemain = 0;
		bRet = FALSE;
	}
	return bRet;
}

void CBonTuner::PurgeTsStream(void)
{
	// m_LastBuff �͎Q�Ƃ���Ă���\��������̂� delete ���Ȃ�

	// ��MTS�o�b�t�@
	m_TsBuff.Purge();

	// �f�R�[�h��TS�o�b�t�@
	m_DecodedTsBuff.Purge();

	// �k���p�P�b�g�폜���������Z�b�g
	::InterlockedExchange(&m_lResetFilter, 1);

	// �r�b�g���[�g�v�Z�p�N���X
	m_BitRate.Clear();
}

LPCTSTR CBonTuner::GetTunerName(void)
{
	return m_aTunerParam.sTunerName.c_str();
}

const BOOL CBonTuner::IsTunerOpening(void)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	BOOL ret = FALSE;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqIsTunerOpening;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.IsTunerOpening;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const BOOL CBonTuner::_IsTunerOpening(void)
{
	return m_bOpened;
}

LPCTSTR CBonTuner::EnumTuningSpace(const DWORD dwSpace)
{
	if (dwSpace < m_TuningData.dwNumSpace) {
		auto itSpace = m_TuningData.Spaces.find(dwSpace);
		if (itSpace != m_TuningData.Spaces.end())
			return itSpace->second.sTuningSpaceName.c_str();
		else
			return _T("-");
	}
	return NULL;
}

LPCTSTR CBonTuner::EnumChannelName(const DWORD dwSpace, const DWORD dwChannel)
{
	auto itSpace = m_TuningData.Spaces.find(dwSpace);
	if (itSpace != m_TuningData.Spaces.end()) {
		if (dwChannel < itSpace->second.dwNumChannel) {
			auto itCh = itSpace->second.Channels.find(dwChannel);
			if (itCh != itSpace->second.Channels.end())
				return itCh->second.sServiceName.c_str();
			else
				return _T("----");
		}
	}
	return NULL;
}

const BOOL CBonTuner::SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
	if (m_aCOMProc.hThread == NULL)
		return FALSE;

	DWORD dw;
	BOOL ret = FALSE;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqSetChannel;
	m_aCOMProc.uParam.SetChannel.dwSpace = dwSpace;
	m_aCOMProc.uParam.SetChannel.dwChannel = dwChannel;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.SetChannel;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const BOOL CBonTuner::_SetChannel(const DWORD dwSpace, const DWORD dwChannel)
{
	HRESULT hr;

	OutputDebug(L"SetChannel(%d, %d)\n", dwSpace, dwChannel);

	m_dwTargetSpace = m_dwCurSpace = CBonTuner::SPACE_INVALID;
	m_dwTargetChannel = m_dwCurChannel = CBonTuner::CHANNEL_INVALID;

	auto itSpace = m_TuningData.Spaces.find(dwSpace);
	if (itSpace == m_TuningData.Spaces.end()) {
		OutputDebug(L"    Invalid channel space.\n");
		return FALSE;
	}

	if (dwChannel >= itSpace->second.dwNumChannel) {
		OutputDebug(L"    Invalid channel number.\n");
		return FALSE;
	}

	auto itCh = itSpace->second.Channels.find(dwChannel);
	if (itCh == itSpace->second.Channels.end()) {
		OutputDebug(L"    Reserved channel number.\n");
		return FALSE;
	}

	if (!m_bOpened) {
		OutputDebug(L"    Tuner not opened.\n");
		return FALSE;
	}

	m_bRecvStarted = FALSE;
	PurgeTsStream();
	TuningSpaceData * TuningSpace = &itSpace->second;
	ChData * Ch = &itCh->second;
	m_LastTuningParam.Frequency = Ch->Frequency + TuningSpace->FrequencyOffset;					// ���g��(MHz)
	m_LastTuningParam.Polarisation = PolarisationMapping[Ch->Polarisation];						// �M���̕Δg
	m_LastTuningParam.Antenna = m_aSatellite[Ch->Satellite].Polarisation[Ch->Polarisation];		// �A���e�i�ݒ�f�[�^
	m_LastTuningParam.Modulation = m_aModulationType[Ch->ModulationType];						// �ϒ������ݒ�f�[�^
	m_LastTuningParam.ONID = Ch->ONID;															// �I���W�i���l�b�g���[�NID / PhysicalChannel (ATSC / Digital Cable)
	m_LastTuningParam.TSID = Ch->TSID;															// �g�����X�|�[�g�X�g���[��ID / Channel (ATSC / Digital Cable)
	m_LastTuningParam.SID = Ch->SID;															// �T�[�r�XID / MinorChannel (ATSC / Digital Cable)
	m_LastTuningParam.MajorChannel = Ch->MajorChannel;											// MajorChannel (Digital Cable)
	m_LastTuningParam.SourceID = Ch->SourceID;													// SourceID (Digital Cable)
	m_LastTuningParam.IniSpaceID = dwSpace;														// ini�t�@�C���œǍ��܂ꂽ�`���[�j���O�X�y�[�X�ԍ�
	m_LastTuningParam.IniChannelID = dwChannel;													// ini�t�@�C���œǍ��܂ꂽ�`�����l���ԍ�

	// IBdaSpecials�Ŏ��O�̏������K�v�Ȃ�s��
	if (m_pIBdaSpecials2)
		hr = m_pIBdaSpecials2->PreLockChannel(&m_LastTuningParam);

	BOOL bRet = LockChannel(&m_LastTuningParam, m_bLockTwice && Ch->LockTwiceTarget);

	// IBdaSpecials�Œǉ��̏������K�v�Ȃ�s��
	if (m_pIBdaSpecials2)
		hr = m_pIBdaSpecials2->PostLockChannel(&m_LastTuningParam);

	SleepWithMessageLoop(100);
	PurgeTsStream();

	// TSMF�����ݒ�
	switch (itSpace->second.TSMFMode) {
	case 0:		// OFF
		m_TSMFParser.Disable();
		m_bIsEnabledTSMF = FALSE;
		break;
	case 1:		// TSID
		m_TSMFParser.SetTSID((WORD)Ch->ONID, (WORD)Ch->TSID, FALSE);
		m_bIsEnabledTSMF = TRUE;
		break;
	case 2:		// Relative
		m_TSMFParser.SetTSID(0xffff, (WORD)Ch->TSID, TRUE);
		m_bIsEnabledTSMF = TRUE;
		break;
	}

	m_bRecvStarted = TRUE;

	// SetChannel()�����݂��`���[�j���O�X�y�[�X�ԍ��ƃ`�����l���ԍ�
	m_dwTargetSpace = dwSpace;
	m_dwTargetChannel = dwChannel;

	if (bRet) {
		OutputDebug(L"SetChannel success.\n");
		m_dwCurSpace = dwSpace;
		m_dwCurChannel = dwChannel;
		return TRUE;
	}
	// m_byCurTone = CBonTuner::TONE_UNKNOWN;

	OutputDebug(L"SetChannel failed.\n");
	if (m_bAlwaysAnswerLocked)
		return TRUE;
	return FALSE;
}

const DWORD CBonTuner::GetCurSpace(void)
{
	if (m_aCOMProc.hThread == NULL)
		return CBonTuner::SPACE_INVALID;

	DWORD dw;
	DWORD ret = CBonTuner::SPACE_INVALID;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqGetCurSpace;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.GetCurSpace;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const DWORD CBonTuner::_GetCurSpace(void)
{
	if (m_bAlwaysAnswerLocked)
		return m_dwTargetSpace;

	return m_dwCurSpace;
}

const DWORD CBonTuner::GetCurChannel(void)
{
	if (m_aCOMProc.hThread == NULL)
		return CBonTuner::CHANNEL_INVALID;

	DWORD dw;
	DWORD ret = CBonTuner::CHANNEL_INVALID;

	::EnterCriticalSection(&m_aCOMProc.csLock);

	m_aCOMProc.nRequest = enumCOMRequest::eCOMReqGetCurChannel;
	::SetEvent(m_aCOMProc.hReqEvent);
	HANDLE h[2] = {
		m_aCOMProc.hEndEvent,
		m_aCOMProc.hThread
	};
	dw = ::WaitForMultipleObjects(2, h, FALSE, INFINITE);
	if (dw == WAIT_OBJECT_0) {
		ret = m_aCOMProc.uRetVal.GetCurChannel;
	}

	::LeaveCriticalSection(&m_aCOMProc.csLock);
	return ret;
}

const DWORD CBonTuner::_GetCurChannel(void)
{
	if (m_bAlwaysAnswerLocked)
		return m_dwTargetChannel;

	return m_dwCurChannel;
}

void CBonTuner::Release(void)
{
	OutputDebug(L"CBonTuner::Release called.\n");

	delete this;
}

DWORD WINAPI CBonTuner::COMProcThread(LPVOID lpParameter)
{
	BOOL terminate = FALSE;
	CBonTuner* pSys = (CBonTuner*) lpParameter;
	COMProc* pCOMProc = &pSys->m_aCOMProc;
	HRESULT hr;

	OutputDebug(L"COMProcThread: Thread created.\n");

	// COM������
	hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);

	HANDLE h[2] = {
		pCOMProc->hTerminateRequest,
		pCOMProc->hReqEvent
	};

	while (!terminate) {
		DWORD ret = WaitForMultipleObjectsWithMessageLoop(2, h, FALSE, 1000);
		switch (ret)
		{
		case WAIT_OBJECT_0:
			terminate = TRUE;
			break;

		case WAIT_OBJECT_0 + 1:
			switch (pCOMProc->nRequest)
			{

			case eCOMReqOpenTuner:
				pCOMProc->uRetVal.OpenTuner = pSys->_OpenTuner();
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqCloseTuner:
				// OpenTuner�ELockChannel�̍Ď��s���Ȃ璆�~
				pCOMProc->ResetReOpenTuner();
				pCOMProc->ResetReLockChannel();

				pSys->_CloseTuner();
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqSetChannel:
				// �ُ팟�m�Ď��^�C�}�[������
				pCOMProc->ResetWatchDog();

				// LockChannel�̍Ď��s���Ȃ璆�~
				pCOMProc->ResetReLockChannel();

				// OpenTuner�̍Ď��s���Ȃ�FALSE��Ԃ�
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->ClearReOpenChannel();
					pCOMProc->uRetVal.SetChannel = FALSE;
				}
				else {
					pCOMProc->uRetVal.SetChannel = pSys->_SetChannel(pCOMProc->uParam.SetChannel.dwSpace, pCOMProc->uParam.SetChannel.dwChannel);
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqGetSignalLevel:
				// OpenTuner�̍Ď��s���Ȃ�0��Ԃ�
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.GetSignalLevel = 0.0F;
				}
				else {
					pCOMProc->uRetVal.GetSignalLevel = pSys->_GetSignalLevel();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqIsTunerOpening:
				// OpenTuner�̍Ď��s���Ȃ�TRUE��Ԃ�
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.IsTunerOpening = TRUE;
				}
				else {
					pCOMProc->uRetVal.IsTunerOpening = pSys->_IsTunerOpening();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqGetCurSpace:
				// OpenTuner�̍Ď��s���Ȃ�ޔ�l��Ԃ�
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.GetCurSpace = pCOMProc->dwReOpenSpace;
				}
				else {
					pCOMProc->uRetVal.GetCurSpace = pSys->_GetCurSpace();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			case eCOMReqGetCurChannel:
				// OpenTuner�̍Ď��s���Ȃ�ޔ�l��Ԃ�
				if (pCOMProc->bDoReOpenTuner) {
					pCOMProc->uRetVal.GetCurChannel = pCOMProc->dwReOpenChannel;
				}
				else {
					pCOMProc->uRetVal.GetCurChannel = pSys->_GetCurChannel();
				}
				::SetEvent(pCOMProc->hEndEvent);
				break;

			default:
				break;
			}
			break;

		case WAIT_TIMEOUT:
			break;

		case WAIT_FAILED:
		default:
			DWORD err = ::GetLastError();
			OutputDebug(L"COMProcThread: Unknown error (ret=%d, LastError=0x%08x).\n", ret, err);
			terminate = TRUE;
			break;
		}

		if (terminate)
			break;

		// �X�g���[���X���b�h�v���C�I���e�B�̕ύX
		if (pSys->m_bIsSetStreamThread) {
			if (pSys->m_nThreadPriorityStream != THREAD_PRIORITY_ERROR_RETURN) {
				OutputDebug(L"COMProcThread: Current stream Thread priority = %d.\n", ::GetThreadPriority(pSys->m_hStreamThread));
				::SetThreadPriority(pSys->m_hStreamThread, pSys->m_nThreadPriorityStream);
				OutputDebug(L"COMProcThread: After changed stream Thread priority = %d.\n", ::GetThreadPriority(pSys->m_hStreamThread));
			}
			pSys->m_bIsSetStreamThread = FALSE;
		}

		// �ُ팟�m�����J�o���[
		// 1000ms������
		if (pCOMProc->CheckTick()) {

			// SetChannel()���s���̃o�b�N�O�����hCH�ؑ֊J�n
			if (pSys->m_bBackgroundChannelLock && pSys->m_dwCurChannel == CBonTuner::CHANNEL_INVALID && pSys->m_dwTargetChannel != CBonTuner::CHANNEL_INVALID) {
				OutputDebug(L"COMProcThread: Background retry.\n");
				pCOMProc->SetReLockChannel();
			}

			// �ُ팟�m
			if (!pCOMProc->bDoReLockChannel && !pCOMProc->bDoReOpenTuner && pSys->m_dwCurChannel != CBonTuner::CHANNEL_INVALID) {

				// SignalLock�̏�Ԋm�F
				if (pSys->m_nWatchDogSignalLocked != 0) {
					int lock = 0;
					pSys->GetSignalState(NULL, NULL, &lock);
					if (pCOMProc->CheckSignalLockErr(lock, pSys->m_nWatchDogSignalLocked * 1000)) {
						// �`�����l�����b�N�Ď��s
						OutputDebug(L"COMProcThread: WatchDogSignalLocked time is up.\n");
						pCOMProc->SetReLockChannel();
					}
				} // SignalLock�̏�Ԋm�F

				// BitRate�m�F
				if (pSys->m_nWatchDogBitRate != 0) {
					if (pCOMProc->CheckBitRateErr((pSys->m_BitRate.GetRate() > 0.0), pSys->m_nWatchDogBitRate * 1000)) {
						// �`�����l�����b�N�Ď��s
						OutputDebug(L"COMProcThread: WatchDogBitRate time is up.\n");
						pCOMProc->SetReLockChannel();
					}
				} // BitRate�m�F
			} // �ُ팟�m

			// CH�ؑ֓��쎎�s���OpenTuner�Ď��s
			if (pCOMProc->bDoReOpenTuner) {
				// OpenTuner�Ď��s
				pSys->_CloseTuner();
				if (pSys->_OpenTuner() && (!pCOMProc->CheckReOpenChannel() || pSys->_SetChannel(pCOMProc->dwReOpenSpace, pCOMProc->dwReOpenChannel))) {
					// OpenTuner�ɐ������ASetChannnel�ɐ����������͕K�v�Ȃ�
					OutputDebug(L"COMProcThread: Re-OpenTuner SUCCESS.\n");
					pCOMProc->ResetReOpenTuner();
				}
				else {
					// ���s...���̂܂܎�����`�������W����
					OutputDebug(L"COMProcThread: Re-OpenTuner FAILED.\n");
				}
			} // CH�ؑ֓��쎎�s���OpenTuner�Ď��s

			// �ُ팟�m��`�����l�����b�N�Ď��s
			if (!pCOMProc->bDoReOpenTuner && pCOMProc->bDoReLockChannel) {
				// �`�����l�����b�N�Ď��s
				if (pSys->LockChannel(&pSys->m_LastTuningParam, FALSE)) {
					// LockChannel�ɐ�������
					OutputDebug(L"COMProcThread: Re-LockChannel SUCCESS.\n");
					pCOMProc->ResetReLockChannel();
				}
				else {
					// LockChannel���s
					OutputDebug(L"COMProcThread: Re-LockChannel FAILED.\n");
					if (pSys->m_nReOpenWhenGiveUpReLock != 0) {
						// CH�ؑ֓��쎎�s�񐔐ݒ�l��0�ȊO
						if (pCOMProc->CheckReLockFailCount(pSys->m_nReOpenWhenGiveUpReLock)) {
							// CH�ؑ֓��쎎�s�񐔂𒴂����̂�OpenTuner�Ď��s
							OutputDebug(L"COMProcThread: ReOpenWhenGiveUpReLock count is up.\n");
							pCOMProc->SetReOpenTuner(pSys->m_dwTargetSpace, pSys->m_dwTargetChannel);
							pCOMProc->ResetReLockChannel();
						}
					}
				}
			} // �ُ팟�m��`�����l�����b�N�Ď��s
		} // 1000ms������
	} // while (!terminate)

	// �O���t�֌W�̉��
	pSys->_CloseTuner();

	// DS�t�B���^�[�񋓂ƃ`���[�i�E�L���v�`���̃��X�g���폜
	SAFE_DELETE(pSys->m_pDSFilterEnumTuner);
	SAFE_DELETE(pSys->m_pDSFilterEnumCapture);
	pSys->m_UsableTunerCaptureList.clear();

	::CoUninitialize();
	OutputDebug(L"COMProcThread: Thread terminated.\n");

	return 0;
}

DWORD WINAPI CBonTuner::DecodeProcThread(LPVOID lpParameter)
{
	BOOL terminate = FALSE;
	CBonTuner* pSys = (CBonTuner*)lpParameter;

	BOOL bNeedDecode = FALSE;

	HRESULT hr;

	OutputDebug(L"DecodeProcThread: Thread created.\n");

	// COM������
	hr = ::CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE | COINIT_SPEED_OVER_MEMORY);

	// IBdaSpecials�ɂ��f�R�[�h�������K�v���ǂ���
	BOOL b = FALSE;
	if (pSys->m_pIBdaSpecials2 && SUCCEEDED(hr = pSys->m_pIBdaSpecials2->IsDecodingNeeded(&b))) {
		if (b)
			bNeedDecode = TRUE;
	}
	OutputDebug(L"DecodeProcThread: Detected IBdaSpecials decoding=%d.\n", bNeedDecode);

	HANDLE h[2] = {
		pSys->m_aDecodeProc.hTerminateRequest,
		pSys->m_hOnStreamEvent
	};

	while (!terminate) {
		DWORD remain = pSys->m_BitRate.CheckRate();
		DWORD ret = WaitForMultipleObjectsWithMessageLoop(2, h, FALSE, remain);
		switch (ret)
		{
		case WAIT_OBJECT_0:
			terminate = TRUE;
			break;
		case WAIT_OBJECT_0 + 1:
			{
				// TS�o�b�t�@����̃f�[�^�擾
				while (TS_DATA *pBuff = pSys->m_TsBuff.Get()) {
					// �K�v�Ȃ��IBdaSpecials�ɂ��f�R�[�h�������s��
					if (bNeedDecode) {
						pSys->m_pIBdaSpecials2->Decode(pBuff->pbyBuff, (DWORD)pBuff->Size);
					}

					// �擾�����o�b�t�@���f�R�[�h�ς݃o�b�t�@�ɒǉ�
					if (pSys->m_bIsEnabledTSMF) {
						// TSMF�̏������s��
						BYTE * newBuf = NULL;
						size_t newBufSize = 0;
						pSys->m_TSMFParser.ParseTsBuffer(pBuff->pbyBuff, pBuff->Size, &newBuf, &newBufSize, pSys->m_bDeleteNullPackets);
						if (newBuf) {
							TS_DATA * pNewTS = new TS_DATA(newBuf, (DWORD)newBufSize, FALSE);
							pSys->m_DecodedTsBuff.Add(pNewTS);
						}
						SAFE_DELETE(pBuff);
					}
					else if (pSys->m_bDeleteNullPackets) {
						if (::InterlockedExchange(&pSys->m_lResetFilter, 0)) {
							// ���Z�b�g
							pSys->m_PacketSize = 0;
							pSys->m_FilterBuf.clear();
						}

						// ��{�I�Ɏ擾�f�[�^�̃������̈�𗘗p���邱�ƂŃ������m�ۂ��ȗ����邪
						// ���[�����~�ς���Ƃ������̈���g�����ĉ�������
						size_t sizeToExtend = pSys->m_FilterBuf.size() >= 188 * 16 ? pBuff->Size + 188 * 16 : 0;
						pSys->m_FilterBuf.insert(pSys->m_FilterBuf.end(), pBuff->pbyBuff, pBuff->pbyBuff + pBuff->Size);
						if (sizeToExtend > 0) {
							// �g��
							SAFE_DELETE(pBuff);
							pBuff = new TS_DATA(new BYTE[sizeToExtend], sizeToExtend, FALSE);
						}

						size_t rpos = 0;
						size_t wpos = 0;
						while (rpos + pSys->m_PacketSize <= pSys->m_FilterBuf.size() && wpos + 188 <= pBuff->Size) {
							if (pSys->m_PacketSize == 0) {
								// TS�p�P�b�g�̓���
								size_t truncate = 0;
								CTSMFParser::SyncPacket(pSys->m_FilterBuf.data() + rpos, pSys->m_FilterBuf.size() - rpos, &truncate, &pSys->m_PacketSize);
								rpos += truncate;
								if (truncate == 0)
									// TS�o�b�t�@�̃f�[�^�T�C�Y�����������ē����ł��Ȃ�
									break;
							}
							else if (pSys->m_FilterBuf[rpos] != CTSMFParser::TS_PACKET_SYNC_BYTE) {
								// �����O��
								pSys->m_PacketSize = 0;
							}
							else {
								WORD pid = ((pSys->m_FilterBuf[rpos + 1] << 8) | pSys->m_FilterBuf[rpos + 2]) & 0x1fff;
								if (pid != 0x1fff) {
									// �k���p�P�b�g�łȂ��̂Œǉ�
									memcpy(pBuff->pbyBuff + wpos, pSys->m_FilterBuf.data() + rpos, 188);
									wpos += 188;
								}
								rpos += pSys->m_PacketSize;
							}
						}

						pSys->m_FilterBuf.erase(pSys->m_FilterBuf.begin(), pSys->m_FilterBuf.begin() + rpos);
						if (wpos > 0) {
							pBuff->Size = wpos;
							pSys->m_DecodedTsBuff.Add(pBuff);
						}
						else {
							SAFE_DELETE(pBuff);
						}
					}
					else {
						// TSMF�̏������s��Ȃ��ꍇ�͂��̂܂ܒǉ�
						pSys->m_DecodedTsBuff.Add(pBuff);
					}

					// ��M�C�x���g�Z�b�g
					if (pSys->m_DecodedTsBuff.Size() >= pSys->m_nWaitTsCount)
						::SetEvent(pSys->m_hOnDecodeEvent);
				}
			}
			break;
		case WAIT_TIMEOUT:
			break;
		case WAIT_FAILED:
		default:
			DWORD err = ::GetLastError();
			OutputDebug(L"DecodeProcThread: Unknown error (ret=%d, LastError=0x%08x).\n", ret, err);
			terminate = TRUE;
			break;
		}
	}

	::CoUninitialize();
	OutputDebug(L"DecodeProcThread: Thread terminated.\n");

	return 0;
}

int CALLBACK CBonTuner::RecvProc(void* pParam, BYTE* pbData, size_t size)
{
	CBonTuner* pSys = (CBonTuner*)pParam;

	if (pSys->m_hStreamThread == NULL) {
		::DuplicateHandle(pSys->m_hProcess, GetCurrentThread(), GetCurrentProcess(), &(pSys->m_hStreamThread), 0, FALSE, 2);
		pSys->m_bIsSetStreamThread = TRUE;
	}

	pSys->m_BitRate.AddRate((DWORD)size);

	if (pSys->m_bRecvStarted) {
		if (pSys->m_TsBuff.AddData(pbData, size)) {
			::SetEvent(pSys->m_hOnStreamEvent);
		}
	}

	return 0;
}

void CBonTuner::StartRecv(void)
{
	if (m_pITsWriter)
		m_pITsWriter->SetCallBackRecv(RecvProc, this);
	m_bRecvStarted = TRUE;
}

void CBonTuner::StopRecv(void)
{
	if (m_pITsWriter)
		m_pITsWriter->SetCallBackRecv(NULL, this);
	m_bRecvStarted = FALSE;
}

void CBonTuner::ReadIniFile(void)
{
	const std::map<const std::wstring, const int, std::less<>> mapThreadPriority = {
		{ L"",                              THREAD_PRIORITY_ERROR_RETURN },
		{ L"THREAD_PRIORITY_IDLE",          THREAD_PRIORITY_IDLE },
		{ L"THREAD_PRIORITY_LOWEST",        THREAD_PRIORITY_LOWEST },
		{ L"THREAD_PRIORITY_BELOW_NORMAL",  THREAD_PRIORITY_BELOW_NORMAL },
		{ L"THREAD_PRIORITY_NORMAL",        THREAD_PRIORITY_NORMAL },
		{ L"THREAD_PRIORITY_ABOVE_NORMAL",  THREAD_PRIORITY_ABOVE_NORMAL },
		{ L"THREAD_PRIORITY_HIGHEST",       THREAD_PRIORITY_HIGHEST },
		{ L"THREAD_PRIORITY_TIME_CRITICAL", THREAD_PRIORITY_TIME_CRITICAL },
	};

	const std::map<const std::wstring, const int, std::less<>> mapModulationType = {
		{ L"BDA_MOD_NOT_SET",          ModulationType::BDA_MOD_NOT_SET },
		{ L"BDA_MOD_NOT_DEFINED",      ModulationType::BDA_MOD_NOT_DEFINED },
		{ L"BDA_MOD_16QAM",            ModulationType::BDA_MOD_16QAM },
		{ L"BDA_MOD_32QAM",            ModulationType::BDA_MOD_32QAM },
		{ L"BDA_MOD_64QAM",            ModulationType::BDA_MOD_64QAM },
		{ L"BDA_MOD_80QAM",            ModulationType::BDA_MOD_80QAM },
		{ L"BDA_MOD_96QAM",            ModulationType::BDA_MOD_96QAM },
		{ L"BDA_MOD_112QAM",           ModulationType::BDA_MOD_112QAM },
		{ L"BDA_MOD_128QAM",           ModulationType::BDA_MOD_128QAM },
		{ L"BDA_MOD_160QAM",           ModulationType::BDA_MOD_160QAM },
		{ L"BDA_MOD_192QAM",           ModulationType::BDA_MOD_192QAM },
		{ L"BDA_MOD_224QAM",           ModulationType::BDA_MOD_224QAM },
		{ L"BDA_MOD_256QAM",           ModulationType::BDA_MOD_256QAM },
		{ L"BDA_MOD_320QAM",           ModulationType::BDA_MOD_320QAM },
		{ L"BDA_MOD_384QAM",           ModulationType::BDA_MOD_384QAM },
		{ L"BDA_MOD_448QAM",           ModulationType::BDA_MOD_448QAM },
		{ L"BDA_MOD_512QAM",           ModulationType::BDA_MOD_512QAM },
		{ L"BDA_MOD_640QAM",           ModulationType::BDA_MOD_640QAM },
		{ L"BDA_MOD_768QAM",           ModulationType::BDA_MOD_768QAM },
		{ L"BDA_MOD_896QAM",           ModulationType::BDA_MOD_896QAM },
		{ L"BDA_MOD_1024QAM",          ModulationType::BDA_MOD_1024QAM },
		{ L"BDA_MOD_QPSK",             ModulationType::BDA_MOD_QPSK },
		{ L"BDA_MOD_BPSK",             ModulationType::BDA_MOD_BPSK },
		{ L"BDA_MOD_OQPSK",            ModulationType::BDA_MOD_OQPSK },
		{ L"BDA_MOD_8VSB",             ModulationType::BDA_MOD_8VSB },
		{ L"BDA_MOD_16VSB",            ModulationType::BDA_MOD_16VSB },
		{ L"BDA_MOD_ANALOG_AMPLITUDE", ModulationType::BDA_MOD_ANALOG_AMPLITUDE },
		{ L"BDA_MOD_ANALOG_FREQUENCY", ModulationType::BDA_MOD_ANALOG_FREQUENCY },
		{ L"BDA_MOD_8PSK",             ModulationType::BDA_MOD_8PSK },
		{ L"BDA_MOD_RF",               ModulationType::BDA_MOD_RF },
		{ L"BDA_MOD_16APSK",           ModulationType::BDA_MOD_16APSK },
		{ L"BDA_MOD_32APSK",           ModulationType::BDA_MOD_32APSK },
		{ L"BDA_MOD_NBC_QPSK",         ModulationType::BDA_MOD_NBC_QPSK },
		{ L"BDA_MOD_NBC_8PSK",         ModulationType::BDA_MOD_NBC_8PSK },
		{ L"BDA_MOD_DIRECTV",          ModulationType::BDA_MOD_DIRECTV },
		{ L"BDA_MOD_ISDB_T_TMCC",      ModulationType::BDA_MOD_ISDB_T_TMCC },
		{ L"BDA_MOD_ISDB_S_TMCC",      ModulationType::BDA_MOD_ISDB_S_TMCC },
	};

	const std::map<const std::wstring, const int, std::less<>> mapFECMethod = {
		{ L"BDA_FEC_METHOD_NOT_SET",     FECMethod::BDA_FEC_METHOD_NOT_SET },
		{ L"BDA_FEC_METHOD_NOT_DEFINED", FECMethod::BDA_FEC_METHOD_NOT_DEFINED },
		{ L"BDA_FEC_VITERBI",            FECMethod::BDA_FEC_VITERBI },
		{ L"BDA_FEC_RS_204_188",         FECMethod::BDA_FEC_RS_204_188 },
		{ L"BDA_FEC_LDPC",               FECMethod::BDA_FEC_LDPC },
		{ L"BDA_FEC_BCH",                FECMethod::BDA_FEC_BCH },
		{ L"BDA_FEC_RS_147_130",         FECMethod::BDA_FEC_RS_147_130 },
	};

	const std::map<const std::wstring, const int, std::less<>> mapBinaryConvolutionCodeRate = {
		{ L"BDA_BCC_RATE_NOT_SET",     BinaryConvolutionCodeRate::BDA_BCC_RATE_NOT_SET },
		{ L"BDA_BCC_RATE_NOT_DEFINED", BinaryConvolutionCodeRate::BDA_BCC_RATE_NOT_DEFINED },
		{ L"BDA_BCC_RATE_1_2",         BinaryConvolutionCodeRate::BDA_BCC_RATE_1_2 },
		{ L"BDA_BCC_RATE_2_3",         BinaryConvolutionCodeRate::BDA_BCC_RATE_2_3 },
		{ L"BDA_BCC_RATE_3_4",         BinaryConvolutionCodeRate::BDA_BCC_RATE_3_4 },
		{ L"BDA_BCC_RATE_3_5",         BinaryConvolutionCodeRate::BDA_BCC_RATE_3_5 },
		{ L"BDA_BCC_RATE_4_5",         BinaryConvolutionCodeRate::BDA_BCC_RATE_4_5 },
		{ L"BDA_BCC_RATE_5_6",         BinaryConvolutionCodeRate::BDA_BCC_RATE_5_6 },
		{ L"BDA_BCC_RATE_5_11",        BinaryConvolutionCodeRate::BDA_BCC_RATE_5_11 },
		{ L"BDA_BCC_RATE_7_8",         BinaryConvolutionCodeRate::BDA_BCC_RATE_7_8 },
		{ L"BDA_BCC_RATE_1_4",         BinaryConvolutionCodeRate::BDA_BCC_RATE_1_4 },
		{ L"BDA_BCC_RATE_1_3",         BinaryConvolutionCodeRate::BDA_BCC_RATE_1_3 },
		{ L"BDA_BCC_RATE_2_5",         BinaryConvolutionCodeRate::BDA_BCC_RATE_2_5 },
		{ L"BDA_BCC_RATE_6_7",         BinaryConvolutionCodeRate::BDA_BCC_RATE_6_7 },
		{ L"BDA_BCC_RATE_8_9",         BinaryConvolutionCodeRate::BDA_BCC_RATE_8_9 },
		{ L"BDA_BCC_RATE_9_10",        BinaryConvolutionCodeRate::BDA_BCC_RATE_9_10 },
	};

	const std::map<const std::wstring, const int, std::less<>> mapTuningSpaceType = {
		{ L"DVB-S/DVB-S2",  enumTunerType::eTunerTypeDVBS },
		{ L"DVB-S2",        enumTunerType::eTunerTypeDVBS },
		{ L"DVB-S",         enumTunerType::eTunerTypeDVBS },
		{ L"DVB-T",         enumTunerType::eTunerTypeDVBT },
		{ L"DVB-C",         enumTunerType::eTunerTypeDVBC },
		{ L"DVB-T2",        enumTunerType::eTunerTypeDVBT2 },
		{ L"ISDB-S",        enumTunerType::eTunerTypeISDBS },
		{ L"ISDB-T",        enumTunerType::eTunerTypeISDBT },
		{ L"ISDB-C",        enumTunerType::eTunerTypeISDBC },
		{ L"ATSC",          enumTunerType::eTunerTypeATSC_Antenna },
		{ L"ATSC CABLE",    enumTunerType::eTunerTypeATSC_Cable },
		{ L"DIGITAL CABLE", enumTunerType::eTunerTypeDigitalCable },
	};

	const std::map<const std::wstring, const int, std::less<>> mapSpecifyTuningSpace = {
		{ L"AUTO",                     enumTuningSpace::eTuningSpaceAuto },
		{ L"DVBTUNINGSPACE",           enumTuningSpace::eTuningSpaceDVB },
		{ L"DVBSTUNINGSPACE",          enumTuningSpace::eTuningSpaceDVBS },
		{ L"ANALOGTVTUNINGSPACE",      enumTuningSpace::eTuningSpaceAnalogTV },
		{ L"ATSCTUNINGSPACE",	       enumTuningSpace::eTuningSpaceATSC },
		{ L"DIGITALCABLETUNINGSPACE",  enumTuningSpace::eTuningSpaceDigitalCable },
	};

	const std::map<const std::wstring, const int, std::less<>> mapSpecifyLocator = {
		{ L"AUTO", eLocatorAuto },
		{ L"DVBTLOCATOR",         enumLocator::eLocatorDVBT },
		{ L"DVBTLOCATOR2",        enumLocator::eLocatorDVBT2 },
		{ L"DVBSLOCATOR",         enumLocator::eLocatorDVBS },
		{ L"DVBCLOCATOR",         enumLocator::eLocatorDVBC },
		{ L"ISDBSLOCATOR",        enumLocator::eLocatorISDBS },
		{ L"ATSCLOCATOR",         enumLocator::eLocatorATSC },
		{ L"DIGITALCABLELOCATOR", enumLocator::eLocatorDigitalCable },
	};

	const std::map<const std::wstring, const int, std::less<>> mapSpecifyITuningSpaceNetworkType = {
		{ L"AUTO",                                       enumNetworkType::eNetworkTypeAuto },
		{ L"STATIC_DVB_TERRESTRIAL_TV_NETWORK_TYPE",     enumNetworkType::eNetworkTypeDVBT },
		{ L"STATIC_DVB_SATELLITE_TV_NETWORK_TYPE",       enumNetworkType::eNetworkTypeDVBS },
		{ L"STATIC_DVB_CABLE_TV_NETWORK_TYPE",           enumNetworkType::eNetworkTypeDVBC },
		{ L"STATIC_ISDB_TERRESTRIAL_TV_NETWORK_TYPE",    enumNetworkType::eNetworkTypeISDBT },
		{ L"STATIC_ISDB_SATELLITE_TV_NETWORK_TYPE",      enumNetworkType::eNetworkTypeISDBS },
		{ L"STATIC_ISDB_CABLE_TV_NETWORK_TYPE",          enumNetworkType::eNetworkTypeISDBC },
		{ L"STATIC_ATSC_TERRESTRIAL_TV_NETWORK_TYPE",    enumNetworkType::eNetworkTypeATSC },
		{ L"STATIC_DIGITAL_CABLE_NETWORK_TYPE",          enumNetworkType::eNetworkTypeDigitalCable },
		{ L"STATIC_BSKYB_TERRESTRIAL_TV_NETWORK_TYPE",   enumNetworkType::eNetworkTypeBSkyB },
		{ L"STATIC_DIRECT_TV_SATELLITE_TV_NETWORK_TYPE", enumNetworkType::eNetworkTypeDIRECTV },
		{ L"STATIC_ECHOSTAR_SATELLITE_TV_NETWORK_TYPE",  enumNetworkType::eNetworkTypeEchoStar },
	};

	const std::map<const std::wstring, const int, std::less<>> mapSpecifyIDVBTuningSpaceSystemType = {
		{ L"AUTO",             enumDVBSystemType::eDVBSystemTypeAuto },
		{ L"DVB_CABLE",        enumDVBSystemType::eDVBSystemTypeDVBC },
		{ L"DVB_TERRESTRIAL",  enumDVBSystemType::eDVBSystemTypeDVBT },
		{ L"DVB_SATELLITE",    enumDVBSystemType::eDVBSystemTypeDVBS },
		{ L"ISDB_TERRESTRIAL", enumDVBSystemType::eDVBSystemTypeISDBT },
		{ L"ISDB_SATELLITE",   enumDVBSystemType::eDVBSystemTypeISDBS },
	};


	const std::map<const std::wstring, const int, std::less<>> mapSpecifyIAnalogTVTuningSpaceInputType = {
		{ L"AUTO",              enumTunerInputType::eTunerInputTypeAuto },
		{ L"TUNERINPUTCABLE",   enumTunerInputType::eTunerInputTypeCable },
		{ L"TUNERINPUTANTENNA", enumTunerInputType::eTunerInputTypeAntenna },
	};

	const std::map<const std::wstring, const int, std::less<>> mapNetworkProvider = {
		{ L"AUTO",                             enumNetworkProvider::eNetworkProviderAuto },
		{ L"MICROSOFT NETWORK PROVIDER",       enumNetworkProvider::eNetworkProviderGeneric },
		{ L"MICROSOFT DVB-S NETWORK PROVIDER", enumNetworkProvider::eNetworkProviderDVBS },
		{ L"MICROSOFT DVB-T NETWORK PROVIDER", enumNetworkProvider::eNetworkProviderDVBT },
		{ L"MICROSOFT DVB-C NETWORK PROVIDER", enumNetworkProvider::eNetworkProviderDVBC },
		{ L"MICROSOFT ATSC NETWORK PROVIDER",  enumNetworkProvider::eNetworkProviderATSC },
	};

	const std::map<const std::wstring, const int, std::less<>> mapDefaultNetwork = {
		{ L"NONE",     enumDefaultNetwork::eDefaultNetworkNone },
		{ L"SPHD",     enumDefaultNetwork::eDefaultNetworkSPHD },
		{ L"BS/CS110", enumDefaultNetwork::eDefaultNetworkBSCS },
		{ L"BS",       enumDefaultNetwork::eDefaultNetworkBSCS },
		{ L"CS110",    enumDefaultNetwork::eDefaultNetworkBSCS },
		{ L"UHF/CATV", enumDefaultNetwork::eDefaultNetworkUHF },
		{ L"UHF",      enumDefaultNetwork::eDefaultNetworkUHF },
		{ L"CATV",     enumDefaultNetwork::eDefaultNetworkUHF },
		{ L"DUAL",     enumDefaultNetwork::eDefaultNetworkDual },
	};

	const std::map<const std::wstring, const int, std::less<>> mapSignalLevelCalcType = {
		{ L"SSSTRENGTH",      enumSignalLevelCalcType::eSignalLevelCalcTypeSSStrength },
		{ L"SSQUALITY",       enumSignalLevelCalcType::eSignalLevelCalcTypeSSQuality },
		{ L"SSMUL",           enumSignalLevelCalcType::eSignalLevelCalcTypeSSMul },
		{ L"SSADD",           enumSignalLevelCalcType::eSignalLevelCalcTypeSSAdd },
		{ L"SSFORMULA",       enumSignalLevelCalcType::eSignalLevelCalcTypeSSFormula },
		{ L"TUNERSTRENGTH",   enumSignalLevelCalcType::eSignalLevelCalcTypeTunerStrength },
		{ L"TUNERQUALITY",    enumSignalLevelCalcType::eSignalLevelCalcTypeTunerQuality },
		{ L"TUNERMUL",        enumSignalLevelCalcType::eSignalLevelCalcTypeTunerMul },
		{ L"TUNERADD",        enumSignalLevelCalcType::eSignalLevelCalcTypeTunerAdd },
		{ L"TUNERFORMULA",    enumSignalLevelCalcType::eSignalLevelCalcTypeTunerFormula },
		{ L"DEMODSSSTRENGTH", enumSignalLevelCalcType::eSignalLevelCalcTypeDemodSSStrength },
		{ L"DEMODSSQUALITY",  enumSignalLevelCalcType::eSignalLevelCalcTypeDemodSSQuality },
		{ L"DEMODSSMUL",      enumSignalLevelCalcType::eSignalLevelCalcTypeDemodSSMul },
		{ L"DEMODSSADD",      enumSignalLevelCalcType::eSignalLevelCalcTypeDemodSSAdd },
		{ L"DEMODSSFORMULA",  enumSignalLevelCalcType::eSignalLevelCalcTypeDemodSSFormula },
		{ L"BITRATE",         enumSignalLevelCalcType::eSignalLevelCalcTypeBR },
	};

	const std::map<const std::wstring, const int, std::less<>> mapSignalLockedJudgeType = {
		{ L"ALWAYS",        enumSignalLockedJudgeType::eSignalLockedJudgeTypeAlways },
		{ L"SSLOCKED",      enumSignalLockedJudgeType::eSignalLockedJudgeTypeSS },
		{ L"TUNERSTRENGTH", enumSignalLockedJudgeType::eSignalLockedJudgeTypeTuner },
		{ L"DEMODSSLOCKED", enumSignalLockedJudgeType::eSignalLockedJudgeTypeDemodSS },
	};

	const std::map<const std::wstring, const int, std::less<>> mapDiSEqC = {
		{ L"",       LNB_Source::BDA_LNB_SOURCE_NOT_SET },
		{ L"PORT-A", LNB_Source::BDA_LNB_SOURCE_A },
		{ L"PORT-B", LNB_Source::BDA_LNB_SOURCE_B },
		{ L"PORT-C", LNB_Source::BDA_LNB_SOURCE_C },
		{ L"PORT-D", LNB_Source::BDA_LNB_SOURCE_D },
	};

	const std::map<const std::wstring, const int, std::less<>> mapTSMFMode = {
		{ L"OFF",      0 },
		{ L"TSID",     1 },
		{ L"RELATIVE", 2 },
	};

	// INI�t�@�C���̃t�@�C�����擾
	std::wstring tempPath = common::GetModuleName(st_hModule);
	m_sIniFilePath = tempPath + L"ini";

	CIniFileAccess IniFileAccess(m_sIniFilePath);
	int val;

	// DebugLog���L�^���邩�ǂ���
	if (IniFileAccess.ReadKeyB(L"BONDRIVER", L"DebugLog", FALSE)) {
		SetDebugLog(tempPath + L"log");
	}

	//
	// Tuner �Z�N�V����
	//
	IniFileAccess.ReadSection(L"TUNER");
	IniFileAccess.CreateSectionData();

	// GUID0 - GUID99: Tuner�f�o�C�X��GUID ... �w�肳��Ȃ���Ό����������Ɏg�������Ӗ�����B
	// FriendlyName0 - FriendlyName99: Tuner�f�o�C�X��FriendlyName ... �w�肳��Ȃ���Ό����������Ɏg�������Ӗ�����B
	// CaptureGUID0 - CaptureGUID99: Capture�f�o�C�X��GUID ... �w�肳��Ȃ���ΐڑ��\�ȃf�o�C�X����������B
	// CaptureFriendlyName0 - CaptureFriendlyName99: Capture�f�o�C�X��FriendlyName ... �w�肳��Ȃ���ΐڑ��\�ȃf�o�C�X����������B
	for (unsigned int i = 0; i < MAX_GUID; i++) {
		std::wstring key;
		key = L"GUID" + std::to_wstring(i);
		std::wstring tunerGuid = IniFileAccess.ReadKeySSectionData(key, L"");
		key = L"FriendlyName" + std::to_wstring(i);
		std::wstring tunerFriendlyName = IniFileAccess.ReadKeySSectionData(key, L"");
		key = L"CaptureGUID" + std::to_wstring(i);
		std::wstring captureGuid = IniFileAccess.ReadKeySSectionData(key, L"");
		key = L"CaptureFriendlyName" + std::to_wstring(i);
		std::wstring captureFriendlyName = IniFileAccess.ReadKeySSectionData(key, L"");
		if (tunerGuid.length() == 0 && tunerFriendlyName.length() == 0 && captureGuid.length() == 0 && captureFriendlyName.length() == 0) {
			// �ǂ���w�肳��Ă��Ȃ�
			if (i == 0) {
				// �ԍ��Ȃ��̌^���œǍ���
				tunerGuid = IniFileAccess.ReadKeySSectionData(L"GUID", L"");
				tunerFriendlyName = IniFileAccess.ReadKeySSectionData(L"FriendlyName", L"");
				captureGuid = IniFileAccess.ReadKeySSectionData(L"CaptureGUID", L"");
				captureFriendlyName = IniFileAccess.ReadKeySSectionData(L"CaptureFriendlyName", L"");
				// �ǂ���w�肳��Ă��Ȃ��ꍇ�ł��o�^
			}
			else
				break;
		}
		m_aTunerParam.Tuner.emplace(i, TunerSearchData(tunerGuid, tunerFriendlyName, captureGuid, captureFriendlyName));
	}

	// Tuner�f�o�C�X�݂̂�Capture�f�o�C�X�����݂��Ȃ�
	m_aTunerParam.bNotExistCaptureDevice = IniFileAccess.ReadKeyBSectionData(L"NotExistCaptureDevice", FALSE);

	// Tuner��Capture�̃f�o�C�X�C���X�^���X�p�X����v���Ă��邩�̊m�F���s�����ǂ���
	m_aTunerParam.bCheckDeviceInstancePath = IniFileAccess.ReadKeyBSectionData(L"CheckDeviceInstancePath", TRUE);

	// Tuner��: GetTunerName�ŕԂ��`���[�i�� ... �w�肳��Ȃ���΃f�t�H���g����
	//   �g����B���̏ꍇ�A�����`���[�i�𖼑O�ŋ�ʂ��鎖�͂ł��Ȃ�
	m_aTunerParam.sTunerName = common::WStringToTString(IniFileAccess.ReadKeySSectionData(L"Name", L"DVB-S2"));

	// �`���[�i�ŗL�֐����g�p���邩�ǂ����B
	//   �ȉ��� INI �t�@�C���Ŏw��\
	//     "" ... �g�p���Ȃ�(default); "AUTO" ... AUTO
	//     "DLLName" ... �`���[�i�ŗL�֐��̓�����DLL���𒼐ڎw��
	m_aTunerParam.sDLLBaseName = IniFileAccess.ReadKeySSectionData(L"UseSpecial", L"");

	// Tone�M���ؑ֎���Wait����
	m_nToneWait = IniFileAccess.ReadKeyISectionData(L"ToneSignalWait", 100);

	// CH�ؑ֌��Lock�m�F����
	m_nLockWait = IniFileAccess.ReadKeyISectionData(L"ChannelLockWait", 2000);

	// CH�ؑ֌��Lock�m�FDelay����
	m_nLockWaitDelay = IniFileAccess.ReadKeyISectionData(L"ChannelLockWaitDelay", 0);

	// CH�ؑ֌��Lock�m�FRetry��
	m_nLockWaitRetry = IniFileAccess.ReadKeyISectionData(L"ChannelLockWaitRetry", 0);

	// CH�ؑ֓���������I��2�x�s�����ǂ���
	m_bLockTwice = IniFileAccess.ReadKeyBSectionData(L"ChannelLockTwice", FALSE);

	// CH�ؑ֓���������I��2�x�s���ꍇ��Delay����
	m_nLockTwiceDelay = IniFileAccess.ReadKeyISectionData(L"ChannelLockTwiceDelay", 100);

	// SignalLock�ُ̈팟�m����(�b)
	m_nWatchDogSignalLocked = IniFileAccess.ReadKeyISectionData(L"WatchDogSignalLocked", 0);

	// BitRate�ُ̈팟�m����(�b)
	m_nWatchDogBitRate = IniFileAccess.ReadKeyISectionData(L"WatchDogBitRate", 0);

	// �ُ팟�m���A�`���[�i�̍ăI�[�v�������݂�܂ł�CH�ؑ֓��쎎�s��
	m_nReOpenWhenGiveUpReLock = IniFileAccess.ReadKeyISectionData(L"ReOpenWhenGiveUpReLock", 0);

	// �`���[�i�̍ăI�[�v�������݂�ꍇ�ɕʂ̃`���[�i��D�悵�Č������邩�ǂ���
	m_bTryAnotherTuner = IniFileAccess.ReadKeyBSectionData(L"TryAnotherTuner", FALSE);

	// CH�ؑւɎ��s�����ꍇ�ɁA�ُ팟�m�����l�o�b�N�O�����h��CH�ؑ֓�����s�����ǂ���
	m_bBackgroundChannelLock = IniFileAccess.ReadKeyBSectionData(L"BackgroundChannelLock", FALSE);

	// Tuning Space���i�݊��p�j
	std::wstring sTempTuningSpaceName = IniFileAccess.ReadKeySSectionData(L"TuningSpaceName", L"�X�J�p�[");

	// SignalLevel �Z�o���@
	m_nSignalLevelCalcType = (enumSignalLevelCalcType)IniFileAccess.ReadKeyIValueMapSectionData(L"SignalLevelCalcType", enumSignalLevelCalcType::eSignalLevelCalcTypeSSStrength, mapSignalLevelCalcType);
	if (m_nSignalLevelCalcType >= eSignalLevelCalcTypeSSMin && m_nSignalLevelCalcType <= eSignalLevelCalcTypeSSMax)
		m_bSignalLevelGetTypeSS = TRUE;
	else if (m_nSignalLevelCalcType >= eSignalLevelCalcTypeTunerMin && m_nSignalLevelCalcType <= eSignalLevelCalcTypeTunerMax)
		m_bSignalLevelGetTypeTuner = TRUE;
	else if (m_nSignalLevelCalcType >= eSignalLevelCalcTypeDemodSSMin && m_nSignalLevelCalcType <= eSignalLevelCalcTypeDemodSSMax)
		m_bSignalLevelGetTypeDemodSS = TRUE;
	else if (m_nSignalLevelCalcType == eSignalLevelCalcTypeBR)
		m_bSignalLevelGetTypeBR = TRUE;

	if (m_nSignalLevelCalcType == eSignalLevelCalcTypeSSStrength || m_nSignalLevelCalcType == eSignalLevelCalcTypeTunerStrength || m_nSignalLevelCalcType == eSignalLevelCalcTypeDemodSSStrength) {
		m_bSignalLevelNeedStrength = TRUE;
		m_sSignalLevelCalcFormula = L"S / SC + SB";
	}
	if (m_nSignalLevelCalcType == eSignalLevelCalcTypeSSQuality || m_nSignalLevelCalcType == eSignalLevelCalcTypeTunerQuality || m_nSignalLevelCalcType == eSignalLevelCalcTypeDemodSSQuality) {
		m_bSignalLevelNeedQuality = TRUE;
		m_sSignalLevelCalcFormula = L"Q / QC + QB";
	}
	if (m_nSignalLevelCalcType == eSignalLevelCalcTypeSSMul || m_nSignalLevelCalcType == eSignalLevelCalcTypeTunerMul || m_nSignalLevelCalcType == eSignalLevelCalcTypeDemodSSMul) {
		m_bSignalLevelNeedStrength = TRUE;
		m_bSignalLevelNeedQuality = TRUE;
		m_sSignalLevelCalcFormula = L"(S / SC + SB) * (Q / QC + QB)";
	}
	if (m_nSignalLevelCalcType == eSignalLevelCalcTypeSSAdd || m_nSignalLevelCalcType == eSignalLevelCalcTypeTunerAdd || m_nSignalLevelCalcType == eSignalLevelCalcTypeDemodSSAdd) {
		m_bSignalLevelNeedStrength = TRUE;
		m_bSignalLevelNeedQuality = TRUE;
		m_sSignalLevelCalcFormula = L"(S / SC + SB) + (Q / QC + QB)";
	}
	if (m_nSignalLevelCalcType == eSignalLevelCalcTypeSSFormula || m_nSignalLevelCalcType == eSignalLevelCalcTypeTunerFormula || m_nSignalLevelCalcType == eSignalLevelCalcTypeDemodSSFormula) {
		m_bSignalLevelNeedStrength = TRUE;
		m_bSignalLevelNeedQuality = TRUE;
		m_sSignalLevelCalcFormula = IniFileAccess.ReadKeySSectionData(L"SignalLevelCalcFormula", L"S / SC + SB");
	}

	// Strength �l�␳�W��
	m_fStrengthCoefficient = (double)IniFileAccess.ReadKeyFSectionData(L"StrengthCoefficient", 1.0);
	if (m_fStrengthCoefficient == 0.0)
		m_fStrengthCoefficient = 1.0;

	// Quality �l�␳�W��
	m_fQualityCoefficient = (double)IniFileAccess.ReadKeyFSectionData(L"QualityCoefficient", 1.0);
	if (m_fQualityCoefficient == 0.0)
		m_fQualityCoefficient = 1.0;

	// Strength �l�␳�o�C�A�X
	m_fStrengthBias = (double)IniFileAccess.ReadKeyFSectionData(L"StrengthBias", 0.0);

	// Quality �l�␳�o�C�A�X
	m_fQualityBias = (double)IniFileAccess.ReadKeyFSectionData(L"QualityBias", 0.0);

	// muparser������
	try {
		m_muParser.DefineVar(_T("S"), &m_fStrength);
		m_muParser.DefineVar(_T("SC"), &m_fStrengthCoefficient);
		m_muParser.DefineVar(_T("SB"), &m_fStrengthBias);
		m_muParser.DefineVar(_T("Q"), &m_fQuality);
		m_muParser.DefineVar(_T("QC"), &m_fQualityCoefficient);
		m_muParser.DefineVar(_T("QB"), &m_fQualityBias);
		m_muParser.SetExpr(common::WStringToTString(m_sSignalLevelCalcFormula));
	}
	catch (...) {
		OutputDebug(L"muParser exception. Wrong formula format?\n");
	}

	// �`���[�j���O��Ԃ̔��f���@
	m_nSignalLockedJudgeType = (enumSignalLockedJudgeType)IniFileAccess.ReadKeyIValueMapSectionData(L"SignalLockedJudgeType", enumSignalLockedJudgeType::eSignalLockedJudgeTypeSS, mapSignalLockedJudgeType);
	if (m_nSignalLockedJudgeType == eSignalLockedJudgeTypeSS)
		m_bSignalLockedJudgeTypeSS = TRUE;
	else if (m_nSignalLockedJudgeType == eSignalLockedJudgeTypeTuner)
		m_bSignalLockedJudgeTypeTuner = TRUE;
	else if (m_nSignalLockedJudgeType == eSignalLockedJudgeTypeDemodSS)
		m_bSignalLockedJudgeTypeDemodSS = TRUE;

	for (unsigned int i = 0; i < MAX_DVB_SYSTEM_TYPE; i++) {
		std::wstring key, prefix[2];
		DVBSystemTypeData typeData;
		if (i == 0) {
			// �`���[�i�[�̎g�p����TuningSpace�̎��
			typeData.nDVBSystemType = enumTunerType::eTunerTypeDVBS;
		}

		unsigned int st = i == 0 ? 0 : 1;
		prefix[0] = L"DVBSystemType";
		prefix[1] = L"DVBSystemType" + std::to_wstring(i);
		for (unsigned int j = st; j < 2; j++) {
			// �`���[�i�[�̎g�p����TuningSpace�̎��
			key = prefix[j];
			typeData.nDVBSystemType = (enumTunerType)IniFileAccess.ReadKeyIValueMapSectionData(key, typeData.nDVBSystemType, mapTuningSpaceType);

			// �g�p����ITuningSpace interface
			key = prefix[j] + L"TuningSpace";
			typeData.nTuningSpace = (enumTuningSpace)IniFileAccess.ReadKeyIValueMapSectionData(key, typeData.nTuningSpace, mapSpecifyTuningSpace);

			// �g�p����ILocator interface
			key = prefix[j] + L"Locator";
			typeData.nLocator = (enumLocator)IniFileAccess.ReadKeyIValueMapSectionData(key, typeData.nLocator, mapSpecifyLocator);

			// ITuningSpace�ɐݒ肷��NetworkType
			key = prefix[j] + L"ITuningSpaceNetworkType";
			typeData.nITuningSpaceNetworkType = (enumNetworkType)IniFileAccess.ReadKeyIValueMapSectionData(key, typeData.nITuningSpaceNetworkType, mapSpecifyITuningSpaceNetworkType);

			// IDVBTuningSpace�ɐݒ肷��SystemType
			key = prefix[j] + L"IDVBTuningSpaceSystemType";
			typeData.nIDVBTuningSpaceSystemType = (enumDVBSystemType)IniFileAccess.ReadKeyIValueMapSectionData(key, typeData.nIDVBTuningSpaceSystemType, mapSpecifyIDVBTuningSpaceSystemType);

			// IAnalogTVTuningSpace�ɐݒ肷��InputType
			key = prefix[j] + L"IAnalogTVTuningSpaceInputType";
			typeData.nIAnalogTVTuningSpaceInputType = (enumTunerInputType)IniFileAccess.ReadKeyIValueMapSectionData(key, typeData.nIAnalogTVTuningSpaceInputType, mapSpecifyIAnalogTVTuningSpaceInputType);
		}

		if (typeData.nDVBSystemType == enumTunerType::eTunerTypeNone && (typeData.nTuningSpace == enumTuningSpace::eTuningSpaceAuto || typeData.nLocator == enumLocator::eLocatorAuto)) {
			continue;
		}

		// DB�ɓo�^
		auto it = m_DVBSystemTypeDB.SystemType.find(i);
		if (it == m_DVBSystemTypeDB.SystemType.end()) {
			it = m_DVBSystemTypeDB.SystemType.emplace(i, typeData).first;
			m_DVBSystemTypeDB.nNumType++;
		}
	}

	// �`���[�i�[�Ɏg�p����NetworkProvider
	m_nNetworkProvider = (enumNetworkProvider)IniFileAccess.ReadKeyIValueMapSectionData(L"NetworkProvider", enumNetworkProvider::eNetworkProviderAuto, mapNetworkProvider);

	// �q����M�p�����[�^/�ϒ������p�����[�^�̃f�t�H���g�l
	m_nDefaultNetwork = (enumDefaultNetwork)IniFileAccess.ReadKeyIValueMapSectionData(L"DefaultNetwork", enumDefaultNetwork::eDefaultNetworkSPHD, mapDefaultNetwork);

	//
	// BonDriver �Z�N�V����
	//
	IniFileAccess.ReadSection(L"BONDRIVER");
	IniFileAccess.CreateSectionData();

	// �X�g���[���f�[�^�o�b�t�@1���̃T�C�Y
	// 188�~�ݒ萔(bytes)
	m_nBuffSize = 188 * (size_t)IniFileAccess.ReadKeyISectionData(L"BuffSize", 1024);

	// �X�g���[���f�[�^�o�b�t�@�̍ő��
	m_nMaxBuffCount = (size_t)IniFileAccess.ReadKeyISectionData(L"MaxBuffCount", 512);

	// WaitTsStream���A�w�肳�ꂽ�����̃X�g���[���f�[�^�o�b�t�@�����܂�܂őҋ@����
	// �`���[�i��CPU���ׂ������Ƃ��͐��l��傫�ڂɂ���ƌ��ʂ�����ꍇ������
	m_nWaitTsCount = IniFileAccess.ReadKeyISectionData(L"WaitTsCount", 1);
	if (m_nWaitTsCount < 1)
		m_nWaitTsCount = 1;

	// WaitTsStream���X�g���[���f�[�^�o�b�t�@�����܂��Ă��Ȃ��ꍇ�ɍŒ���ҋ@���鎞��(msec)
	// �`���[�i��CPU���ׂ������Ƃ���100msec���x���w�肷��ƌ��ʂ�����ꍇ������
	m_nWaitTsSleep = IniFileAccess.ReadKeyISectionData(L"WaitTsSleep", 100);

	// �k���p�P�b�g���폜���邩�ǂ���
	m_bDeleteNullPackets = IniFileAccess.ReadKeyBSectionData(L"DeleteNullPackets", FALSE);

	// SetChannel()�Ń`�����l�����b�N�Ɏ��s�����ꍇ�ł�FALSE��Ԃ��Ȃ��悤�ɂ��邩�ǂ���
	m_bAlwaysAnswerLocked = IniFileAccess.ReadKeyBSectionData(L"AlwaysAnswerLocked", FALSE);

	// COMProcThread�̃X���b�h�v���C�I���e�B
	m_nThreadPriorityCOM = IniFileAccess.ReadKeyIValueMapSectionData(L"ThreadPriorityCOM", THREAD_PRIORITY_ERROR_RETURN, mapThreadPriority);

	// DecodeProcThread�̃X���b�h�v���C�I���e�B
	m_nThreadPriorityDecode = IniFileAccess.ReadKeyIValueMapSectionData(L"ThreadPriorityDecode", THREAD_PRIORITY_ERROR_RETURN, mapThreadPriority);

	// �X�g���[���X���b�h�v���C�I���e�B
	m_nThreadPriorityStream = IniFileAccess.ReadKeyIValueMapSectionData(L"ThreadPriorityStream", THREAD_PRIORITY_ERROR_RETURN, mapThreadPriority);

	// timeBeginPeriod()�Őݒ肷��Windows�̍ŏ��^�C�}����\(msec)
	m_nPeriodicTimer = IniFileAccess.ReadKeyISectionData(L"PeriodicTimer", 0);

	//
	// Satellite �Z�N�V����
	//
	IniFileAccess.ReadSection(L"SATELLITE");
	IniFileAccess.CreateSectionData();

	// �q���ʎ�M�p�����[�^
	std::wstring sateliteSettingsAuto[MAX_SATELLITE];

	// Satellite0�`�͉q���ݒ薳���p�iini�t�@�C������̓Ǎ��͍s��Ȃ��j
	m_sSatelliteName[0] = L"not set";						// �`�����l���������p�q������
	// ���̈ȊO�̓R���X�g���N�^�̃f�t�H���g�l�g�p

	// DefaultNetwork�ɂ��f�t�H���g�l�ݒ�
	switch (m_nDefaultNetwork) {
	case eDefaultNetworkSPHD:
		// SPHD
		sateliteSettingsAuto[1] = L"JCSAT-3";
		sateliteSettingsAuto[2] = L"JCSAT-4";
		break;

	case eDefaultNetworkBSCS:
	case eDefaultNetworkDual:
		sateliteSettingsAuto[1] = L"BS/CS110";
		break;
	}

	// BS/CS110��CH�ݒ莩���������Ɏg�p����q���ݒ�ԍ�
	unsigned int satelliteNumberBSCS110R = 1;

	// SPHD��CH�ݒ莩���������Ɏg�p����q���ݒ�ԍ�
	unsigned int satelliteNumberJCSAT3 = 1;
	unsigned int satelliteNumberJCSAT4 = 2;

	// �q���ݒ�1�`9�̐ݒ��Ǎ�
	for (unsigned int satellite = 1; satellite < MAX_SATELLITE; satellite++) {
		std::wstring key, prefix1, prefix2;
		prefix1 = L"Satellite" + std::to_wstring(satellite);

		// �q���ݒ莩������
		key = prefix1 + L"SettingsAuto";
		sateliteSettingsAuto[satellite] = common::WStringToUpperCase(IniFileAccess.ReadKeySSectionData(key, sateliteSettingsAuto[satellite]));

		if (sateliteSettingsAuto[satellite] == L"JCSAT-3") {
			// JCSAT-3A
			satelliteNumberJCSAT3 = satellite;
			m_sSatelliteName[satellite] = L"128.0E";																					// �`�����l���������p�q������
			m_aSatellite[satellite].Polarisation[1].HighOscillator = m_aSatellite[satellite].Polarisation[1].LowOscillator = 11200000;	// �����Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[1].Tone = 0;																			// �����Δg���g�[���M��
			m_aSatellite[satellite].Polarisation[2].HighOscillator = m_aSatellite[satellite].Polarisation[2].LowOscillator = 11200000;	// �����Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[2].Tone = 0;																			// �����Δg���g�[���M��
		}
		else if (sateliteSettingsAuto[satellite] == L"JCSAT-4") {
			// JCSAT-4B
			satelliteNumberJCSAT4 = satellite;
			m_sSatelliteName[satellite] = L"124.0E";																					// �`�����l���������p�q������
			m_aSatellite[satellite].Polarisation[1].HighOscillator = m_aSatellite[satellite].Polarisation[1].LowOscillator = 11200000;	// �����Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[1].Tone = 1;																			// �����Δg���g�[���M��
			m_aSatellite[satellite].Polarisation[2].HighOscillator = m_aSatellite[satellite].Polarisation[2].LowOscillator = 11200000;	// �����Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[2].Tone = 1;																			// �����Δg���g�[���M��
		}
		else if (sateliteSettingsAuto[satellite] == L"BS/CS110") {
			// BS/CS110 �E���~�Δg�ƍ����~�Δg
			satelliteNumberBSCS110R = satellite;
			m_sSatelliteName[satellite] = L"BS/CS110";																					// �`�����l���������p�q������
			m_aSatellite[satellite].Polarisation[3].HighOscillator = m_aSatellite[satellite].Polarisation[3].LowOscillator = 9505000;	// �����~�Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[3].Tone = 0;																			// �����~�Δg�Δg���g�[���M��
			m_aSatellite[satellite].Polarisation[4].HighOscillator = m_aSatellite[satellite].Polarisation[4].LowOscillator = 10678000;	// �E���~�Δg��LNB���g��
			m_aSatellite[satellite].Polarisation[4].Tone = 0;																			// �E���~�Δg�Δg���g�[���M��
		}

		// �T�[�r�X�\���p�q������
		key = prefix1 + L"Name";
		m_sSatelliteName[satellite] = IniFileAccess.ReadKeySSectionData(key, m_sSatelliteName[satellite]);

		// �Δg���1�`4�̃A���e�i�ݒ��Ǎ�
		for (unsigned int polarisation = 1; polarisation < POLARISATION_SIZE; polarisation++) {
			prefix2 = prefix1 + PolarisationChar[polarisation];
			// �ǔ����g�� (KHz)
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			key = prefix1 + L"Oscillator";
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator = m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator);
			key = prefix1 + L"HighOscillator";
			m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator);
			key = prefix1 + L"LowOscillator";
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key, m_aSatellite[satellite].Polarisation[polarisation].LowOscillator);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			key = prefix2 + L"Oscillator";
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator = m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator);
			key = prefix2 + L"HighOscillator";
			m_aSatellite[satellite].Polarisation[polarisation].HighOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key, m_aSatellite[satellite].Polarisation[polarisation].HighOscillator);
			key = prefix2 + L"LowOscillator";
			m_aSatellite[satellite].Polarisation[polarisation].LowOscillator
				= (long)IniFileAccess.ReadKeyISectionData(key, m_aSatellite[satellite].Polarisation[polarisation].LowOscillator);

			// LNB�ؑ֎��g�� (KHz)
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			key = prefix1 + L"LNBSwitch";
			m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch
				= (long)IniFileAccess.ReadKeyISectionData(key, m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			key = prefix2 + L"LNBSwitch";
			m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch
				= (long)IniFileAccess.ReadKeyISectionData(key, m_aSatellite[satellite].Polarisation[polarisation].LNBSwitch);

			// �g�[���M�� (0 or 1)
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			key = prefix1 + L"ToneSignal";
			m_aSatellite[satellite].Polarisation[polarisation].Tone
				= (long)IniFileAccess.ReadKeyBSectionData(key, m_aSatellite[satellite].Polarisation[polarisation].Tone);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			key = prefix2 + L"ToneSignal";
			m_aSatellite[satellite].Polarisation[polarisation].Tone
				= (long)IniFileAccess.ReadKeyBSectionData(key, m_aSatellite[satellite].Polarisation[polarisation].Tone);

			// DiSEqC
			// �S�Δg���ʂł̐ݒ肪����Γǂݍ���
			key = prefix1 + L"DiSEqC";
			m_aSatellite[satellite].Polarisation[polarisation].DiSEqC
				= (long)IniFileAccess.ReadKeyIValueMapSectionData(key, m_aSatellite[satellite].Polarisation[polarisation].DiSEqC, mapDiSEqC);
			// �ʐݒ肪����Ώ㏑���œǂݍ���
			key = prefix2 + L"DiSEqC";
			m_aSatellite[satellite].Polarisation[polarisation].DiSEqC
				= (long)IniFileAccess.ReadKeyIValueMapSectionData(key, m_aSatellite[satellite].Polarisation[polarisation].DiSEqC, mapDiSEqC);
		}
	}

	//
	// Modulation �Z�N�V����
	//
	IniFileAccess.ReadSection(L"MODULATION");
	IniFileAccess.CreateSectionData();

	// �ϒ������ʃp�����[�^�i0�`9�̏��Ȃ̂Œ��Ӂj
	std::wstring modulationSettingsAuto[MAX_MODULATION];

	// DefaultNetwork�ɂ��f�t�H���g�l�ݒ�
	switch (m_nDefaultNetwork) {
	case eDefaultNetworkSPHD:
		//SPHD
		modulationSettingsAuto[0] = L"DVB-S";
		modulationSettingsAuto[1] = L"DVB-S2";
		break;

	case eDefaultNetworkBSCS:
		// BS/CS110
		modulationSettingsAuto[0] = L"ISDB-S";
		break;

	case eDefaultNetworkUHF:
		// UHF/CATV
		modulationSettingsAuto[0] = L"ISDB-T";
		break;

	case eDefaultNetworkDual:
		modulationSettingsAuto[0] = L"ISDB-T";
		modulationSettingsAuto[1] = L"ISDB-S";
		break;
	}

	// UHF / CATV��CH�ݒ莩���������Ɏg�p����ϒ������ԍ�
	unsigned int modulationNumberISDBT = 0;

	// BS/CS110��CH�ݒ莩���������Ɏg�p����ϒ������ԍ�
	unsigned int modulationNumberISDBS = 0;

	// SPHD��CH�ݒ莩���������Ɏg�p����ϒ������ԍ�
	unsigned int modulationNumberDVBS = 0;
	unsigned int modulationNumberDVBS2 = 1;

	// ISDB-C�g�����X���W�����[�V������CH�ݒ莩���������Ɏg�p����ϒ������ԍ�
	unsigned int modulationNumberJ83C64QAM = 0;
	unsigned int modulationNumberJ83C256QAM = 1;

	// �X�J�p�[!�v���~�A���T�[�r�X����CH�ݒ莩���������Ɏg�p����ϒ������ԍ�
	unsigned int modulationNumberOpticast = 0;

	// �ϒ������ݒ�0�`9�̒l��Ǎ�
	for (unsigned int modulation = 0; modulation < MAX_MODULATION; modulation++) {
		std::wstring key, prefix;
		prefix = L"ModulationType" + std::to_wstring(modulation);

		// �ϒ������ݒ莩������
		key = prefix + L"SettingsAuto";
		modulationSettingsAuto[modulation] = common::WStringToUpperCase(IniFileAccess.ReadKeySSectionData(key, modulationSettingsAuto[modulation]));

		if (modulationSettingsAuto[modulation] == L"DVB-S") {
			// SPHD DVB-S
			modulationNumberDVBS = modulation;
			m_sModulationName[modulation] = L"DVB-S";							// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_QPSK;			// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_VITERBI;			// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_3_4;		// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_RS_204_188;		// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = 21096;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"DVB-S2") {
			// SPHD DVB-S2
			modulationNumberDVBS2 = modulation;
			m_sModulationName[modulation] = L"DVB-S2";							// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_NBC_8PSK;		// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_LDPC;				// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_3_5;		// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_BCH;				// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = 23303;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"ISDB-S") {
			// BS/CS110
			modulationNumberISDBS = modulation;
			m_sModulationName[modulation] = L"ISDB-S";							// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_ISDB_S_TMCC;		// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_VITERBI;			// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_2_3;		// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_RS_204_188;		// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = 28860;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"ISDB-T") {
			// UHF/CATV
			modulationNumberISDBT = modulation;
			m_sModulationName[modulation] = L"ISDB-T";							// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_ISDB_T_TMCC;		// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_VITERBI;			// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_3_4;		// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_RS_204_188;		// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = -1;						// �V���{�����[�g
			m_aModulationType[modulation].BandWidth = 6;						// �ш敝(MHz)
		}
		else if (modulationSettingsAuto[modulation] == L"J.83C-64QAM") {
			// ISDB-C�g�����X���W�����[�V���� 64QAM
			modulationNumberJ83C64QAM = modulation;
			m_sModulationName[modulation] = L"J.83C-64QAM";						// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_64QAM;			// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_METHOD_NOT_SET;	// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_NOT_SET;	// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_RS_204_188;		// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = 5274;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"J.83C-256QAM") {
			// ISDB-C�g�����X���W�����[�V���� 256QAM
			modulationNumberJ83C256QAM = modulation;
			m_sModulationName[modulation] = L"J.83C-256QAM";					// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_256QAM;			// �ϒ��^�C�v
			m_aModulationType[modulation].InnerFEC = BDA_FEC_METHOD_NOT_SET;	// �����O���������^�C�v
			m_aModulationType[modulation].InnerFECRate = BDA_BCC_RATE_NOT_SET;	// ����FEC���[�g
			m_aModulationType[modulation].OuterFEC = BDA_FEC_RS_204_188;		// �O���O���������^�C�v
			m_aModulationType[modulation].OuterFECRate = BDA_BCC_RATE_NOT_SET;	// �O��FEC���[�g
			m_aModulationType[modulation].SymbolRate = 5274;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"J.83B-OPTICAST") {
			// �X�J�p�[!�v���~�A���T�[�r�X�� 256QAM
			modulationNumberOpticast = modulation;
			m_sModulationName[modulation] = L"J.83B-Opticast";					// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_256QAM;			// �ϒ��^�C�v
			m_aModulationType[modulation].SymbolRate = 5600;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"J.83B-64QAM") {
			// J.83 Annex-B 64QAM
			m_sModulationName[modulation] = L"J.83B-64QAM";						// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_64QAM;			// �ϒ��^�C�v
			m_aModulationType[modulation].SymbolRate = 5057;					// �V���{�����[�g
		}
		else if (modulationSettingsAuto[modulation] == L"J.83B-256QAM") {
			// J.83 Annex-B 256QAM
			m_sModulationName[modulation] = L"J.83B-256QAM";					// �`�����l���������p�ϒ���������
			m_aModulationType[modulation].Modulation = BDA_MOD_256QAM;			// �ϒ��^�C�v
			m_aModulationType[modulation].SymbolRate = 5361;					// �V���{�����[�g
		}

		// �`�����l���������p�ϒ���������
		key = prefix + L"Name";
		m_sModulationName[modulation] = IniFileAccess.ReadKeySSectionData(key, m_sModulationName[modulation]);

		// �ϒ��^�C�v
		key = prefix + L"Modulation";
		m_aModulationType[modulation].Modulation
			= (ModulationType)IniFileAccess.ReadKeyIValueMapSectionData(key, m_aModulationType[modulation].Modulation, mapModulationType);

		// �����O���������^�C�v
		key = prefix + L"InnerFEC";
		m_aModulationType[modulation].InnerFEC
			= (FECMethod)IniFileAccess.ReadKeyIValueMapSectionData(key, m_aModulationType[modulation].InnerFEC, mapFECMethod);

		// ����FEC���[�g
		key = prefix + L"InnerFECRate";
		m_aModulationType[modulation].InnerFECRate
			= (BinaryConvolutionCodeRate)IniFileAccess.ReadKeyIValueMapSectionData(key, m_aModulationType[modulation].InnerFECRate, mapBinaryConvolutionCodeRate);

		// �O���O���������^�C�v
		key = prefix + L"OuterFEC";
		m_aModulationType[modulation].OuterFEC
			= (FECMethod)IniFileAccess.ReadKeyIValueMapSectionData(key, m_aModulationType[modulation].OuterFEC, mapFECMethod);

		// �O��FEC���[�g
		key = prefix + L"OuterFECRate";
		m_aModulationType[modulation].OuterFECRate
			= (BinaryConvolutionCodeRate)IniFileAccess.ReadKeyIValueMapSectionData(key, m_aModulationType[modulation].OuterFECRate, mapBinaryConvolutionCodeRate);

		// �V���{�����[�g
		key = prefix + L"SymbolRate";
		m_aModulationType[modulation].SymbolRate
			= (long)IniFileAccess.ReadKeyISectionData(key, m_aModulationType[modulation].SymbolRate);

		// �ш敝(MHz)
		key = prefix + L"BandWidth";
		m_aModulationType[modulation].BandWidth
			= (long)IniFileAccess.ReadKeyISectionData(key, m_aModulationType[modulation].BandWidth);
	}

	//
	// Channel �Z�N�V����
	//

	// ini�t�@�C������CH�ݒ��Ǎ��ލۂɎg�p����Ă��Ȃ�CH�ԍ��������Ă��O�l�����m�ۂ��Ă������ǂ���
	// [Channel]�Z�N�V�����ł̒�` ... �S�Ẵ`���[�j���O��Ԃɉe��
	BOOL bReserveUnusedChGlobal = IniFileAccess.ReadKeyB(L"CHANNEL", L"ReserveUnusedCh", FALSE);

	// �`���[�j���O���00�`99�̐ݒ��Ǎ�
	for (DWORD space = 0; space < 100; space++)	{
		std::wstring section = common::WStringPrintf(L"TUNINGSPACE%02d", space);
		if (IniFileAccess.ReadSection(section) <= 0) {
			// TuningSpaceXX�̃Z�N�V���������݂��Ȃ��ꍇ
			if (space != 0)
				continue;
			// TuningSpace00�̎���Channel�Z�N�V����������
			IniFileAccess.ReadSection(L"CHANNEL");
		}
		IniFileAccess.CreateSectionData();

		// ini�t�@�C������CH�ݒ��Ǎ��ލۂɎg�p����Ă��Ȃ�CH�ԍ��������Ă��O�l�����m�ۂ��Ă������ǂ���
		// [TuningSpaceXX]�Z�N�V�����ł̒�` ... ���݂̃`���[�j���O��Ԃɉe��
		BOOL bReserveUnusedCh = IniFileAccess.ReadKeyBSectionData(L"ReserveUnusedCh", bReserveUnusedChGlobal);

		// ���Ƀ`���[�j���O��ԃf�[�^�����݂���ꍇ�͂��̓��e������������
		// �����ꍇ�͋�̃`���[�j���O��Ԃ��쐬
		auto itSpace = m_TuningData.Spaces.find(space);
		if (itSpace == m_TuningData.Spaces.end()) {
			itSpace = m_TuningData.Spaces.emplace(space, TuningSpaceData()).first;
		}

		// Tuning Space��
		std::wstring temp;
		if (space == 0)
			temp = sTempTuningSpaceName;
		else
			temp = L"NoName";
		
		itSpace->second.sTuningSpaceName = common::WStringToTString(IniFileAccess.ReadKeySSectionData(L"TuningSpaceName", temp));

		// ini�t�@�C����1�̃`���[�j���O��ԂŒ�`�ł���ő�ChannelGenerate�̐�
		static constexpr unsigned int MAX_CH_GENERATE = 100;

		// �`�����l�����������^�C�v
		enum enumChGenerate {
			eChGenerateNone = 0,
			eChGenerateVHF_L = 1,				// VHF 1ch�`3ch
			eChGenerateVHF_H = 2,				// VHF 4ch�`12ch
			eChGenerateUHF = 3,					// UHF 13ch�`62ch
			eChGenerateCATV_L = 4,				// CATV C13ch�`C22ch
			eChGenerateCATV_H = 5,				// CATV C23ch�`C63ch
			eChGenerateVHF_4Plus = 8,			// VHF 4ch+
			eChGenerateCATV_22Plus = 9,			// CATV C22ch+
			eChGenerateCATV_24Plus = 10,		// CATV C24ch+�`C27ch+
			eChGenerateBS1 = 16,				// BS-R BS1,BS3,BS5...�`BS23
			eChGenerateND2 = 20,				// CS110-R ND2,ND4,ND6...�`ND24
			eChGenerateJD1 = 32,				// JCSAT-3A/JCSAT-4B JD1�`JD16
			eChGenerateJD17A = 33,				// JCSAT-3A JD17�`JD28
			eChGenerateJD17B = 34,				// JCSAT-4B JD17�`JD32
			eChGenerateOpticast = 64,			// SKY PerfecTV! Premium Service Hikari H001�`H058
			eChGenerateOpticast_11Plus = 72,	// SKY PerfecTV! Premium Service Hikari H011+�`H012+
			eChGenerateOpticast_26Plus = 73,	// SKY PerfecTV! Premium Service Hikari H026+�`H028+
		};

		// �`�����l�����������p�p�����[�^
		struct ChGenerate {
			enumChGenerate Space;
			unsigned int Offset;			// �J�n�I�t�Z�b�g
			unsigned int Count;				// �쐬CH��
			unsigned int RelativeTS;		// ����TS�쐬��
			unsigned int ModulationNumber;	// �ϒ������ԍ�
			unsigned int SatelliteNumber;	// �q���ԍ�
			unsigned int StartCh;			// �擪�`�����l���ԍ�
			unsigned int TuningFreq;		// �`���[�j���O���g���I�t�Z�b�g
			std::wstring NameFormat;		// �`�����l�����t�H�[�}�b�g
			unsigned int NameOffset;		// �`�����l�����Ɏg�p����`�����l���ԍ��I�t�Z�b�g
			unsigned int NameStep;			// �`�����l�����Ɏg�p����`�����l���ԍ��X�e�b�v
			unsigned int NameOffsetTS;		// �`�����l�����Ɏg�p����TS�ԍ��I�t�Z�b�g
			ChGenerate(void)
				: Space(eChGenerateNone),
				Offset(0),
				Count(0),
				RelativeTS(0),
				ModulationNumber(0),
				SatelliteNumber(0),
				StartCh(0),
				TuningFreq(0),
				NameOffset(0),
				NameStep(1),
				NameOffsetTS(0)
			{
			};
			~ChGenerate(void)
			{
			};
		};

		// �`�����l���̎���������`�s
		std::wstring ChannelGenerate[MAX_CH_GENERATE];

		// UHF/CATV����CH�ݒ��������������
		std::wstring chAuto = common::WStringToUpperCase(IniFileAccess.ReadKeySSectionData(L"ChannelSettingsAuto", L""));
		std::wstring chAutoOpt = common::WStringToUpperCase(IniFileAccess.ReadKeySSectionData(L"ChannelSettingsAutoOptions", L""));
		BOOL bOptVHFPlus = FALSE;
		BOOL bOptC24Plus = FALSE;
		BOOL bOptOnly64QAM = FALSE;
		BOOL bOptOnly256QAM = FALSE;
		BOOL bOptH11Plus = FALSE;
		BOOL bOptH26Plus = FALSE;
		BOOL bOptOnlySD = FALSE;
		BOOL bOptOnlyHD = FALSE;
		BOOL bOptSpinel = FALSE;
		BOOL bOptAll = FALSE;
		std::wstring sOptRelativeTS = L"";
		std::wstring sOptRelativeTS64QAM = L"";
		{
			// �J���}��؂��7�ɕ���
			std::wstring t(chAutoOpt);
			for (int n = 0; n < 10; n++) {
				std::wstring opt;
				std::wstring::size_type pos = common::WStringSplit(&t, L',', &opt);
				if (opt == L"VHF+") {
					bOptVHFPlus = TRUE;
				}
				else if (opt == L"C24+") {
					bOptC24Plus = TRUE;
				}
				else if (opt == L"ONLY64QAM") {
					bOptOnly64QAM = TRUE;
				}
				else if (opt == L"ONLY256QAM") {
					bOptOnly256QAM = TRUE;
				}
				else if (opt == L"H011+") {
					bOptH11Plus = TRUE;
				}
				else if (opt == L"H026+") {
					bOptH26Plus = TRUE;
				}
				else if (opt == L"ONLYSD") {
					bOptOnlySD = TRUE;
				}
				else if (opt == L"ONLYHD") {
					bOptOnlyHD = TRUE;
				}
				else if (opt == L"SPINEL") {
					bOptSpinel = TRUE;
				}
				else if (opt == L"ALL") {
					bOptAll = TRUE;
				}
				else if (opt.substr(0, 11) == L"RELATIVETS:") {
					sOptRelativeTS = opt.substr(11);
				}
				else if (opt.substr(0, 18) == L"RELATIVETS-256QAM:") {
					sOptRelativeTS = opt.substr(18);
				}
				else if (opt.substr(0, 17) == L"RELATIVETS-64QAM:") {
					sOptRelativeTS64QAM = opt.substr(17);
				}
				if (pos == std::wstring::npos)
					break;
			}

		}
		if (chAuto == L"UHF") {
			if (bOptAll) {
				ChannelGenerate[0] = L"UHF," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[0] = L"UHF:0:40," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
		}
		else if (chAuto == L"CATV") {
			int num = 0;
			if (bOptAll || !bOptVHFPlus) {
				ChannelGenerate[num++] = L"CATV-L," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[num++] = L"CATV-L:0:9," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-22+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			if (bOptAll || !bOptC24Plus) {
				ChannelGenerate[num++] = L"CATV-H," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[num++] = L"CATV-H:0:1," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-24+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-H:5," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			if (bOptAll) {
				ChannelGenerate[num++] = L"CATV-22+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-24+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
		}
		else if (chAuto == L"PASSTHROUGH") {
			int num = 0;
			ChannelGenerate[num++] = L"VHF-L," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			if (bOptAll || !bOptVHFPlus) {
				ChannelGenerate[num++] = L"VHF-H," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[num++] = L"VHF-4+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"VHF-H:4," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			ChannelGenerate[num++] = L"UHF," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			if (bOptAll || !bOptVHFPlus) {
				ChannelGenerate[num++] = L"CATV-L," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[num++] = L"CATV-L:0:9," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-22+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			if (bOptAll || !bOptC24Plus) {
				ChannelGenerate[num++] = L"CATV-H," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			else {
				ChannelGenerate[num++] = L"CATV-H:0:1," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-24+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-H:5," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
			if (bOptAll) {
				ChannelGenerate[num++] = L"VHF-4+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-22+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
				ChannelGenerate[num++] = L"CATV-24+," + std::to_wstring(modulationNumberISDBT) + L",,,143";
			}
		}
		else if (chAuto == L"TRANSMODULATION") {
			std::wstring sRelative64;
			std::wstring sRelative256;
			if (sOptRelativeTS64QAM != L"") {
				sRelative64 = L":" + sOptRelativeTS64QAM;
			}
			if (sOptRelativeTS != L"") {
				sRelative256 = L":" + sOptRelativeTS;
			}
			int num = 0;
			unsigned int startCh = 0;
			if (!bOptOnly256QAM) {
				ChannelGenerate[num++] = L"VHF-L::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",," + std::to_wstring(startCh) + L",0,,,,1";
				if (bOptAll || !bOptVHFPlus) {
					ChannelGenerate[num++] = L"VHF-H::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"VHF-4+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"VHF-H:4:" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				ChannelGenerate[num++] = L"UHF::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				if (bOptAll || !bOptVHFPlus) {
					ChannelGenerate[num++] = L"CATV-L::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"CATV-L:0:9" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-22+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				if (bOptAll || !bOptC24Plus) {
					ChannelGenerate[num++] = L"CATV-H::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"CATV-H:0:1" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-24+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-H:5:" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
				}
				startCh += max(common::WStringToLong(sOptRelativeTS64QAM), 1) * (12 + 50 + 51);
				if (bOptAll) {
					ChannelGenerate[num++] = L"VHF-4+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-22+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-24+::" + sRelative64 + L"," + std::to_wstring(modulationNumberJ83C64QAM) + L",,,0,,,,1";
					startCh += max(common::WStringToLong(sOptRelativeTS64QAM), 1) * (3 + 1 + 4);
				}
				startCh = ((startCh + 99) / 100) * 100;
			}
			if (!bOptOnly64QAM) {
				ChannelGenerate[num++] = L"VHF-L::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",," + std::to_wstring(startCh) + L",0,,,,1";
				if (bOptAll || !bOptVHFPlus) {
					ChannelGenerate[num++] = L"VHF-H::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"VHF-4+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"VHF-H:4:" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				ChannelGenerate[num++] = L"UHF::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				if (bOptAll || !bOptVHFPlus) {
					ChannelGenerate[num++] = L"CATV-L::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"CATV-L:0:9" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-22+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				if (bOptAll || !bOptC24Plus) {
					ChannelGenerate[num++] = L"CATV-H::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				else {
					ChannelGenerate[num++] = L"CATV-H:0:1" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-24+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-H:5:" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
				if (bOptAll) {
					ChannelGenerate[num++] = L"VHF-4+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-22+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
					ChannelGenerate[num++] = L"CATV-24+::" + sRelative256 + L"," + std::to_wstring(modulationNumberJ83C256QAM) + L",,,0,,,,1";
				}
			}
		}
		else if (chAuto == L"BS") {
			std::wstring sRelative;
			if (sOptRelativeTS != L"") {
				sRelative = L":" + sOptRelativeTS;
			}
			int num = 0;
			ChannelGenerate[num++] = L"BS1::" + sRelative + L"," + std::to_wstring(modulationNumberISDBS) + L"," + std::to_wstring(satelliteNumberBSCS110R) + L",,,BS%02d/TS%d,1,2,0";
		}
		else if (chAuto == L"CS110") {
			int num = 0;
			ChannelGenerate[num++] = L"ND2," + std::to_wstring(modulationNumberISDBS) + L"," + std::to_wstring(satelliteNumberBSCS110R) + L",,,ND%02d/TS0,2,2";
		}
		else if (chAuto == L"SPHD") {
		std::wstring sName4 = bOptSpinel ? L"JCSAT4A-TP%02d" : L"JCSAT4B-TP%02d";
		int num = 0;
			unsigned int startCh = 1;
			if (!bOptOnlyHD) {
				ChannelGenerate[num++] = L"JD17A," + std::to_wstring(modulationNumberDVBS) + L"," + std::to_wstring(satelliteNumberJCSAT3) + L"," + std::to_wstring(startCh) + L",,JCSAT3A-TP%02d,1";
				ChannelGenerate[num++] = L"JD1," + std::to_wstring(modulationNumberDVBS) + L"," + std::to_wstring(satelliteNumberJCSAT3) + L",,,JCSAT3A-TP%02d,13";
				startCh += 100;
				ChannelGenerate[num++] = L"JD17B," + std::to_wstring(modulationNumberDVBS) + L"," + std::to_wstring(satelliteNumberJCSAT4) + L"," + std::to_wstring(startCh) + L",," + sName4 + L",1";
				ChannelGenerate[num++] = L"JD1," + std::to_wstring(modulationNumberDVBS) + L"," + std::to_wstring(satelliteNumberJCSAT4) + L",,," + sName4 + L",17";
				startCh += 100;
			}
			if (!bOptOnlySD) {
				ChannelGenerate[num++] = L"JD17A," + std::to_wstring(modulationNumberDVBS2) + L"," + std::to_wstring(satelliteNumberJCSAT3) + L"," + std::to_wstring(startCh) + L",,JCSAT3A-TP%02d,1";
				ChannelGenerate[num++] = L"JD1," + std::to_wstring(modulationNumberDVBS2) + L"," + std::to_wstring(satelliteNumberJCSAT3) + L",,,JCSAT3A-TP%02d,13";
				startCh += 100;
				ChannelGenerate[num++] = L"JD17B," + std::to_wstring(modulationNumberDVBS2) + L"," + std::to_wstring(satelliteNumberJCSAT4) + L"," + std::to_wstring(startCh) + L",," + sName4 + L",1";
				ChannelGenerate[num++] = L"JD1," + std::to_wstring(modulationNumberDVBS2) + L"," + std::to_wstring(satelliteNumberJCSAT4) + L",,," + sName4 + L",17";
			}
		}
		else if (chAuto == L"OPTICAST") {
			int num = 0;
			if (bOptAll || (!bOptH11Plus && !bOptH26Plus)) {
				ChannelGenerate[num++] = L"OPTICAST," + std::to_wstring(modulationNumberOpticast) + L",,,0";
			}
			else if (bOptH11Plus && !bOptH26Plus) {
				ChannelGenerate[num++] = L"OPTICAST:0:10," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST-11+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST:12," + std::to_wstring(modulationNumberOpticast) + L",,,0";
			}
			else if (!bOptH11Plus && bOptH26Plus) {
				ChannelGenerate[num++] = L"OPTICAST:0:25," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST-26+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST:28," + std::to_wstring(modulationNumberOpticast) + L",,,0";
			} else {
				ChannelGenerate[num++] = L"OPTICAST:0:10," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST-11+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST:12:13," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST-26+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST:28," + std::to_wstring(modulationNumberOpticast) + L",,,0";
			}
			if (bOptAll) {
				ChannelGenerate[num++] = L"OPTICAST-11+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
				ChannelGenerate[num++] = L"OPTICAST-26+," + std::to_wstring(modulationNumberOpticast) + L",,,0";
			}
		}

		unsigned int nextCh = 0;	// �����擪�`�����l���ԍ�

		// ini�t�@�C������̓Ǎ�
		for (int i = 0; i < MAX_CH_GENERATE; i++) {
			ChGenerate Generate;
			std::wstring key = L"ChannelGenerate" + std::to_wstring(i);
			ChannelGenerate[i] = IniFileAccess.ReadKeySSectionData(key, ChannelGenerate[i]);
			if (ChannelGenerate[i].length() == 0)
				break;
			// �J���}��؂��9�ɕ���
			std::wstring t(ChannelGenerate[i]);
			std::wstring buf[9];
			for (int n = 0; n < 9; n++) {
				if (std::wstring::npos == common::WStringSplit(&t, L',', &buf[n]))
					break;
			}
			// �쐬���:�J�n�I�t�Z�b�g:�쐬CH��:����TS�쐬�� �𕪉�
			std::wstring t2(buf[0]);
			std::wstring buf2[4];
			for (int n = 0; n < 4; n++) {
				if (std::wstring::npos == common::WStringSplit(&t2, L':', &buf2[n]))
					break;
			}
			std::wstring genSpace = common::WStringToUpperCase(buf2[0]);
			unsigned int offset = common::WStringToLong(buf2[1]);
			unsigned int count = common::WStringToLong(buf2[2]);
			if (count < 1) {
				count = 999;
			}
			unsigned int relativeTS = 0;
			std::wstring postfix = L"/TS%d";
			if (genSpace == L"VHF-L") {
				Generate.Space = eChGenerateVHF_L;
				Generate.Offset = min(offset, 3UL - 1UL);
				Generate.Count = min(count, 3UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"%dch";
				Generate.NameOffset = 1UL;
			}
			else if (genSpace == L"VHF-H") {
				Generate.Space = eChGenerateVHF_H;
				Generate.Offset = min(offset, 9UL - 1UL);
				Generate.Count = min(count, 9UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"%dch";
				Generate.NameOffset = 4UL;
			}
			else if (genSpace == L"VHF-4+") {
				Generate.Space = eChGenerateVHF_4Plus;
				Generate.Offset = min(offset, 4UL - 1UL);
				Generate.Count = min(count, 4UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"%dch+";
				Generate.NameOffset = 4UL;
			}
			else if (genSpace == L"UHF") {
				Generate.Space = eChGenerateUHF;
				Generate.Offset = min(offset, 50UL - 1UL);
				Generate.Count = min(count, 50UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"%dch";
				Generate.NameOffset = 13UL;
			}
			else if (genSpace == L"CATV-L") {
				Generate.Space = eChGenerateCATV_L;
				Generate.Offset = min(offset, 10UL - 1UL);
				Generate.Count = min(count, 10UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"C%dch";
				Generate.NameOffset = 13UL;
			}
			else if (genSpace == L"CATV-22+") {
				Generate.Space = eChGenerateCATV_22Plus;
				Generate.Offset = min(offset, 1UL - 1UL);
				Generate.Count = min(count, 1UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"C%dch+";
				Generate.NameOffset = 22UL;
			}
			else if (genSpace == L"CATV-H") {
				Generate.Space = eChGenerateCATV_H;
				Generate.Offset = min(offset, 41UL - 1UL);
				Generate.Count = min(count, 41UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"C%dch";
				Generate.NameOffset = 23UL;
			}
			else if (genSpace == L"CATV-24+") {
				Generate.Space = eChGenerateCATV_24Plus;
				Generate.Offset = min(offset, 4UL - 1UL);
				Generate.Count = min(count, 4UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"C%dch+";
				Generate.NameOffset = 24UL;
			}
			else if (genSpace == L"BS1") {
				Generate.Space = eChGenerateBS1;
				Generate.Offset = min(offset, 12UL - 1UL);
				Generate.Count = min(count, 12UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.SatelliteNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"BS%02d/TS%d";
				postfix = L"";
				Generate.NameOffset = 1UL;
				Generate.NameStep = 2UL;
				Generate.NameOffsetTS = 0UL;
				relativeTS = 4UL;
			}
			else if (genSpace == L"ND2") {
				Generate.Space = eChGenerateND2;
				Generate.Offset = min(offset, 12UL - 1UL);
				Generate.Count = min(count, 12UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.SatelliteNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"ND%02d/TS%d";
				postfix = L"";
				Generate.NameOffset = 2UL;
				Generate.NameStep = 2UL;
				Generate.NameOffsetTS = 0UL;
			}
			else if (genSpace == L"JD17A") {
				Generate.Space = eChGenerateJD17A;
				Generate.Offset = min(offset, 12UL - 1UL);
				Generate.Count = min(count, 12UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.SatelliteNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"JD%d";
				Generate.NameOffset = 17UL;
			}
			else if (genSpace == L"JD17B") {
				Generate.Space = eChGenerateJD17B;
				Generate.Offset = min(offset, 16UL - 1UL);
				Generate.Count = min(count, 16UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.SatelliteNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"JD%d";
				Generate.NameOffset = 17UL;
			}
			else if (genSpace == L"JD1") {
				Generate.Space = eChGenerateJD1;
				Generate.Offset = min(offset, 16UL - 1UL);
				Generate.Count = min(count, 16UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.SatelliteNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"JD%d";
				Generate.NameOffset = 1UL;
			}
			else if (genSpace == L"OPTICAST") {
				Generate.Space = eChGenerateOpticast;
				Generate.Offset = min(offset, 58UL - 1UL);
				Generate.Count = min(count, 58UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"H%03d";
				Generate.NameOffset = 1UL;
			}
			else if (genSpace == L"OPTICAST-11+") {
				Generate.Space = eChGenerateOpticast_11Plus;
				Generate.Offset = min(offset, 2UL - 1UL);
				Generate.Count = min(count, 2UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"H%03d+";
				Generate.NameOffset = 11UL;
			}
			else if (genSpace == L"OPTICAST-26+") {
				Generate.Space = eChGenerateOpticast_26Plus;
				Generate.Offset = min(offset, 3UL - 1UL);
				Generate.Count = min(count, 3UL - offset);
				Generate.ModulationNumber = 0UL;
				Generate.TuningFreq = 0UL;
				Generate.NameFormat = L"H%03d+";
				Generate.NameOffset = 26UL;
			}
			// ����TS�쐬��
			if (buf2[3] != L"") {
				Generate.RelativeTS = common::WStringToLong(buf2[3]);
				Generate.NameFormat += postfix;
			}
			else {
				Generate.RelativeTS = relativeTS;
			}

			// �ϒ������ԍ�
			if (buf[1] != L"") {
				Generate.ModulationNumber = common::WStringToLong(buf[1]);
			}
			// �q���ԍ�
			if (buf[2] != L"") {
				Generate.SatelliteNumber = common::WStringToLong(buf[2]);
			}
			// �擪�`�����l���ԍ�
			if (buf[3] != L"") {
				Generate.StartCh = common::WStringToLong(buf[3]);
			}
			else {
				Generate.StartCh = nextCh;
			}
			// �`���[�j���O���g���I�t�Z�b�g
			if (buf[4] != L"") {
				Generate.TuningFreq = common::WStringToLong(buf[4]);
			}
			// �`�����l�����t�H�[�}�b�g
			if (buf[5] != L"") {
				Generate.NameFormat = buf[5];
			}
			// �`�����l�����I�t�Z�b�g
			if (buf[6] != L"") {
				Generate.NameOffset = common::WStringToLong(buf[6]);
			}
			// �`�����l�����X�e�b�v
			if (buf[7] != L"") {
				Generate.NameStep = common::WStringToLong(buf[7]);
			}
			// TS�ԍ��I�t�Z�b�g
			if (buf[8] != L"") {
				Generate.NameOffsetTS = common::WStringToLong(buf[8]);
			}

			unsigned int freqBase = 0UL;			// �擪�`�����l���̎��g��
			unsigned int freqStep = 0UL;			// ���g���X�e�b�v
			unsigned int polarisastionType = 0UL;	// �Δg�^�C�v 0 .. �g�p���Ȃ�, 1 .. V/H�̌J��Ԃ�, 8 .. R�Œ�

			switch (Generate.Space) {
			case eChGenerateVHF_L:
				freqBase = 93000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateVHF_H:
				freqBase = 171000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateVHF_4Plus:
				freqBase = 173000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateUHF:
				freqBase = 473000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateCATV_L:
				freqBase = 111000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateCATV_22Plus:
				freqBase = 167000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateCATV_H:
				freqBase = 225000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateCATV_24Plus:
				freqBase = 233000UL;
				freqStep = 6000UL;
				break;
			case eChGenerateBS1:
				freqBase = 11727480UL;
				freqStep = 38360UL;
				polarisastionType = 8UL;
				break;
			case eChGenerateND2:
				freqBase = 12291000UL;
				freqStep = 40000UL;
				polarisastionType = 8UL;
				break;
			case eChGenerateJD1:
				freqBase = 12508000UL;
				freqStep = 15000UL;
				polarisastionType = 1UL;
				break;
			case eChGenerateJD17A:
				freqBase = 12268000UL;
				freqStep = 20000UL;
				polarisastionType = 1UL;
				break;
			case eChGenerateJD17B:
				freqBase = 12268000UL;
				freqStep = 15000UL;
				polarisastionType = 1UL;
				break;
			case eChGenerateOpticast:
				freqBase = 93250UL;
				freqStep = 6500UL;
				break;
			case eChGenerateOpticast_11Plus:
				freqBase = 159250UL;
				freqStep = 6500UL;
				break;
			case eChGenerateOpticast_26Plus:
				freqBase = 259750UL;
				freqStep = 6500UL;
				break;
			}

			if (Generate.Count && freqBase) {
				bReserveUnusedCh = TRUE;
				unsigned int tsCount = max(Generate.RelativeTS, 1);
				unsigned int chNum = Generate.StartCh;
				for (unsigned int ch = Generate.Offset; ch < Generate.Offset + Generate.Count; ch++) {
					for (unsigned int ts = 0; ts < tsCount; ts++) {
						auto itCh = itSpace->second.Channels.find(chNum);
						if (itCh == itSpace->second.Channels.end()) {
							itCh = itSpace->second.Channels.emplace(chNum, ChData()).first;
						}
						else {
							OutputDebug(L"    Replaced to :\n");
						}
						switch (polarisastionType) {
						case 1UL:
							itCh->second.Polarisation = (ch % 2UL) ? 1UL : 2UL;
							break;
						case 8UL:
							itCh->second.Polarisation = 4UL;
							break;
						default:
							itCh->second.Polarisation = 0UL;
							break;
						}
						itCh->second.ModulationType = Generate.ModulationNumber;
						itCh->second.Satellite = Generate.SatelliteNumber;
						itCh->second.Frequency = freqBase + freqStep * ch + Generate.TuningFreq;
						if (Generate.RelativeTS) {
							itCh->second.TSID = ts;
						}
						itCh->second.sServiceName = common::TStringPrintf(common::WStringToTString(Generate.NameFormat).c_str(), ch * Generate.NameStep + Generate.NameOffset, ts + Generate.NameOffsetTS);
						OutputDebug(L"%s: (auto) CH%03ld=%ld,%ld.%03ld,%c,%ld,%s,%ld,%ld,%ld,%ld,%ld\n", section.c_str(), itCh->first, itCh->second.Satellite, itCh->second.Frequency / 1000L,
							itCh->second.Frequency % 1000L, PolarisationChar[itCh->second.Polarisation], itCh->second.ModulationType, itCh->second.sServiceName.c_str(), itCh->second.SID, 
							itCh->second.TSID, itCh->second.ONID, itCh->second.MajorChannel, itCh->second.SourceID);
						chNum++;
						nextCh = max(nextCh, chNum);
					}
				}
				OutputDebug(L".\n");
			}
		}

		// ���g���I�t�Z�b�g�l
		itSpace->second.FrequencyOffset = (long)IniFileAccess.ReadKeyISectionData(L"FrequencyOffset", 0);

		// TuningSpace�̎�ޔԍ�
		itSpace->second.DVBSystemTypeNumber = IniFileAccess.ReadKeyISectionData(L"DVBSystemTypeNumber", 0);

		// TSMF�̏������[�h
		itSpace->second.TSMFMode = IniFileAccess.ReadKeyIValueMapSectionData(L"TSMFMode", 0, mapTSMFMode);

		// CH�ݒ�
		//    �`�����l���ԍ� = �q���ԍ�,���g��,�Δg,�ϒ�����[,�`�����l����[,SID/MinorChannel[,TSID/Channel[,ONID/PhysicalChannel[,MajorChannel[,SourceID]]]]]]
		//    ��: CH001 = 1,12658,V,0
		//      �`�����l���ԍ� : CH000�`CH999�Ŏw��
		//      �q���ԍ�       : Satellite�Z�N�V�����Őݒ肵���q���ԍ�(1�`4) �܂��� 0(���w�莞)
		//                       (�n�f�W�`���[�i�[����0���w�肵�Ă�������)
		//      ���g��         : ���g����MHz�Ŏw��
		//                       (�����_��t���邱�Ƃɂ��KHz�P�ʂł̎w�肪�\�ł�)
		//      �Δg           : 'V' = �����Δg 'H'=�����Δg 'L'=�����~�Δg 'R'=�E���~�Δg ' '=���w��
		//                       (�n�f�W�`���[�i�[���͖��w��)
		//      �ϒ�����       : Modulation�Z�N�V�����Őݒ肵���ϒ������ԍ�(0�`3)
		//      �`�����l����   : �`�����l������
		//                       (�ȗ������ꍇ�� 128.0E / 12658H / DVB - S �̂悤�Ȍ`���Ŏ�����������܂�)
		//      SID            : DVB / ISDB�̃T�[�r�XID
		//      TSID           : DVB / ISDB�̃g�����X�|�[�g�X�g���[��ID
		//      ONID           : DVB / ISDB�̃I���W�i���l�b�g���[�NID
		//      MinorChannel   : ATSC / Digital Cable��Minor Channel
		//      Channel        : ATSC / Digital Cable��Channel
		//      PhysicalChannel: ATSC / Digital Cable��Physical Channel
		//      MajorChannel   : Digital Cable��Major Channel
		//      SourceID       : Digital Cable��SourceID
		std::wstring key, data;
		while (IniFileAccess.ReadSectionData(&key, &data) == true) {
			std::wregex re(LR"(^CH(?:\d{3}|)$)", std::regex_constants::icase);
			if (std::regex_match(key, re) == true) {
				if (data.length() != 0) {
					// CH�ݒ�L��
					DWORD ch = common::WStringDecimalToLong(key.substr(2));

					// ReserveUnusedCh���w�肳��Ă���ꍇ��CH�ԍ����㏑������
					// �w�肳��Ă��Ȃ��ꍇ�͓o�^����CH�ԍ���U��
					DWORD chNum = bReserveUnusedCh ? ch : (DWORD)(itSpace->second.Channels.size());
					auto itCh = itSpace->second.Channels.find(chNum);
					if (itCh == itSpace->second.Channels.end()) {
						itCh = itSpace->second.Channels.emplace(chNum, ChData()).first;
					}

					// �J���}��؂��10�ɕ���
					std::wstring t(data);
					std::wstring buf[10];
					for (int n = 0; n < 10; n++) {
						if (std::wstring::npos == common::WStringSplit(&t, L',', &buf[n]))
							break;
					}

					// �q���ԍ�
					val = common::WStringToLong(buf[0]);
					if (val >= 0 && val < MAX_SATELLITE) {
						itCh->second.Satellite = val;
					}
					else
						OutputDebug(L"Format Error in readIniFile; Wrong Bird.\n");

					// ���g��
					val = (int)(common::WstringToDouble(buf[1]) * 1000.0);
					if ((val > 0) && (val <= 30000000)) {
						itCh->second.Frequency = val;
					}
					else
						OutputDebug(L"Format Error in readIniFile; Wrong Frequency.\n");

					// �Δg���
					WCHAR c = buf[2].c_str()[0];
					if (c == L'\0') {
						c = L' ';
					}
					auto it = std::find(std::begin(PolarisationChar), std::end(PolarisationChar), c);
					if (it != std::end(PolarisationChar)) {
						itCh->second.Polarisation = (unsigned int)(it - std::begin(PolarisationChar));
					}
					else
						OutputDebug(L"Format Error in readIniFile; Wrong Polarisation.\n");

					// �ϒ�����
					val = common::WStringToLong(buf[3]);
					if (val >= 0 && val < MAX_MODULATION) {
						itCh->second.ModulationType = val;
					}
					else
						OutputDebug(L"Format Error in readIniFile; Wrong Method.\n");

					// �`�����l����
					std::basic_string<TCHAR> name(common::WStringToTString(buf[4]));
					if (name.length() == 0)
						// ini�t�@�C���Ŏw�肵�����̂��Ȃ����128.0E/12658H/DVB-S �̂悤�Ȍ`���ō쐬����
						name = MakeChannelName(&itCh->second);
					itCh->second.sServiceName = name;

					// SID / PhysicalChannel
					if (buf[5] != L"") {
						val = common::WStringToLong(buf[5]);
						itCh->second.SID = val;
					}

					// TSID / Channel
					if (buf[6] != L"") {
						val = common::WStringToLong(buf[6]);
						itCh->second.TSID = val;
					}

					// ONID / MinorChannel
					if (buf[7] != L"") {
						val = common::WStringToLong(buf[7]);
						itCh->second.ONID = val;
					}

					// MajorChannel
					if (buf[8] != L"") {
						val = common::WStringToLong(buf[8]);
						itCh->second.MajorChannel = val;
					}

					// SourceID
					if (buf[9] != L"") {
						val = common::WStringToLong(buf[9]);
						itCh->second.SourceID = val;
					}
					OutputDebug(L"%s: (manual) CH%03ld=%ld,%ld.%03ld,%c,%ld,%s,%ld,%ld,%ld,%ld,%ld\n", section.c_str(), itCh->first, itCh->second.Satellite, itCh->second.Frequency / 1000L,
						itCh->second.Frequency % 1000L, PolarisationChar[itCh->second.Polarisation], itCh->second.ModulationType, itCh->second.sServiceName.c_str(), itCh->second.SID,
						itCh->second.TSID, itCh->second.ONID, itCh->second.MajorChannel, itCh->second.SourceID);
				}
			}
		}

		//�`�����l����`�̐�
		auto itChEnd = itSpace->second.Channels.end();
		if (itChEnd == itSpace->second.Channels.begin()) {
			// �`�����l����`��1���Ȃ�
			itSpace->second.dwNumChannel = 0;
		}
		else {
			// CH�ԍ��̍ő�l + 1
			itChEnd--;
			itSpace->second.dwNumChannel = itChEnd->first + 1;
		}
		OutputDebug(L"%s: dwNumChannel = %d.\n", section.c_str(), itSpace->second.dwNumChannel);

		// CH�ؑ֓���������I��2�x�s���ꍇ�̑Ώ�CH
		if (m_bLockTwice) {
			std::wstring s = IniFileAccess.ReadKeySSectionData(L"ChannelLockTwiceTarget", L"");
			if (s != L"") {
				while (1) {
					// �J���}��؂�܂ł̕�������擾
					std::wstring token;
					std::wstring::size_type pos = common::WStringSplit(&s, L',', &token);
					if (token != L"") {
						DWORD begin = 0;
						DWORD end = itSpace->second.dwNumChannel - 1;
						// �����'-'��؂�̐��l�ɕ���
						std::wstring left;
						std::wstring right(token);
						if (std::wstring::npos == common::WStringSplit(&right, L'-', &left)) {
							// "-"�L��������
							begin = end = common::WStringToLong(left);
						}
						else {
							// "-"�L�����L��
							if (left != L"") {
								// "-"�L���̑O�ɐ��l������
								begin = common::WStringToLong(left);
							}
							if (right != L"") {
								// "-"�L���̌�ɐ��l������
								end = common::WStringToLong(right);
							}
						}
						// �Ώ۔͈͂�CH��Flag��Set����
						for (DWORD ch = begin; ch <= end; ch++) {
							auto itCh = itSpace->second.Channels.find(ch);
							if (itCh != itSpace->second.Channels.end()) {
								itCh->second.LockTwiceTarget = TRUE;
							}
						}
					}
					if (pos == std::wstring::npos)
						break;
				}
			}
			else {
				// ChannelLockTwiceTarget�̎w�肪�����ꍇ�͂��ׂĂ�CH���Ώ�
				for (auto itCh = itSpace->second.Channels.begin(); itCh != itSpace->second.Channels.end(); itCh++) {
					itCh->second.LockTwiceTarget = TRUE;
				}
			}
		}
	}

	// �`���[�j���O��Ԕԍ�0��T��
	auto itSpace0 = m_TuningData.Spaces.find(0);
	if (itSpace0 == m_TuningData.Spaces.end()) {
		// �����ɂ͗��Ȃ��͂������ǈꉞ
		// ���TuningSpaceData���`���[�j���O��Ԕԍ�0�ɑ}��
		itSpace0 = m_TuningData.Spaces.emplace(0, TuningSpaceData()).first;
	}

	if (!itSpace0->second.Channels.size()) {
		// CH��`���������Ă��Ȃ�
		if (m_nDefaultNetwork == eDefaultNetworkSPHD) {
			// SPHD�̏ꍇ�̂݉ߋ��̃o�[�W�����݊�����
			// 3��TP���f�t�H���g�ŃZ�b�g���Ă���
			//   128.0E 12.658GHz V DVB-S *** 2015-10-10���݁ANIT�ɂ͑��݂��邯�ǒ�g��
			auto itCh = itSpace0->second.Channels.emplace(0, ChData()).first;
			itCh->second.Satellite = 1;
			itCh->second.Polarisation = 2;
			itCh->second.ModulationType = 0;
			itCh->second.Frequency = 12658000;
			itCh->second.sServiceName = MakeChannelName(&itCh->second);
			//   124.0E 12.613GHz H DVB-S2
			itCh = itSpace0->second.Channels.emplace(1, ChData()).first;
			itCh->second.Satellite = 2;
			itCh->second.Polarisation = 1;
			itCh->second.ModulationType = 1;
			itCh->second.Frequency = 12613000;
			itCh->second.sServiceName = MakeChannelName(&itCh->second);
			//   128.0E 12.733GHz H DVB-S2
			itCh = itSpace0->second.Channels.emplace(2, ChData()).first;
			itCh->second.Satellite = 1;
			itCh->second.Polarisation = 1;
			itCh->second.ModulationType = 1;
			itCh->second.Frequency = 12733000;
			itCh->second.sServiceName = MakeChannelName(&itCh->second);
			itSpace0->second.dwNumChannel = 3;
		}
	}

	// �`���[�j���O��Ԃ̐�
	auto itSpaceEnd = m_TuningData.Spaces.end();
	if (itSpaceEnd == m_TuningData.Spaces.begin()) {
		// ���������ꉞ
		m_TuningData.dwNumSpace = 0;
	}
	else {
		itSpaceEnd--;
		m_TuningData.dwNumSpace = itSpaceEnd->first + 1;
	}
}

void CBonTuner::GetSignalState(int* pnStrength, int* pnQuality, int* pnLock)
{
	if (pnStrength) *pnStrength = 0;
	if (pnQuality) *pnQuality = 0;
	if (pnLock) *pnLock = 1;

	// �`���[�i�ŗL GetSignalState ������΁A�ۓ���
	HRESULT hr;
	if ((m_pIBdaSpecials) && (hr = m_pIBdaSpecials->GetSignalState(pnStrength, pnQuality, pnLock)) != E_NOINTERFACE) {
		// E_NOINTERFACE �łȂ���΁A�ŗL�֐����������Ƃ������Ȃ̂ŁA
		// ���̂܂܃��^�[��
		return;
	}

	if (m_pTunerDevice == NULL)
		return;

	long longVal;
	BYTE byteVal;

	if (m_pITuner) {
		if ((m_bSignalLevelGetTypeTuner && ((m_bSignalLevelNeedStrength && pnStrength) || (m_bSignalLevelNeedQuality && pnQuality))) ||
				(m_bSignalLockedJudgeTypeTuner && pnLock)) {
			longVal = 0;
			if (SUCCEEDED(hr = m_pITuner->get_SignalStrength(&longVal))) {
				__int16 strength = (__int16)(longVal & 0xffffL);
				__int16 quality = (__int16)(longVal >> 16);
				if (m_bSignalLevelNeedStrength && pnStrength)
					*pnStrength = (int)(strength < 0xffffi16 ? 0xffffi16 - strength : strength);
				if (m_bSignalLevelNeedQuality && pnQuality)
					*pnQuality = min(max((int)quality, 0), 100);
				if (m_bSignalLockedJudgeTypeTuner && pnLock)
					*pnLock = strength != 0i16 ? 1 : 0;
			}
		}
	}

	if (m_pIBDA_SignalStatisticsTunerNode) {
		if (m_bSignalLevelGetTypeSS) {
			if (m_bSignalLevelNeedStrength && pnStrength) {
				longVal = 0;
				if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsTunerNode->get_SignalStrength(&longVal)))
					*pnStrength = (int)(longVal & 0xffff);
			}

			if (m_bSignalLevelNeedQuality && pnQuality) {
				longVal = 0;
				if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsTunerNode->get_SignalQuality(&longVal)))
					*pnQuality = (int)(min(max(longVal & 0xffff, 0), 100));
			}
		}

		if (m_bSignalLockedJudgeTypeSS && pnLock) {
			byteVal = 0;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsTunerNode->get_SignalLocked(&byteVal)))
				*pnLock = (int)byteVal;
		}
	}

	if (m_pIBDA_SignalStatisticsDemodNode) {
		if (m_bSignalLevelGetTypeDemodSS) {
			if (m_bSignalLevelNeedStrength && pnStrength) {
				longVal = 0;
				if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsDemodNode->get_SignalStrength(&longVal)))
					*pnStrength = (int)(longVal & 0xffff);
			}

			if (m_bSignalLevelNeedQuality && pnQuality) {
				longVal = 0;
				if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsDemodNode->get_SignalQuality(&longVal)))
					*pnQuality = (int)(min(max(longVal & 0xffff, 0), 100));
			}
		}

		if (m_bSignalLockedJudgeTypeDemodSS && pnLock) {
			byteVal = 0;
			if (SUCCEEDED(hr = m_pIBDA_SignalStatisticsDemodNode->get_SignalLocked(&byteVal)))
				*pnLock = (int)byteVal;
		}
	}

	return;
}

BOOL CBonTuner::LockChannel(const TuningParam *pTuningParam, BOOL bLockTwice)
{
	HRESULT hr;

	// �`���[�i�ŗL LockChannel ������΁A�ۓ���
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->LockChannel(pTuningParam)) != E_NOINTERFACE) {
		// BonDriver_BDA����pDLL
		// E_NOINTERFACE �łȂ���΁A�ŗL�֐����������Ƃ������Ȃ̂ŁA
		// ���̒��őI�Ǐ������s�Ȃ��Ă���͂��B����Ă��̂܂܃��^�[��
		m_nCurTone = pTuningParam->Antenna.Tone;
		if (SUCCEEDED(hr) && bLockTwice) {
			OutputDebug(L"  TwiceLock 1st[Special2] SUCCESS.\n");
			SleepWithMessageLoop(m_nLockTwiceDelay);
			hr = m_pIBdaSpecials2->LockChannel(pTuningParam);
		}
		if (SUCCEEDED(hr)) {
			OutputDebug(L"  LockChannel[Special2] SUCCESS.\n");
			return TRUE;
		} else {
			OutputDebug(L"  LockChannel[Special2] FAIL.\n");
			return FALSE;
		}
	}

	if (m_pIBdaSpecials && (hr = m_pIBdaSpecials->LockChannel(pTuningParam->Antenna.Tone ? 1 : 0, (pTuningParam->Polarisation == BDA_POLARISATION_LINEAR_H) ? TRUE : FALSE, pTuningParam->Frequency / 1000,
			(pTuningParam->Modulation.Modulation == BDA_MOD_NBC_8PSK || pTuningParam->Modulation.Modulation == BDA_MOD_8PSK) ? TRUE : FALSE)) != E_NOINTERFACE) {
		// BonDriver_BDA�I���W�i���݊�DLL
		// E_NOINTERFACE �łȂ���΁A�ŗL�֐����������Ƃ������Ȃ̂ŁA
		// ���̒��őI�Ǐ������s�Ȃ��Ă���͂��B����Ă��̂܂܃��^�[��
		m_nCurTone = pTuningParam->Antenna.Tone;
		if (SUCCEEDED(hr) && bLockTwice) {
			OutputDebug(L"  TwiceLock 1st[Special] SUCCESS.\n");
			SleepWithMessageLoop(m_nLockTwiceDelay);
			hr = m_pIBdaSpecials->LockChannel(pTuningParam->Antenna.Tone ? 1 : 0, (pTuningParam->Polarisation == BDA_POLARISATION_LINEAR_H) ? TRUE : FALSE, pTuningParam->Frequency / 1000,
					(pTuningParam->Modulation.Modulation == BDA_MOD_NBC_8PSK || pTuningParam->Modulation.Modulation == BDA_MOD_8PSK) ? TRUE : FALSE);
		}
		if (SUCCEEDED(hr)) {
			OutputDebug(L"  LockChannel[Special] SUCCESS.\n");
			return TRUE;
		}
		else {
			OutputDebug(L"  LockChannel[Special] FAIL.\n");
			return FALSE;
		}
	}

	// �`���[�i�ŗL�g�[������֐�������΁A����������ŌĂяo��
	if (m_pIBdaSpecials2 && (hr = m_pIBdaSpecials2->Set22KHz(pTuningParam->Antenna.Tone)) != E_NOINTERFACE) {
		// BonDriver_BDA����pDLL
		if (SUCCEEDED(hr)) {
			OutputDebug(L"  Set22KHz[Special2] successfully.\n");
			if (pTuningParam->Antenna.Tone != m_nCurTone) {
				m_nCurTone = pTuningParam->Antenna.Tone;
				if (m_nToneWait) {
					SleepWithMessageLoop(m_nToneWait); // �q���֑ؑ҂�
				}
			}
		}
		else {
			OutputDebug(L"  Set22KHz[Special2] failed.\n");
			// BDA generic �ȕ��@�Ő؂�ւ�邩������Ȃ��̂ŁA���b�Z�[�W�����o���āA���̂܂ܑ��s
		}
	}
	else if (m_pIBdaSpecials && (hr = m_pIBdaSpecials->Set22KHz(pTuningParam->Antenna.Tone ? 1 : 0)) != E_NOINTERFACE) {
		// BonDriver_BDA�I���W�i���݊�DLL
		if (SUCCEEDED(hr)) {
			OutputDebug(L"  Set22KHz[Special] successfully.\n");
			if (pTuningParam->Antenna.Tone != m_nCurTone) {
				m_nCurTone = pTuningParam->Antenna.Tone;
				if (m_nToneWait) {
					SleepWithMessageLoop(m_nToneWait); // �q���֑ؑ҂�
				}
			}
		}
		else {
			OutputDebug(L"  Set22KHz[Special] failed.\n");
			// BDA generic �ȕ��@�Ő؂�ւ�邩������Ȃ��̂ŁA���b�Z�[�W�����o���āA���̂܂ܑ��s
		}
	}
	else {
		// �ŗL�֐����Ȃ������Ȃ̂ŁA��������
	}

	// TuningSpace�̎�ޔԍ�
	unsigned int systemTypeNumber = m_TuningData.Spaces[pTuningParam->IniSpaceID].DVBSystemTypeNumber;
	if (!m_DVBSystemTypeDB.IsExist(systemTypeNumber))
		systemTypeNumber = 0;

	// ITuningSpace�p�����F
	//   ITuningSpace �� IDVBTuningSpace �� IDVBTuningSpace2 �� IDVBSTuningSpace
	//                �� IAnalogTVTuningSpace �� IATSCTuningSpace �� IDigitalCableTuningSpace
	//                �� IAnalogRadioTuningSpace �� IAnalogRadioTuningSpace2
	//                �� IAuxInTuningSpace �� IAuxInTuningSpace2
	CComPtr<ITuningSpace> pITuningSpace(m_DVBSystemTypeDB.SystemType[systemTypeNumber].pITuningSpace);
	if (!pITuningSpace) {
		OutputDebug(L"  Fail to get ITuningSpace.\n");
		return FALSE;
	}

	OutputDebug(L"    ITuningSpace\n");
	// IDVBSTuningSpace���L
	{
		CComQIPtr<IDVBSTuningSpace> pIDVBSTuningSpace(pITuningSpace);
		if (pIDVBSTuningSpace) {
			OutputDebug(L"    ->IDVBSTuningSpace\n");
			// LNB ���g����ݒ�
			if (pTuningParam->Antenna.HighOscillator != -1) {
				pIDVBSTuningSpace->put_HighOscillator(pTuningParam->Antenna.HighOscillator);
			}
			if (pTuningParam->Antenna.LowOscillator != -1) {
				pIDVBSTuningSpace->put_LowOscillator(pTuningParam->Antenna.LowOscillator);
			}

			// LNB�X�C�b�`�̎��g����ݒ�
			if (pTuningParam->Antenna.LNBSwitch != -1) {
				// LNBSwitch���g���̐ݒ肪����Ă���
				pIDVBSTuningSpace->put_LNBSwitch(pTuningParam->Antenna.LNBSwitch);
			}
			else {
				// 10GHz��ݒ肵�Ă�����High���ɁA20GHz��ݒ肵�Ă�����Low���ɐؑւ��͂�
				pIDVBSTuningSpace->put_LNBSwitch((pTuningParam->Antenna.Tone != 0) ? 10000000 : 20000000);
			}

			// �ʑ��ϒ��X�y�N�g�����]�̎��
			pIDVBSTuningSpace->put_SpectralInversion(BDA_SPECTRAL_INVERSION_AUTOMATIC);
		}
	}

	// ILocator�擾
	//
	// ILocator�p�����F
	//   ILocator �� IDigitalLocator �� IDVBTLocator �� IDVBTLocator2
	//                               �� IDVBSLocator �� IDVBSLocator2
	//                                               �� IISDBSLocator
	//                               �� IDVBCLocator
	//                               �� IATSCLocator �� IATSCLocator2 �� IDigitalCableLocator
	//            �� IAnalogLocator
	CComPtr<ILocator> pILocator;
	if (FAILED(hr = pITuningSpace->get_DefaultLocator(&pILocator))) {
		OutputDebug(L"  Fail to get ILocator. hr=0x%08lx\n", hr);
		return FALSE;
	}

	OutputDebug(L"    ILocator\n");
	// RF �M���̎��g����ݒ�
	pILocator->put_CarrierFrequency(pTuningParam->Frequency);

	// �����O���������̃^�C�v��ݒ�
	pILocator->put_InnerFEC(pTuningParam->Modulation.InnerFEC);

	// ���� FEC ���[�g��ݒ�
	// �O�������������Ŏg���o�C�i�� �R���{���[�V�����̃R�[�h ���[�g DVB-S�� 3/4 S2�� 3/5
	pILocator->put_InnerFECRate(pTuningParam->Modulation.InnerFECRate);

	// �ϒ��^�C�v��ݒ�
	// DVB-S��QPSK�AS2�̏ꍇ�� 8PSK
	pILocator->put_Modulation(pTuningParam->Modulation.Modulation);

	// �O���O���������̃^�C�v��ݒ�
	//	���[�h-�\������ 204/188 (�O�� FEC), DVB-S2�ł�����
	pILocator->put_OuterFEC(pTuningParam->Modulation.OuterFEC);

	// �O�� FEC ���[�g��ݒ�
	pILocator->put_OuterFECRate(pTuningParam->Modulation.OuterFECRate);

	// QPSK �V���{�� ���[�g��ݒ�
	pILocator->put_SymbolRate(pTuningParam->Modulation.SymbolRate);

	// IDVBSLocator���L
	{
		CComQIPtr<IDVBSLocator> pIDVBSLocator(pILocator);
		if (pIDVBSLocator) {
			OutputDebug(L"    ->IDVBSLocator\n");
			// �M���̕Δg��ݒ�
			pIDVBSLocator->put_SignalPolarisation(pTuningParam->Polarisation);
		}
	}

	// IDVBSLocator2���L
	{
		CComQIPtr<IDVBSLocator2> pIDVBSLocator2(pILocator);
		if (pIDVBSLocator2) {
			OutputDebug(L"    ->IDVBSLocator2\n");
			// DiSEqC��ݒ�
			if (pTuningParam->Antenna.DiSEqC >= BDA_LNB_SOURCE_A) {
				pIDVBSLocator2->put_DiseqLNBSource((LNB_Source)(pTuningParam->Antenna.DiSEqC));
			}
			else {
				pIDVBSLocator2->put_DiseqLNBSource(BDA_LNB_SOURCE_NOT_SET);
			}
		}
	}

	// IDVBTLocator���L
	{
		CComQIPtr<IDVBTLocator> pIDVBTLocator(pILocator);
		if (pIDVBTLocator) {
			OutputDebug(L"    ->IDVBTLocator\n");
			// ���g���̑ш敝 (MHz)��ݒ�
			if (pTuningParam->Modulation.BandWidth != -1) {
				pIDVBTLocator->put_Bandwidth(pTuningParam->Modulation.BandWidth);
			}
		}
	}

	// IATSCLocator���L
	{
		CComQIPtr<IATSCLocator> pIATSCLocator(pILocator);
		if (pIATSCLocator) {
			OutputDebug(L"    ->IATSCLocator\n");
			// ATSC PhysicalChannel
			if (pTuningParam->PhysicalChannel != -1) {
				pIATSCLocator->put_PhysicalChannel(pTuningParam->PhysicalChannel);
			}
		}
	}

	// ITuneRequest�쐬
	//
	// ITuneRequest�p�����F
	//   ITuneRequest �� IDVBTuneRequest
	//                �� IChannelTuneRequest �� IATSCChannelTuneRequest �� IDigitalCableTuneRequest
	//                �� IChannelIDTuneRequest
	//                �� IMPEG2TuneRequest
	CComPtr<ITuneRequest> pITuneRequest;
	if (FAILED(hr = pITuningSpace->CreateTuneRequest(&pITuneRequest))) {
		OutputDebug(L"  Fail to create ITuneRequest. hr=0x%08lx\n", hr);
		return FALSE;
	}

	OutputDebug(L"    ITuneRequest\n");
	// ITuneRequest��ILocator��ݒ�
	hr = pITuneRequest->put_Locator(pILocator);

	// IDVBTuneRequest���L
	{
		CComQIPtr<IDVBTuneRequest> pIDVBTuneRequest(pITuneRequest);
		if (pIDVBTuneRequest) {
			OutputDebug(L"    ->IDVBTuneRequest\n");
			// DVB Triplet ID�̐ݒ�
			pIDVBTuneRequest->put_ONID(pTuningParam->ONID);
			pIDVBTuneRequest->put_TSID(pTuningParam->TSID);
			pIDVBTuneRequest->put_SID(pTuningParam->SID);
		}
	}

	// IChannelTuneRequest���L
	{
		CComQIPtr<IChannelTuneRequest> pIChannelTuneRequest(pITuneRequest);
		if (pIChannelTuneRequest) {
			OutputDebug(L"    ->IChannelTuneRequest\n");
			// ATSC Channel
			pIChannelTuneRequest->put_Channel(pTuningParam->Channel);
		}
	}

	// IATSCChannelTuneRequest���L
	{
		CComQIPtr<IATSCChannelTuneRequest> pIATSCChannelTuneRequest(pITuneRequest);
		if (pIATSCChannelTuneRequest) {
			OutputDebug(L"    ->IATSCChannelTuneRequest\n");
			// ATSC MinorChannel
			pIATSCChannelTuneRequest->put_MinorChannel(pTuningParam->MinorChannel);
		}
	}

	// IDigitalCableTuneRequest���L
	{
		CComQIPtr<IDigitalCableTuneRequest> pIDigitalCableTuneRequest(pITuneRequest);
		if (pIDigitalCableTuneRequest) {
			OutputDebug(L"    ->IDigitalCableTuneRequest\n");
			// Digital Cable MinorChannel
			pIDigitalCableTuneRequest->put_MajorChannel(pTuningParam->MinorChannel);
			// Digital Cable SourceID
			pIDigitalCableTuneRequest->put_SourceID(pTuningParam->SourceID);
		}
	}

	if (m_pIBdaSpecials2) {
		// �`���[�i�ŗL��TSID�֐�������ΌĂяo��
		hr = m_pIBdaSpecials2->SetTSid(pTuningParam->TSID);

		// m_pIBdaSpecials��put_TuneRequest�̑O�ɉ��炩�̏������K�v�Ȃ�s��
		hr = m_pIBdaSpecials2->PreTuneRequest(pTuningParam, pITuneRequest);
	}

	if (pTuningParam->Antenna.Tone != m_nCurTone && m_nToneWait) {
		//�g�[���ؑւ���̏ꍇ�A��Ɉ�xTuneRequest���Ă���
		OutputDebug(L"  Requesting pre tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"  Fail to put pre tune request.  hr=0x%08lx\n", hr);
			return FALSE;
		}
		OutputDebug(L"  Pre tune request complete.\n");
		if (m_pIBdaSpecials2) {
			// m_pIBdaSpecials��put_TuneRequest�̌�ɉ��炩�̏������K�v�Ȃ�s��
			hr = m_pIBdaSpecials2->PostTuneRequest(pTuningParam);
		}

		SleepWithMessageLoop(m_nToneWait); // �q���֑ؑ҂�
	}
	m_nCurTone = pTuningParam->Antenna.Tone;

	if (bLockTwice) {
		// TuneRequest�������I��2�x�s��
		OutputDebug(L"  Requesting 1st twice tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"  Fail to put 1st twice tune request. hr=0x%08lx\n", hr);
			return FALSE;
		}
		OutputDebug(L"  1st Twice tune request complete.\n");
		if (m_pIBdaSpecials2) {
			// m_pIBdaSpecials��put_TuneRequest�̌�ɉ��炩�̏������K�v�Ȃ�s��
			hr = m_pIBdaSpecials2->PostTuneRequest(pTuningParam);
		}
		SleepWithMessageLoop(m_nLockTwiceDelay);
	}

	unsigned int nRetryRemain = m_nLockWaitRetry;
	int nLock = 0;
	do {
		OutputDebug(L"  Requesting tune.\n");
		if (FAILED(hr = m_pITuner->put_TuneRequest(pITuneRequest))) {
			OutputDebug(L"  Fail to put tune request. hr=0x%08lx\n", hr);
			return FALSE;
		}
		OutputDebug(L"  Tune request complete.\n");
		if (m_pIBdaSpecials2) {
			// m_pIBdaSpecials��put_TuneRequest�̌�ɉ��炩�̏������K�v�Ȃ�s��
			hr = m_pIBdaSpecials2->PostTuneRequest(pTuningParam);
		}

		static constexpr int LockRetryTime = 50;
		unsigned int nWaitRemain = m_nLockWait;
		SleepWithMessageLoop(m_nLockWaitDelay);
		GetSignalState(NULL, NULL, &nLock);
		while (!nLock && nWaitRemain) {
			DWORD dwSleepTime = (nWaitRemain > LockRetryTime) ? LockRetryTime : nWaitRemain;
			OutputDebug(L"    Waiting lock status remaining %d msec.\n", nWaitRemain);
			SleepWithMessageLoop(dwSleepTime);
			nWaitRemain -= dwSleepTime;
			GetSignalState(NULL, NULL, &nLock);
		}
	} while (!nLock && nRetryRemain--);

	if (nLock != 0)
		OutputDebug(L"  LockChannel success.\n");
	else
		OutputDebug(L"  LockChannel failed.\n");

	return nLock != 0;
}

// �`���[�i�ŗLDll�̃��[�h
HRESULT CBonTuner::CheckAndInitTunerDependDll(IBaseFilter * pTunerDevice, std::wstring tunerGUID, std::wstring tunerFriendlyName)
{
	if (m_aTunerParam.sDLLBaseName == L"") {
		// �`���[�i�ŗL�֐����g��Ȃ��ꍇ
		return S_OK;
	}

	if (common::WStringToUpperCase(m_aTunerParam.sDLLBaseName) == L"AUTO") {
		// INI �t�@�C���� "AUTO" �w��̏ꍇ
		BOOL found = FALSE;
		for (unsigned int i = 0; i < sizeof aTunerSpecialData / sizeof TUNER_SPECIAL_DLL; i++) {
			std::wstring dbGUID(aTunerSpecialData[i].sTunerGUID);
			if ((dbGUID != L"") && (tunerGUID.find(dbGUID)) != std::wstring::npos) {
				// ���̎��̃`���[�i�ˑ��R�[�h���`���[�i�p�����[�^�ɕϐ��ɃZ�b�g����
				m_aTunerParam.sDLLBaseName = aTunerSpecialData[i].sDLLBaseName;
				break;
			}
		}
		if (!found) {
			// ������Ȃ������̂Ń`���[�i�ŗL�֐��͎g��Ȃ�
			return S_OK;
		}
	}

	// ������ DLL �����[�h����B
	WCHAR szPath[_MAX_PATH + 1] = L"";
	::GetModuleFileNameW(st_hModule, szPath, _MAX_PATH + 1);
	// �t���p�X�𕪉�
	WCHAR szDrive[_MAX_DRIVE];
	WCHAR szDir[_MAX_DIR];
	WCHAR szFName[_MAX_FNAME];
	WCHAR szExt[_MAX_EXT];
	::_wsplitpath_s(szPath, szDrive, szDir, szFName, szExt);

	// �t�H���_���擾
	std::wstring sDllName;
	sDllName = common::WStringPrintf(L"%s%s%s.dll", szDrive, szDir, m_aTunerParam.sDLLBaseName.c_str());

	if ((m_hModuleTunerSpecials = ::LoadLibraryW(sDllName.c_str())) == NULL) {
		// ���[�h�ł��Ȃ��ꍇ�A�ǂ�����? 
		//  �� �f�o�b�O���b�Z�[�W�����o���āA�ŗL�֐����g��Ȃ����̂Ƃ��Ĉ���
		OutputDebug(L"CheckAndInitTunerDependDll: DLL Not found.\n");
		return S_OK;
	} else {
		OutputDebug(L"CheckAndInitTunerDependDll: Load Library successfully.\n");
	}

	HRESULT (* func)(IBaseFilter *, const WCHAR *, const WCHAR *, const WCHAR *) =
		(HRESULT (*)(IBaseFilter *, const WCHAR *, const WCHAR *, const WCHAR *))::GetProcAddress(m_hModuleTunerSpecials, "CheckAndInitTuner");
	if (!func) {
		// �������R�[�h������
		// ���������s�v
		return S_OK;
	}

	return (* func)(pTunerDevice, tunerGUID.c_str(), tunerFriendlyName.c_str(), m_sIniFilePath.c_str());
}

// �`���[�i�ŗLDll�ł̃L���v�`���f�o�C�X�m�F
HRESULT CBonTuner::CheckCapture(std::wstring tunerGUID, std::wstring tunerFriendlyName, std::wstring captureGUID, std::wstring captureFriendlyName)
{
	if (m_hModuleTunerSpecials == NULL) {
		return S_OK;
	}

	HRESULT (* func)(const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *) =
		(HRESULT (*)(const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *, const WCHAR *))::GetProcAddress(m_hModuleTunerSpecials, "CheckCapture");
	if (!func) {
		return S_OK;
	}

	return (* func)(tunerGUID.c_str(), tunerFriendlyName.c_str(), captureGUID.c_str(), captureFriendlyName.c_str(), m_sIniFilePath.c_str());
}

// �`���[�i�ŗL�֐��̃��[�h
void CBonTuner::LoadTunerDependCode(std::wstring tunerGUID, std::wstring tunerFriendlyName, std::wstring captureGUID, std::wstring captureFriendlyName)
{
	if (!m_hModuleTunerSpecials)
		return;

	IBdaSpecials* (*func)(CComPtr<IBaseFilter>);
	func = (IBdaSpecials* (*)(CComPtr<IBaseFilter>))::GetProcAddress(m_hModuleTunerSpecials, "CreateBdaSpecials");
	IBdaSpecials* (*func2)(CComPtr<IBaseFilter>, CComPtr<IBaseFilter>, const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*);
	func2 = (IBdaSpecials * (*)(CComPtr<IBaseFilter>, CComPtr<IBaseFilter>, const WCHAR*, const WCHAR*, const WCHAR*, const WCHAR*))::GetProcAddress(m_hModuleTunerSpecials, "CreateBdaSpecials2");
	if (!func2 && !func) {
		OutputDebug(L"LoadTunerDependCode: Cannot find CreateBdaSpecials.\n");
		::FreeLibrary(m_hModuleTunerSpecials);
		m_hModuleTunerSpecials = NULL;
		return;
	}
	if (func2)
	{
		OutputDebug(L"LoadTunerDependCode: CreateBdaSpecials2 found.\n");
		m_pIBdaSpecials = func2(m_pTunerDevice, m_pCaptureDevice, tunerGUID.c_str(), tunerFriendlyName.c_str(), captureGUID.c_str(), captureFriendlyName.c_str());
	}
	else {
		OutputDebug(L"LoadTunerDependCode: CreateBdaSpecials found.\n");
		m_pIBdaSpecials = func(m_pTunerDevice);
	}

	m_pIBdaSpecials2 = dynamic_cast<IBdaSpecials2b5 *>(m_pIBdaSpecials);
	if (!m_pIBdaSpecials2)
		OutputDebug(L"LoadTunerDependCode: Not IBdaSpecials2 Interface DLL.\n");

	//  BdaSpecials��ini�t�@�C����ǂݍ��܂���
	HRESULT hr;
	if (m_pIBdaSpecials2) {
		hr = m_pIBdaSpecials2->ReadIniFile(m_sIniFilePath.c_str());
	}

	// �`���[�i�ŗL�������֐��������Ŏ��s���Ă���
	if (m_pIBdaSpecials)
		m_pIBdaSpecials->InitializeHook();

	return;
}

// �`���[�i�ŗL�֐���Dll�̉��
void CBonTuner::ReleaseTunerDependCode(void)
{
	HRESULT hr;

	// �`���[�i�ŗL�֐�����`����Ă���΁A�����Ŏ��s���Ă���
	if (m_pIBdaSpecials) {
		if ((hr = m_pIBdaSpecials->FinalizeHook()) == E_NOINTERFACE) {
			// �ŗLFinalize�֐����Ȃ������Ȃ̂ŁA��������
		}
		else if (SUCCEEDED(hr)) {
			OutputDebug(L"ReleaseTunerDependCode: Tuner Special Finalize successfully.\n");
		}
		else {
			OutputDebug(L"ReleaseTunerDependCode: Tuner Special Finalize failed.\n");
		}

		SAFE_RELEASE(m_pIBdaSpecials);
		m_pIBdaSpecials2 = NULL;
	}

	if (m_hModuleTunerSpecials) {
		if (::FreeLibrary(m_hModuleTunerSpecials) == 0) {
			OutputDebug(L"ReleaseTunerDependCode: FreeLibrary failed.\n");
		}
		else {
			OutputDebug(L"ReleaseTunerDependCode: FreeLibrary Success.\n");
			m_hModuleTunerSpecials = NULL;
		}
	}
}

HRESULT CBonTuner::InitializeGraphBuilder(void)
{
	HRESULT hr = E_FAIL;
	
	// pIGraphBuilder interface���擾
	CComPtr<IGraphBuilder> pIGraphBuilder;
	if (FAILED(hr = pIGraphBuilder.CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER))) {
		OutputDebug(L"[InitializeGraphBuilder] Fail to get IGraphBuilder interface.\n");
	}
	else {
		// pIGraphBuilder interface�̎擾����
		// IMediaControl interface���擾
		CComQIPtr<IMediaControl> pIMediaControl(pIGraphBuilder);
		if (!pIMediaControl) {
			OutputDebug(L"[InitializeGraphBuilder] Fail to get IMediaControl interface.\n");
			hr = E_FAIL;
		}
		else {
			// �����Ȃ̂ł��̂܂܏I��
			m_pIGraphBuilder = pIGraphBuilder;
			m_pIMediaControl = pIMediaControl;
			return hr;
		}
	}

	// ���s
	return hr;
}

void CBonTuner::CleanupGraph(void)
{
	// DisconnectAll(m_pTif);
	// DisconnectAll(m_pDemux);
	// DisconnectAll(m_pTsWriter);
	// DisconnectAll(m_pCaptureDevice);
	// DisconnectAll(m_pTunerDevice);
	// DisconnectAll(m_pNetworkProvider);

	UnloadTif();
	UnloadDemux();
	UnloadTsWriter();

	// Tuner �� Capture �̏��� Release ���Ȃ���
	// ���������[�N���N�����f�o�C�X������
	UnloadTunerDevice();
	UnloadCaptureDevice();

	UnloadNetworkProvider();
	UnloadTuningSpace();

	m_pIMediaControl.Release();
	m_pIGraphBuilder.Release();

	return;
}

HRESULT CBonTuner::RunGraph(void)
{
	HRESULT hr;
	if (!m_pIMediaControl)
		return E_POINTER;

	SAFE_CLOSE_HANDLE(m_hStreamThread);
	m_bIsSetStreamThread = FALSE;

	if (FAILED(hr =  m_pIMediaControl->Run())) {
		return hr;
	}

	return S_OK;
}

void CBonTuner::StopGraph(void)
{
	HRESULT hr;
	if (m_pIMediaControl) {
		SAFE_CLOSE_HANDLE(m_hStreamThread);
		m_bIsSetStreamThread = FALSE;

		// a workaround for WinXP SP3
		// CBonTuner::LoadAndConnectDevice() �ɂē��삷��`���[�i������Ȃ������Ƃ��A
		// m_pIMediaControl->Stop() �̓����� MsDvbNp.ax �� access violation ���N�����B
		// �Ȃ̂ŁAStop ����K�v�̂Ȃ��Ƃ��͉������Ȃ��悤�ɂ���
		OAFilterState fs;
		if (FAILED(hr = m_pIMediaControl->GetState(100, &fs))) {
			OutputDebug(L"IMediaControl::GetState failed.\n");
		}
		else {
			if (fs == State_Stopped)
				return;
		}

		if (FAILED(hr = m_pIMediaControl->Pause())) {
			OutputDebug(L"IMediaControl::Pause failed.\n");
		}

		if (FAILED(hr = m_pIMediaControl->Stop())) {
			OutputDebug(L"IMediaControl::Stop failed.\n");
		}
	}
}

HRESULT CBonTuner::CreateTuningSpace(void)
{
	for (auto it = m_DVBSystemTypeDB.SystemType.begin(); it != m_DVBSystemTypeDB.SystemType.end(); it++) {
		OutputDebug(L"[CreateTuningSpace] Processing number %ld.\n", it->first);

		// �I�u�W�F�N�g�쐬�p�ϐ�
		enumTuningSpace specifyTuningSpace = eTuningSpaceAuto;					// �g�p����TuningSpace�I�u�W�F�N�g
		CLSID clsidTuningSpace = CLSID_NULL;									// TuningSpace�I�u�W�F�N�g�̃N���Xid
		enumLocator specifyLocator = eLocatorAuto;								// �g�p����Locator�I�u�W�F�N�g
		CLSID clsidLocator = CLSID_NULL;										// Locator�I�u�W�F�N�g�̃N���Xid

		// TuningSpace�ݒ�p�ϐ�
		// ITuningSpace
		_bstr_t bstrUniqueName;													// ITuningSpace�ɐݒ肷��UniqueName
		_bstr_t bstrFriendlyName;												// ITuningSpace�ɐݒ肷��FriendlyName
		enumNetworkType specifyITuningSpaceNetworkType = eNetworkTypeAuto;		// ITuningSpace�ɐݒ肷��NetworkType
		IID iidNetworkType = IID_NULL;											// ITuningSpace�ɐݒ肷��NetworkType��GUID
		// IDVBTuningSpace
		DVBSystemType dvbSystemType = DVB_Satellite;							// DVB�̃V�X�e���^�C�v
		// IDVBTuningSpace2
		long networkID = -1;													// Network ID
		// IDVBSTuningSpace
		long highOscillator = -1;												// High��Oscillator���g��
		long lowOscillator = -1;												// Low��Oscillator���g��
		long lnbSwitch = -1;													// LNB�X�C�b�`���g��
		SpectralInversion spectralInversion = BDA_SPECTRAL_INVERSION_NOT_SET;	// �X�y�N�g�����]
		// IAnalogTVTuningSpace
		TunerInputType inputType = TunerInputCable;								// �A���e�i�E�P�[�u���̓��̓^�C�v
		long countryCode = 0;													// ���E�n��R�[�h
		long minChannel = 0;													// Channel�ԍ��̍ŏ��l
		long maxChannel = 0;													// Channel�ԍ��̍ő�l
		// IATSCTuningSpace
		long minPhysicalChannel = 0;											// Physical Channel�ԍ��̍ŏ��l
		long maxPhysicalChannel = 0;											// Physical Channel�ԍ��̍ő�l
		long minMinorChannel = 0;												// Minor Channel�ԍ��̍ŏ��l
		long maxMinorChannel = 0;												// Minor Channel�ԍ��̍ő�l
		// IDigitalCableTuningSpace
		long minMajorChannel = 0;												// Major Channel�ԍ��̍ŏ��l
		long maxMajorChannel = 0;												// Major Channel�ԍ��̍ő�l
		long minSourceID = 0;													// Source ID�̍ŏ��l
		long maxSourceID = 0;													// Source ID�̍ő�l

		// Default Locator�ݒ�p�ϐ�
		// ILocator
		long frequency = -1;													// RF�M���̎��g��
		long symbolRate = -1;													// �V���{�����[�g
		FECMethod innerFECMethod = BDA_FEC_METHOD_NOT_SET;						// �����O���������^�C�v
		BinaryConvolutionCodeRate innerFECRate = BDA_BCC_RATE_NOT_SET;			// ����FEC���[�g
		FECMethod outerFECMethod = BDA_FEC_METHOD_NOT_SET;						// �O���O���������^�C�v
		BinaryConvolutionCodeRate outerFECRate = BDA_BCC_RATE_NOT_SET;			// �O��FEC���[�g
		ModulationType modulationType = BDA_MOD_NOT_SET;						// �ϒ��^�C�v
		// IDVBSLocator
		VARIANT_BOOL westPosition = VARIANT_TRUE;								// �q���̌o�x�𐼌o�Ɠ��o�̂ǂ���ŕ\�����邩(True�Ő��o)
		long orbitalPosition = -1;												// �q���̌o�x(1/10��)
		long elevation = -1;													// �q���̋p(1/10��)
		long azimuth = -1;														// �q���̕��ʊp(1/10��)
		Polarisation polarisation = BDA_POLARISATION_NOT_SET;					// �Δg�l
		// IDVBSLocator2
		LNB_Source diseqLNBSource = BDA_LNB_SOURCE_NOT_SET;						// DiSeqC LNB���̓\�[�X
		Pilot pilot = BDA_PILOT_NOT_SET;										// DVB-S2�p�C���b�g���[�h
		RollOff rollOff = BDA_ROLL_OFF_NOT_SET;									// DVB-S2���[���I�t�W��
		// IDVBTLocator
		long bandwidth = -1;													// �ш敝(MHz)
		GuardInterval guardInterval = BDA_GUARD_NOT_SET;						// �K�[�h�C���^�[�o��
		HierarchyAlpha hierarchyAlpha = BDA_HALPHA_NOT_SET;						// �K�w��A���t�@
		FECMethod lpInnerFECMethod = BDA_FEC_METHOD_NOT_SET;					// LP�X�g���[���̓����O���������^�C�v
		BinaryConvolutionCodeRate lpInnerFECRate = BDA_BCC_RATE_NOT_SET;		// LP�X�g���[���̓���FEC���[�g
		TransmissionMode transmissionMode = BDA_XMIT_MODE_NOT_SET;				// �`�����[�h
		VARIANT_BOOL otherFrequencyInUse = VARIANT_TRUE;						// �ʂ�DVB-T�u���[�h�L���X�^�Ŏg���Ă��邩�ǂ���
		// IDVBTLocator2
		long physicalLayerPipeId = -1;											// PLP ID
		// IATSCLocator
		long physicalChannel = -1;												// Physical Channel�ԍ�
		long transportStreamID = -1;											// TSID
		// IATSCLocator2
		long programNumber = -1;												// Program Number

		switch (it->second.nDVBSystemType) {
		case eTunerTypeDVBT:
		case eTunerTypeDVBT2:
			bstrUniqueName = L"DVB-T";
			bstrFriendlyName = L"Local DVB-T Digital Antenna";
			specifyTuningSpace = eTuningSpaceDVB;
			if (it->second.nDVBSystemType == eTunerTypeDVBT2) {
				specifyLocator = eLocatorDVBT2;
			}
			else {
				specifyLocator = eLocatorDVBT;
			}
			specifyITuningSpaceNetworkType = eNetworkTypeDVBT;
			dvbSystemType = DVB_Terrestrial;
			networkID = 0;
			break;

		case eTunerTypeDVBC:
			bstrUniqueName = L"DVB-C";
			bstrFriendlyName = L"Local DVB-C Digital Cable";
			specifyTuningSpace = eTuningSpaceDVB;
			specifyLocator = eLocatorDVBC;
			specifyITuningSpaceNetworkType = eNetworkTypeDVBC;
			dvbSystemType = DVB_Cable;
			networkID = 0;
			break;

		case eTunerTypeISDBT:
			bstrUniqueName = L"ISDB-T";
			bstrFriendlyName = L"Local ISDB-T Digital Antenna";
			specifyTuningSpace = eTuningSpaceDVB;
			specifyLocator = eLocatorDVBT;
			specifyITuningSpaceNetworkType = eNetworkTypeISDBT;
			dvbSystemType = ISDB_Terrestrial;
			networkID = -1;
			break;

		case eTunerTypeISDBC:
			bstrUniqueName = L"ISDB-C";
			bstrFriendlyName = L"Local ISDB-C Digital Cable";
			specifyTuningSpace = eTuningSpaceDVB;
			specifyLocator = eLocatorDVBC;
			specifyITuningSpaceNetworkType = eNetworkTypeISDBC;
			dvbSystemType = DVB_Cable;
			networkID = -1;
			break;

		case eTunerTypeISDBS:
			bstrUniqueName = L"ISDB-S";
			bstrFriendlyName = L"Default Digital ISDB-S Tuning Space";
			specifyTuningSpace = eTuningSpaceDVBS;
			specifyLocator = eLocatorISDBS;
			specifyITuningSpaceNetworkType = eNetworkTypeISDBS;
			dvbSystemType = ISDB_Satellite;
			networkID = -1;
			highOscillator = -1;
			lowOscillator = -1;
			lnbSwitch = -1;
			spectralInversion = BDA_SPECTRAL_INVERSION_NOT_SET;
			break;

		case eTunerTypeATSC_Antenna:
			bstrUniqueName = L"ATSC";
			bstrFriendlyName = L"Local ATSC Digital Antenna";
			specifyTuningSpace = eTuningSpaceATSC;
			specifyLocator = eLocatorATSC;
			specifyITuningSpaceNetworkType = eNetworkTypeATSC;
			inputType = TunerInputAntenna;
			countryCode = 0;
			minChannel = 1;
			maxChannel = 99;
			minPhysicalChannel = 2;
			maxPhysicalChannel = 69;
			minMinorChannel = 0;
			maxMinorChannel = 999;
			modulationType = BDA_MOD_128QAM;
			break;

		case eTunerTypeATSC_Cable:
			bstrUniqueName = L"ATSCCable";
			bstrFriendlyName = L"Local ATSC Digital Cable";
			specifyTuningSpace = eTuningSpaceATSC;
			specifyLocator = eLocatorATSC;
			specifyITuningSpaceNetworkType = eNetworkTypeATSC;
			inputType = TunerInputCable;
			countryCode = 0;
			minChannel = 1;
			maxChannel = 99;
			minPhysicalChannel = 1;
			maxPhysicalChannel = 158;
			minMinorChannel = 0;
			maxMinorChannel = 999;
			modulationType = BDA_MOD_128QAM;
			break;

		case eTunerTypeDigitalCable:
			bstrUniqueName = L"Digital Cable";
			bstrFriendlyName = L"Local Digital Cable";
			specifyTuningSpace = eTuningSpaceDigitalCable;
			specifyLocator = eLocatorDigitalCable;
			specifyITuningSpaceNetworkType = eNetworkTypeDigitalCable;
			inputType = TunerInputCable;
			countryCode = 0;
			minChannel = 2;
			maxChannel = 9999;
			minPhysicalChannel = 2;
			maxPhysicalChannel = 158;
			minMinorChannel = 0;
			maxMinorChannel = 999;
			minMajorChannel = 1;
			maxMajorChannel = 99;
			minSourceID = 0;
			maxSourceID = 0x7fffffff;
			break;

		case eTunerTypeDVBS:
		default:
			bstrUniqueName = L"DVB-S";
			bstrFriendlyName = L"Default Digital DVB-S Tuning Space";
			specifyTuningSpace = eTuningSpaceDVBS;
			specifyLocator = eLocatorDVBS;
			specifyITuningSpaceNetworkType = eNetworkTypeDVBS;
			dvbSystemType = DVB_Satellite;
			networkID = -1;
			highOscillator = 10600000;
			lowOscillator = 9750000;
			lnbSwitch = 11700000;
			spectralInversion = BDA_SPECTRAL_INVERSION_NOT_SET;
			westPosition = VARIANT_FALSE;
			break;
		}

		if (it->second.nTuningSpace != eTuningSpaceAuto) {
			specifyTuningSpace = it->second.nTuningSpace;
		}
		switch (specifyTuningSpace) {
		case eTuningSpaceDVB:
			clsidTuningSpace = __uuidof(DVBTuningSpace);
			break;
		case eTuningSpaceDVBS:
			clsidTuningSpace = __uuidof(DVBSTuningSpace);
			break;
		case eTuningSpaceAnalogTV:
			clsidTuningSpace = __uuidof(AnalogTVTuningSpace);
			break;
		case eTuningSpaceATSC:
			clsidTuningSpace = __uuidof(ATSCTuningSpace);
			break;
		case eTuningSpaceDigitalCable:
			clsidTuningSpace = __uuidof(DigitalCableTuningSpace);
			break;
		}

		if (it->second.nLocator != eLocatorAuto) {
			specifyLocator = it->second.nLocator;
		}
		switch (specifyLocator) {
		case eLocatorDVBT:
			clsidLocator = __uuidof(DVBTLocator);
			break;
		case eLocatorDVBT2:
			clsidLocator = __uuidof(DVBTLocator2);
			break;
		case eLocatorDVBS:
			clsidLocator = __uuidof(DVBSLocator);
			break;
		case eLocatorDVBC:
			clsidLocator = __uuidof(DVBCLocator);
			break;
		case eLocatorISDBS:
			clsidLocator = __uuidof(ISDBSLocator);
			break;
		case eLocatorATSC:
			clsidLocator = __uuidof(ATSCLocator);
			break;
		case eLocatorDigitalCable:
			clsidLocator = __uuidof(DigitalCableLocator);
			break;
		}

		if (it->second.nITuningSpaceNetworkType != eNetworkTypeAuto) {
			specifyITuningSpaceNetworkType = it->second.nITuningSpaceNetworkType;
		}
		switch (specifyITuningSpaceNetworkType) {
		case eNetworkTypeDVBT:
			iidNetworkType = { STATIC_DVB_TERRESTRIAL_TV_NETWORK_TYPE };
			break;
		case eNetworkTypeDVBS:
			iidNetworkType = { STATIC_DVB_SATELLITE_TV_NETWORK_TYPE };
			break;
		case eNetworkTypeDVBC:
			iidNetworkType = { STATIC_DVB_CABLE_TV_NETWORK_TYPE };
			break;
		case eNetworkTypeISDBT:
			iidNetworkType = { STATIC_ISDB_TERRESTRIAL_TV_NETWORK_TYPE };
			break;
		case eNetworkTypeISDBS:
			iidNetworkType = { STATIC_ISDB_SATELLITE_TV_NETWORK_TYPE };
			break;
		case eNetworkTypeISDBC:
			iidNetworkType = { STATIC_ISDB_CABLE_TV_NETWORK_TYPE };
			break;
		case eNetworkTypeATSC:
			iidNetworkType = { STATIC_ATSC_TERRESTRIAL_TV_NETWORK_TYPE };
			break;
		case eNetworkTypeDigitalCable:
			iidNetworkType = { STATIC_DIGITAL_CABLE_NETWORK_TYPE };
			break;
		case eNetworkTypeBSkyB:
			iidNetworkType = { STATIC_BSKYB_TERRESTRIAL_TV_NETWORK_TYPE };
			break;
		case eNetworkTypeDIRECTV:
			iidNetworkType = { STATIC_DIRECT_TV_SATELLITE_TV_NETWORK_TYPE };
			break;
		case eNetworkTypeEchoStar:
			iidNetworkType = { STATIC_ECHOSTAR_SATELLITE_TV_NETWORK_TYPE };
			break;
		}

		if (it->second.nIDVBTuningSpaceSystemType != enumDVBSystemType::eDVBSystemTypeAuto) {
			dvbSystemType = (DVBSystemType)it->second.nIDVBTuningSpaceSystemType;
		}

		if (it->second.nIAnalogTVTuningSpaceInputType != enumTunerInputType::eTunerInputTypeAuto) {
			inputType = (tagTunerInputType)it->second.nIAnalogTVTuningSpaceInputType;
		}

		HRESULT hr;

		CComPtr<ITuningSpace> pITuningSpace;
		// ITuningSpace���쐬
		//
		// ITuningSpace�p�����F
		//   ITuningSpace �� IDVBTuningSpace �� IDVBTuningSpace2 �� IDVBSTuningSpace
		//                �� IAnalogTVTuningSpace �� IATSCTuningSpace �� IDigitalCableTuningSpace
		//                �� IAnalogRadioTuningSpace �� IAnalogRadioTuningSpace2
		//                �� IAuxInTuningSpace �� IAuxInTuningSpace2
		if (FAILED(hr = pITuningSpace.CoCreateInstance(clsidTuningSpace, NULL, CLSCTX_INPROC_SERVER))) {
			OutputDebug(L"[CreateTuningSpace] Fail to get ITuningSpace interface\n");
		}
		else {
			// ITuningSpace �� NetworkType ��ݒ�
			if (FAILED(hr = pITuningSpace->put__NetworkType(iidNetworkType))) {
				OutputDebug(L"[CreateTuningSpace] put__NetworkType failed\n");
			}
			else {
				OutputDebug(L"[CreateTuningSpace] %s is created.\n", (wchar_t *)bstrFriendlyName);

				// ITuningSpace
				pITuningSpace->put_FrequencyMapping(L"");
				pITuningSpace->put_UniqueName(bstrUniqueName);
				pITuningSpace->put_FriendlyName(bstrFriendlyName);
				OutputDebug(L"  ITuningSpace is initialized.\n");

				// IDVBTuningSpace���L
				{
					CComQIPtr<IDVBTuningSpace> pIDVBTuningSpace(pITuningSpace);
					if (pIDVBTuningSpace) {
						pIDVBTuningSpace->put_SystemType(dvbSystemType);
						OutputDebug(L"  IDVBTuningSpace is initialized.\n");
					}
				}

				// IDVBTuningSpace2���L
				{
					CComQIPtr<IDVBTuningSpace2> pIDVBTuningSpace2(pITuningSpace);
					if (pIDVBTuningSpace2) {
						pIDVBTuningSpace2->put_NetworkID(networkID);
						OutputDebug(L"  IDVBTuningSpace2 is initialized.\n");
					}
				}

				// IDVBSTuningSpace���L
				{
					CComQIPtr<IDVBSTuningSpace> pIDVBSTuningSpace(pITuningSpace);
					if (pIDVBSTuningSpace) {
						pIDVBSTuningSpace->put_HighOscillator(highOscillator);
						pIDVBSTuningSpace->put_LowOscillator(lowOscillator);
						pIDVBSTuningSpace->put_LNBSwitch(lnbSwitch);
						pIDVBSTuningSpace->put_SpectralInversion(spectralInversion);
						OutputDebug(L"  IDVBSTuningSpace is initialized.\n");
					}
				}

				// IAnalogTVTuningSpace���L
				{
					CComQIPtr<IAnalogTVTuningSpace> pIAnalogTVTuningSpace(pITuningSpace);
					if (pIAnalogTVTuningSpace) {
						pIAnalogTVTuningSpace->put_InputType(inputType);
						pIAnalogTVTuningSpace->put_MinChannel(minChannel);
						pIAnalogTVTuningSpace->put_MaxChannel(maxChannel);
						pIAnalogTVTuningSpace->put_CountryCode(countryCode);
						OutputDebug(L"  IAnalogTVTuningSpace is initialized.\n");
					}
				}

				// IATSCTuningSpace���L
				{
					CComQIPtr<IATSCTuningSpace> pIATSCTuningSpace(pITuningSpace);
					if (pIATSCTuningSpace) {
						pIATSCTuningSpace->put_MinPhysicalChannel(minPhysicalChannel);
						pIATSCTuningSpace->put_MaxPhysicalChannel(maxPhysicalChannel);
						pIATSCTuningSpace->put_MinMinorChannel(minMinorChannel);
						pIATSCTuningSpace->put_MaxMinorChannel(maxMinorChannel);
						OutputDebug(L"  IATSCTuningSpace is initialized.\n");
					}
				}

				// IDigitalCableTuningSpace���L
				{
					CComQIPtr<IDigitalCableTuningSpace> pIDigitalCableTuningSpace(pITuningSpace);
					if (pIDigitalCableTuningSpace) {
						pIDigitalCableTuningSpace->put_MinMajorChannel(minMajorChannel);
						pIDigitalCableTuningSpace->put_MaxMajorChannel(maxMajorChannel);
						pIDigitalCableTuningSpace->put_MinSourceID(minSourceID);
						pIDigitalCableTuningSpace->put_MaxSourceID(maxSourceID);
						OutputDebug(L"  IDigitalCableTuningSpace is initialized.\n");
					}
				}

				// pILocator���쐬
				//
				// ILocator�p�����F
				//   ILocator �� IDigitalLocator �� IDVBTLocator �� IDVBTLocator2
				//                               �� IDVBSLocator �� IDVBSLocator2
				//                                               �� IISDBSLocator
				//                               �� IDVBCLocator
				//                               �� IATSCLocator �� IATSCLocator2 �� IDigitalCableLocator
				//            �� IAnalogLocator
				CComPtr<ILocator> pILocator;
				if (FAILED(hr = pILocator.CoCreateInstance(clsidLocator))) {
					OutputDebug(L"[CreateTuningSpace] Failed to get ILocator interface.\n");
				}
				else {
					// Default Locator�̒l���쐬 
					// ILocator
					pILocator->put_CarrierFrequency(frequency);
					pILocator->put_SymbolRate(symbolRate);
					pILocator->put_InnerFEC(innerFECMethod);
					pILocator->put_InnerFECRate(innerFECRate);
					pILocator->put_OuterFEC(outerFECMethod);
					pILocator->put_OuterFECRate(outerFECRate);
					pILocator->put_Modulation(modulationType);
					OutputDebug(L"  ILocator is initialized.\n");

					// IDigitalLocator���L�v���p�e�B�͖�������Log�Ɏc��
					{
						CComQIPtr<IDigitalLocator> pIDigitalLocator(pILocator);
						if (pIDigitalLocator) {
							OutputDebug(L"  IDigitalLocator is initialized.\n");
						}
					}

					// IDVBSLocator���L
					{
						CComQIPtr<IDVBSLocator> pIDVBSLocator(pILocator);
						if (pIDVBSLocator) {
							pIDVBSLocator->put_WestPosition(westPosition);
							pIDVBSLocator->put_OrbitalPosition(orbitalPosition);
							pIDVBSLocator->put_Elevation(elevation);
							pIDVBSLocator->put_Azimuth(azimuth);
							pIDVBSLocator->put_SignalPolarisation(polarisation);
							OutputDebug(L"  IDVBSLocator is initialized.\n");
						}
					}

					// IDVBSLocator2���L
					{
						CComQIPtr<IDVBSLocator2> pIDVBSLocator2(pILocator);
						if (pIDVBSLocator2) {
							pIDVBSLocator2->put_LocalOscillatorOverrideHigh(-1);
							pIDVBSLocator2->put_LocalOscillatorOverrideLow(-1);
							pIDVBSLocator2->put_LocalLNBSwitchOverride(-1);
							pIDVBSLocator2->put_LocalSpectralInversionOverride(BDA_SPECTRAL_INVERSION_NOT_SET);
							pIDVBSLocator2->put_DiseqLNBSource(diseqLNBSource);
							pIDVBSLocator2->put_SignalPilot(pilot);
							pIDVBSLocator2->put_SignalRollOff(rollOff);
							OutputDebug(L"  IDVBSLocator2 is initialized.\n");
						}
					}

					// IISDBSLocator���L�v���p�e�B�͖�������Log�Ɏc��
					{
						CComQIPtr<IISDBSLocator> pIISDBSLocator(pILocator);
						if (pIISDBSLocator) {
							OutputDebug(L"  IISDBSLocator is initialized.\n");
						}
					}

					// IDVBTLocator���L
					{
						CComQIPtr<IDVBTLocator> pIDVBTLocator(pILocator);
						if (pIDVBTLocator) {
							pIDVBTLocator->put_Bandwidth(bandwidth);
							pIDVBTLocator->put_Guard(guardInterval);
							pIDVBTLocator->put_HAlpha(hierarchyAlpha);
							pIDVBTLocator->put_LPInnerFEC(lpInnerFECMethod);
							pIDVBTLocator->put_LPInnerFECRate(lpInnerFECRate);
							pIDVBTLocator->put_Mode(transmissionMode);
							pIDVBTLocator->put_OtherFrequencyInUse(otherFrequencyInUse);
							OutputDebug(L"  IDVBTLocator is initialized.\n");
						}
					}

					// IDVBTLocator2���L
					{
						CComQIPtr<IDVBTLocator2> pIDVBTLocator2(pILocator);
						if (pIDVBTLocator2) {
							pIDVBTLocator2->put_PhysicalLayerPipeId(physicalLayerPipeId);
							OutputDebug(L"  IDVBTLocator2 is initialized.\n");
						}
					}

					// IDVBCLocator���L�v���p�e�B�͖�������Log�Ɏc��
					{
						CComQIPtr<IDVBCLocator> pIDVBCLocator(pILocator);
						if (pIDVBCLocator) {
							OutputDebug(L"  IDVBCLocator is initialized.\n");
						}
					}

					// IATSCLocator���L
					{
						CComQIPtr<IATSCLocator> pIATSCLocator(pILocator);
						if (pIATSCLocator) {
							pIATSCLocator->put_PhysicalChannel(physicalChannel);
							pIATSCLocator->put_TSID(transportStreamID);
							OutputDebug(L"  IATSCLocator is initialized.\n");
						}
					}

					// IATSCLocator2���L
					{
						CComQIPtr<IATSCLocator2> pIATSCLocator2(pILocator);
						if (pIATSCLocator2) {
							pIATSCLocator2->put_ProgramNumber(programNumber);
							OutputDebug(L"  IATSCLocator2 is initialized.\n");
						}
					}

					// IDigitalCableLocator���L�v���p�e�B�͖�������Log�Ɏc��
					{
						CComQIPtr<IDigitalCableLocator> pIDigitalCableLocator(pILocator);
						if (pIDigitalCableLocator) {
							OutputDebug(L"  IDigitalCableLocator is initialized.\n");
						}
					}

					pITuningSpace->put_DefaultLocator(pILocator);

					// �S�Đ���
					it->second.pITuningSpace = pITuningSpace;
					OutputDebug(L"[CreateTuningSpace] Process number %ld was successful.\n", it->first);
					continue;
				}
			}
		}

		// ���s
		OutputDebug(L"[CreateTuningSpace] Process number %ld failed.\n", it->first);
	}

	if (!m_DVBSystemTypeDB.IsExist(0) || !m_DVBSystemTypeDB.SystemType[0].pITuningSpace) {
		OutputDebug(L"[CreateTuningSpace] Fail to create default ITuningSpace.\n");
		return E_FAIL;
	}

	return S_OK;
}

void CBonTuner::UnloadTuningSpace(void)
{
	m_DVBSystemTypeDB.ReleaseAll();
}

// Tuning Request �𑗂��� Tuning Space ������������
//   ��������Ȃ��� output pin ���o�����Ȃ��`���[�i�t�B���^��
//   ����炵��
HRESULT CBonTuner::InitTuningSpace(void)
{
	if (!m_DVBSystemTypeDB.IsExist(0)) {
		OutputDebug(L"[InitTuningSpace] TuningSpace NOT SET.\n");
		return E_POINTER;
	}

	if (!m_pITuner) {
		OutputDebug(L"[InitTuningSpace] ITuner NOT SET.\n");
		return E_POINTER;
	}

	HRESULT hr = E_FAIL;

	// ITuneRequest�쐬
	//
	// ITuneRequest�p�����F
	//   ITuneRequest �� IDVBTuneRequest
	//                �� IChannelTuneRequest �� IATSCChannelTuneRequest �� IDigitalCableTuneRequest
	//                �� IChannelIDTuneRequest
	//                �� IMPEG2TuneRequest
	CComPtr<ITuneRequest> pITuneRequest;
	if (FAILED(hr = m_DVBSystemTypeDB.SystemType[0].pITuningSpace->CreateTuneRequest(&pITuneRequest))) {
		OutputDebug(L"[InitTuningSpace] Fail to get ITuneRequest interface.\n");
	}
	else {
		// IDVBTuneRequest���L
		{
			CComQIPtr<IDVBTuneRequest> pIDVBTuneRequest(pITuneRequest);
			if (pIDVBTuneRequest) {
				pIDVBTuneRequest->put_ONID(-1);
				pIDVBTuneRequest->put_TSID(-1);
				pIDVBTuneRequest->put_SID(-1);
			}
		}

		// IChannelTuneRequest���L
		{
			CComQIPtr<IChannelTuneRequest> pIChannelTuneRequest(pITuneRequest);
			if (pIChannelTuneRequest) {
				pIChannelTuneRequest->put_Channel(-1);
			}
		}

		// IATSCChannelTuneRequest���L
		{
			CComQIPtr<IATSCChannelTuneRequest> pIATSCChannelTuneRequest(pITuneRequest);
			if (pIATSCChannelTuneRequest) {
				pIATSCChannelTuneRequest->put_MinorChannel(-1);
			}
		}

		// IDigitalCableTuneRequest���L
		{
			CComQIPtr<IDigitalCableTuneRequest> pIDigitalCableTuneRequest(pITuneRequest);
			if (pIDigitalCableTuneRequest) {
				pIDigitalCableTuneRequest->put_MajorChannel(-1);
				pIDigitalCableTuneRequest->put_SourceID(-1);
			}
		}

		hr = m_pITuner->put_TuningSpace(m_DVBSystemTypeDB.SystemType[0].pITuningSpace);
		hr = m_pITuner->put_TuneRequest(pITuneRequest);

		// �S�Đ���
		return S_OK;
	}

	// ���s
	return hr;
}

HRESULT CBonTuner::LoadNetworkProvider(void)
{
	CLSID clsidNetworkProvider = CLSID_NULL;

	switch (m_nNetworkProvider) {
	case eNetworkProviderGeneric:
		clsidNetworkProvider = CLSID_NetworkProvider;
		break;
	case eNetworkProviderDVBS:
		clsidNetworkProvider = CLSID_DVBSNetworkProvider;
		break;
	case eNetworkProviderDVBT:
		clsidNetworkProvider = CLSID_DVBTNetworkProvider;
		break;
	case eNetworkProviderDVBC:
		clsidNetworkProvider = CLSID_DVBCNetworkProvider;
		break;
	case eNetworkProviderATSC:
		clsidNetworkProvider = CLSID_ATSCNetworkProvider;
		break;
	case eNetworkProviderAuto:
	default:
		if (m_DVBSystemTypeDB.nNumType > 1) {
			clsidNetworkProvider = CLSID_NetworkProvider;
		}
		else {
			switch (m_DVBSystemTypeDB.SystemType[0].nDVBSystemType) {
			case eTunerTypeDVBS:
			case eTunerTypeISDBS:
				clsidNetworkProvider = CLSID_DVBSNetworkProvider;
				break;
			case eTunerTypeDVBT:
			case eTunerTypeDVBT2:
			case eTunerTypeISDBT:
				clsidNetworkProvider = CLSID_DVBTNetworkProvider;
				break;
			case eTunerTypeDVBC:
				clsidNetworkProvider = CLSID_DVBCNetworkProvider;
				break;
			case eTunerTypeATSC_Antenna:
			case eTunerTypeATSC_Cable:
			case eTunerTypeDigitalCable:
				clsidNetworkProvider = CLSID_ATSCNetworkProvider;
				break;
			default:
				clsidNetworkProvider = CLSID_NetworkProvider;
				break;
			}
		}
		break;
	}

	HRESULT hr = E_FAIL;

	CComPtr<IBaseFilter> pNetworkProvider;
	// Network Proveider�t�B���^���擾
	if (FAILED(hr = pNetworkProvider.CoCreateInstance(clsidNetworkProvider, NULL, CLSCTX_INPROC_SERVER))) {
		OutputDebug(L"[LoadNetworkProvider] Fail to get NetworkProvider IBaseFilter interface.\n");
	}
	else {
		std::wstring strName = CDSFilterEnum::getRegistryName(pNetworkProvider);
		OutputDebug(L"[LoadNetworkProvider] %s is loaded.\n", strName.c_str());
		// �t�B���^�擾����
		// Graph Builder�Ƀt�B���^��ǉ�
		if (FAILED(hr = m_pIGraphBuilder->AddFilter(pNetworkProvider, strName.c_str()))) {
			OutputDebug(L"[LoadNetworkProvider] Fail to add NetworkProvider IBaseFilter into graph.\n");
		}
		else {
			// �t�B���^�ǉ�����
			// ITuner interface���擾
			CComQIPtr<ITuner> pITuner(pNetworkProvider);
			if (!pITuner) {
				OutputDebug(L"[LoadNetworkProvider] Fail to get ITuner interface.\n");
				hr = E_FAIL;
			}
			else {
				// ITuner interface�̎擾����
				// �S�Đ���
				m_pNetworkProvider = pNetworkProvider;
				m_pITuner = pITuner;
				return hr;
			}
		}
	}

	// ���s
	return hr;
}

void CBonTuner::UnloadNetworkProvider(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pNetworkProvider)
		hr = m_pIGraphBuilder->RemoveFilter(m_pNetworkProvider);

	m_pITuner.Release();
	m_pNetworkProvider.Release();
}

// ini �t�@�C���Ŏw�肳�ꂽ�`���[�i�E�L���v�`���̑g����List���쐬
HRESULT CBonTuner::InitDSFilterEnum(void)
{
	HRESULT hr;

	// �V�X�e���ɑ��݂���`���[�i�E�L���v�`���̃��X�g
	std::vector<DSListData> TunerList;
	std::vector<DSListData> CaptureList;

	ULONG order;

	SAFE_DELETE(m_pDSFilterEnumTuner);
	SAFE_DELETE(m_pDSFilterEnumCapture);

	m_pDSFilterEnumTuner = new CDSFilterEnum(KSCATEGORY_BDA_NETWORK_TUNER, CDEF_DEVMON_PNP_DEVICE);
	order = 0;
	while (SUCCEEDED(hr = m_pDSFilterEnumTuner->next()) && hr == S_OK) {
		std::wstring sDisplayName;
		std::wstring sFriendlyName;

		// �`���[�i�� DisplayName, FriendlyName �𓾂�
		m_pDSFilterEnumTuner->getDisplayName(&sDisplayName);
		m_pDSFilterEnumTuner->getFriendlyName(&sFriendlyName);

		// �ꗗ�ɒǉ�
		TunerList.emplace_back(sDisplayName, sFriendlyName, order);

		order++;
	}

	m_pDSFilterEnumCapture = new CDSFilterEnum(KSCATEGORY_BDA_RECEIVER_COMPONENT, CDEF_DEVMON_PNP_DEVICE);
	order = 0;
	while (SUCCEEDED(hr = m_pDSFilterEnumCapture->next()) && hr == S_OK) {
		std::wstring sDisplayName;
		std::wstring sFriendlyName;

		// �`���[�i�� DisplayName, FriendlyName �𓾂�
		m_pDSFilterEnumCapture->getDisplayName(&sDisplayName);
		m_pDSFilterEnumCapture->getFriendlyName(&sFriendlyName);

		// �ꗗ�ɒǉ�
		CaptureList.emplace_back(sDisplayName, sFriendlyName, order);

		order++;
	}

	unsigned int total = 0;
	m_UsableTunerCaptureList.clear();

	for (unsigned int i = 0; i < m_aTunerParam.Tuner.size(); i++) {
		for (auto it = TunerList.begin(); it != TunerList.end(); it++) {
			// DisplayName �� GUID ���܂܂�邩�������āANO�������玟�̃`���[�i��
			if (m_aTunerParam.Tuner[i].TunerGUID.compare(L"") != 0 && it->GUID.find(m_aTunerParam.Tuner[i].TunerGUID) == std::wstring::npos) {
				continue;
			}

			// FriendlyName ���܂܂�邩�������āANO�������玟�̃`���[�i��
			if (m_aTunerParam.Tuner[i].TunerFriendlyName.compare(L"") != 0 && it->FriendlyName.find(m_aTunerParam.Tuner[i].TunerFriendlyName) == std::wstring::npos) {
				continue;
			}

			// �Ώۂ̃`���[�i�f�o�C�X������
			OutputDebug(L"[InitDSFilterEnum] Found tuner device=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
			if (!m_aTunerParam.bNotExistCaptureDevice) {
				// Capture�f�o�C�X���g�p����
				std::vector<DSListData> TempCaptureList;
				for (auto it2 = CaptureList.begin(); it2 != CaptureList.end(); it2++) {
					// DisplayName �� GUID ���܂܂�邩�������āANO�������玟�̃L���v�`����
					if (m_aTunerParam.Tuner[i].CaptureGUID.compare(L"") != 0 && it2->GUID.find(m_aTunerParam.Tuner[i].CaptureGUID) == std::wstring::npos) {
						continue;
					}

					// FriendlyName ���܂܂�邩�������āANO�������玟�̃L���v�`����
					if (m_aTunerParam.Tuner[i].CaptureFriendlyName.compare(L"") != 0 && it2->FriendlyName.find(m_aTunerParam.Tuner[i].CaptureFriendlyName) == std::wstring::npos) {
						continue;
					}

					// �Ώۂ̃L���v�`���f�o�C�X������
					OutputDebug(L"[InitDSFilterEnum]   Found capture device=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
					TempCaptureList.emplace_back(*it2);
				}

				if (TempCaptureList.empty()) {
					// �L���v�`���f�o�C�X��������Ȃ������̂Ŏ��̃`���[�i��
					OutputDebug(L"[InitDSFilterEnum]   No combined capture devices.\n");
					continue;
				}

				// �`���[�i��List�ɒǉ�
				m_UsableTunerCaptureList.emplace_back(*it);

				unsigned int count = 0;
				if (m_aTunerParam.bCheckDeviceInstancePath) {
					// �`���[�i�f�o�C�X�ƃL���v�`���f�o�C�X�̃f�o�C�X�C���X�^���X�p�X����v���Ă��邩�m�F
					OutputDebug(L"[InitDSFilterEnum]   Checking device instance path.\n");
					std::wstring dip = CDSFilterEnum::getDeviceInstancePathrFromDisplayName(it->GUID);
					for (auto it2 = TempCaptureList.begin(); it2 != TempCaptureList.end(); it2++) {
						if (CDSFilterEnum::getDeviceInstancePathrFromDisplayName(it2->GUID) == dip) {
							// �f�o�C�X�p�X����v������̂�List�ɒǉ�
							OutputDebug(L"[InitDSFilterEnum]     Adding matched tuner and capture device.\n");
							OutputDebug(L"[InitDSFilterEnum]       tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
							OutputDebug(L"[InitDSFilterEnum]       capture=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
							m_UsableTunerCaptureList.back().CaptureList.emplace_back(*it2);
							count++;
						}
					}
				}

				if (count == 0) {
					// �f�o�C�X�p�X����v������̂��Ȃ����� or �m�F���Ȃ�
					if (m_aTunerParam.bCheckDeviceInstancePath) {
						OutputDebug(L"[InitDSFilterEnum]     No matched devices.\n");
					}
					for (auto it2 = TempCaptureList.begin(); it2 != TempCaptureList.end(); it2++) {
						// ���ׂ�List�ɒǉ�
						OutputDebug(L"[InitDSFilterEnum]   Adding tuner and capture device.\n");
						OutputDebug(L"[InitDSFilterEnum]     tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
						OutputDebug(L"[InitDSFilterEnum]     capture=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
						m_UsableTunerCaptureList.back().CaptureList.emplace_back(*it2);
						count++;
					}
				}

				OutputDebug(L"[InitDSFilterEnum]   %d of combination was added.\n", count);
				total += count;
			}
			else
			{
				// Capture�f�o�C�X���g�p���Ȃ�
				OutputDebug(L"[InitDSFilterEnum]   Adding tuner device only.\n");
				OutputDebug(L"[InitDSFilterEnum]     tuner=FriendlyName:%s,  GUID:%s\n", it->FriendlyName.c_str(), it->GUID.c_str());
				m_UsableTunerCaptureList.emplace_back(*it);
			}
		}
	}
	if (m_UsableTunerCaptureList.empty()) {
		OutputDebug(L"[InitDSFilterEnum] No devices found.\n");
		return E_FAIL;
	}

	OutputDebug(L"[InitDSFilterEnum] Total %d of combination was added.\n", total);
	return S_OK;
}

// �`���[�i�E�L���v�`���̑g���킹���X�g���瓮�삷����̂�T��
HRESULT CBonTuner::LoadAndConnectDevice(void)
{
	HRESULT hr;
	if (!m_DVBSystemTypeDB.IsExist(0) || !m_pNetworkProvider) {
		OutputDebug(L"[P->T] TuningSpace or NetworkProvider NOT SET.\n");
		return E_POINTER;
	}

	if (!m_pDSFilterEnumTuner || (!m_pDSFilterEnumCapture && !m_aTunerParam.bNotExistCaptureDevice)) {
		OutputDebug(L"[P->T] DSFilterEnum NOT SET.\n");
		return E_POINTER;
	}

	for (auto it = m_UsableTunerCaptureList.begin(); it != m_UsableTunerCaptureList.end(); it++) {
		OutputDebug(L"[P->T] Trying tuner device=FriendlyName:%s,  GUID:%s\n", it->Tuner.FriendlyName.c_str(), it->Tuner.GUID.c_str());
		// �`���[�i�f�o�C�X���[�v
		// �r�������p�ɃZ�}�t�H�p��������쐬 ('\' -> '/')
		std::wstring semName = it->Tuner.GUID;
		std::replace(semName.begin(), semName.end(), L'\\', L'/');
		semName = L"Global\\" + semName;

		// �r������
		m_hSemaphore = ::CreateSemaphoreW(NULL, 1, 1, semName.c_str());
		DWORD result = WaitForSingleObjectWithMessageLoop(m_hSemaphore, 0);
		if (result != WAIT_OBJECT_0) {
			// �g�p���Ȃ̂Ŏ��̃`���[�i��T��
			OutputDebug(L"[P->T] Another is using.\n");
		} 
		else {
			// �r���m�FOK
			CComPtr<IBaseFilter> pTunerDevice;
			// �`���[�i�f�o�C�X�̃t�B���^���擾
			if (FAILED(hr = m_pDSFilterEnumTuner->getFilter(&pTunerDevice, it->Tuner.Order))) {
				// �t�B���^���擾�ł��Ȃ������̂Ŏ��̃`���[�i��
				OutputDebug(L"[P->T] Fail to get TunerDevice IBaseFilter interface.\n");
			}
			else {
				// �t�B���^�擾����
				// Graph Builder�Ƀ`���[�i�f�o�C�X�̃t�B���^��ǉ�
				if (FAILED(hr = m_pIGraphBuilder->AddFilter(pTunerDevice, it->Tuner.FriendlyName.c_str()))) {
					// �t�B���^�̒ǉ��Ɏ��s�����̂Ŏ��̃`���[�i��
					OutputDebug(L"[P->T] Fail to add TunerDevice IBaseFilter into graph.\n");
				}
				else {
					// �t�B���^�ǉ�����
					// �`���[�i�f�o�C�X��connect ���Ă݂�
					if (FAILED(hr = Connect(m_pNetworkProvider, pTunerDevice))) {
						// connect�Ɏ��s�����̂Ŏ��̃`���[�i��
						OutputDebug(L"[P->T] Fail to connect.\n");
					}
					else {
						// connect ����
						OutputDebug(L"[P->T] Connect OK.\n");
						// �`���[�i�ŗLDll���K�v�Ȃ�Ǎ��݁A�ŗL�̏���������������ΌĂяo��
						if (FAILED(hr = CheckAndInitTunerDependDll(pTunerDevice, it->Tuner.GUID, it->Tuner.FriendlyName))) {
							// �ŗLDll�̏��������s�����̂Ŏ��̃`���[�i��
							OutputDebug(L"[P->T] Discarded by BDASpecial's CheckAndInitTuner function.\n");
						}
						else {
							// �ŗLDll����OK
							if (!m_aTunerParam.bNotExistCaptureDevice) {
								// �L���v�`���f�o�C�X���g�p����ꍇ
								for (auto it2 = it->CaptureList.begin(); it2 != it->CaptureList.end(); it2++) {
									// �L���v�`���f�o�C�X���[�v
									OutputDebug(L"[T->C] Trying capture device=FriendlyName:%s,  GUID:%s\n", it2->FriendlyName.c_str(), it2->GUID.c_str());
									// �`���[�i�ŗLDll�ł̊m�F����������ΌĂяo��
									if (FAILED(hr = CheckCapture(it->Tuner.GUID, it->Tuner.FriendlyName, it2->GUID, it2->FriendlyName))) {
										// �ŗLDll���_���ƌ����Ă���̂Ŏ��̃L���v�`���f�o�C�X��
										OutputDebug(L"[T->C] Discarded by BDASpecial's CheckCapture function.\n");
									}
									else {
										// �ŗLDll�̊m�FOK
										CComPtr<IBaseFilter> pCaptureDevice;
										// �L���v�`���f�o�C�X�̃t�B���^���擾
										if (!m_pDSFilterEnumCapture || FAILED(hr = m_pDSFilterEnumCapture->getFilter(&pCaptureDevice, it2->Order))) {
											// �t�B���^���擾�ł��Ȃ������̂Ŏ��̃L���v�`���f�o�C�X��
											OutputDebug(L"[T->C] Fail to get CaptureDevice IBaseFilter interface.\n");
										}
										else {
											// �t�B���^�擾����
											// Graph Builder�ɃL���v�`���f�o�C�X�̃t�B���^��ǉ�
											if (FAILED(hr = m_pIGraphBuilder->AddFilter(pCaptureDevice, it2->FriendlyName.c_str()))) {
												// �t�B���^�̒ǉ��Ɏ��s�����̂Ŏ��̃L���v�`���f�o�C�X��
												OutputDebug(L"[T->C] Fail to add CaptureDevice IBaseFilter into graph.\n");
											}
											else {
												// �t�B���^�ǉ�����
												// �L���v�`���f�o�C�X��connect ���Ă݂�
												if (FAILED(hr = Connect(pTunerDevice, pCaptureDevice))) {
													// connect�Ɏ��s�����̂Ŏ��̃L���v�`���f�o�C�X��
													OutputDebug(L"[T->C] Fail to connect.\n");
												}
												else {
													// connect ����
													OutputDebug(L"[T->C] Connect OK.\n");
													// TsWriter�ȍ~�Ɛڑ��`Run
													if (SUCCEEDED(LoadAndConnectMiscFilters(pTunerDevice, pCaptureDevice))) {
														// ���ׂĐ���
														m_pTunerDevice = pTunerDevice;
														m_pCaptureDevice = pCaptureDevice;
														// �`���[�i�ŗL�֐��̃��[�h
														LoadTunerDependCode(it->Tuner.GUID, it->Tuner.FriendlyName, it2->GUID, it2->FriendlyName);
														if (m_bTryAnotherTuner)
															// ����̑g�������`���[�i�E�L���v�`�����X�g�̍Ō���Ɉړ�
															m_UsableTunerCaptureList.splice(m_UsableTunerCaptureList.end(), m_UsableTunerCaptureList, it);
														return S_OK;
													}
													// �L���v�`���f�o�C�X��disconnect
													// DisconnectAll(pCaptureDevice);
												}
												// Graph Builder����L���v�`���f�o�C�X��remove
												m_pIGraphBuilder->RemoveFilter(pCaptureDevice);
											}
										}
									}
									// ���̃L���v�`���f�o�C�X�փ��[�v
								}
								// �L���v�`���f�o�C�X���[�v�I���
								// ���삷��g������������Ȃ������̂Ŏ��̃`���[�i��
							}
							else {
								// �L���v�`���f�o�C�X���g�p���Ȃ��ꍇ
								// TsWriter�ȍ~�Ɛڑ��`Run
								if (SUCCEEDED(hr = LoadAndConnectMiscFilters(pTunerDevice, NULL))) {
									// ���ׂĐ���
									m_pTunerDevice = pTunerDevice;
									// �`���[�i�ŗL�֐��̃��[�h
									LoadTunerDependCode(it->Tuner.GUID, it->Tuner.FriendlyName, L"", L"");
									if (m_bTryAnotherTuner)
										// ����̑g�������`���[�i�E�L���v�`�����X�g�̍Ō���Ɉړ�
										m_UsableTunerCaptureList.splice(m_UsableTunerCaptureList.end(), m_UsableTunerCaptureList, it);
									return S_OK;
								}
							}
							// �`���[�i�ŗL�֐���Dll���
							ReleaseTunerDependCode();
						}
						// Graph Builder����`���[�i��remove
						// DisconnectAll(pTunerDevice);
					}
					// Graph Builder����`���[�i��remove
					m_pIGraphBuilder->RemoveFilter(pTunerDevice);
				}
			}
			// �r�������I��
			::ReleaseSemaphore(m_hSemaphore, 1, NULL);
		}
		// �Z�}�t�H���
		SAFE_CLOSE_HANDLE(m_hSemaphore);
		// ���̃`���[�i�f�o�C�X�փ��[�v
	}
	// �`���[�i�f�o�C�X���[�v�I���
	// ���삷��g�ݍ��킹��������Ȃ�����
	OutputDebug(L"[P->T] Can not found a connectable pair of TunerDevice and CaptureDevice.\n");
	return E_FAIL;
}

void CBonTuner::UnloadTunerDevice(void)
{
	HRESULT hr;

	ReleaseTunerDependCode();

	if (m_pIGraphBuilder && m_pTunerDevice)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTunerDevice);

	m_pTunerDevice.Release();
}

void CBonTuner::UnloadCaptureDevice(void)
{
	HRESULT hr;

	if (m_pIGraphBuilder && m_pCaptureDevice)
		hr = m_pIGraphBuilder->RemoveFilter(m_pCaptureDevice);

	m_pCaptureDevice.Release();
}

HRESULT CBonTuner::LoadAndConnectMiscFilters(IBaseFilter* pTunerDevice, IBaseFilter* pCaptureDevice)
{
	HRESULT hr = E_FAIL;

	// TsWriter�Ɛڑ�
	if (SUCCEEDED(hr = LoadAndConnectTsWriter(pTunerDevice, pCaptureDevice))) {
		// TsDemuxer�Ɛڑ�
		if (SUCCEEDED(hr = LoadAndConnectDemux())) {
			// TIF�Ɛڑ�
			if (SUCCEEDED(hr = LoadAndConnectTif())) {
				// Run���Ă݂�
				if (SUCCEEDED(hr = RunGraph())) {
					// ����
					OutputDebug(L"RunGraph OK.\n");
					return hr;
				}
				OutputDebug(L"RunGraph Failed.\n");
				// DisconnectAll(m_pTif);
				UnloadTif();
			}
			// DisconnectAll(m_pDemux);
			UnloadDemux();
		}
		// DisconnectAll(m_pTsWriter);
		UnloadTsWriter();
	}

	// ���s
	return hr;
}

HRESULT CBonTuner::LoadAndConnectTsWriter(IBaseFilter* pTunerDevice, IBaseFilter* pCaptureDevice)
{
	// TS Writer�̖���:AddFilter���ɓo�^���閼�O
	static constexpr WCHAR * const FILTER_GRAPH_NAME_TSWRITER = L"TS Writer";

	HRESULT hr = E_FAIL;

	if (!pTunerDevice || (!pCaptureDevice && !m_aTunerParam.bNotExistCaptureDevice)) {
		OutputDebug(L"[C->W/T->W] TunerDevice or CaptureDevice NOT SET.\n");
		return E_POINTER;
	}

	// �t�B���^�N���X�̃��[�h
	CTsWriter *pCTsWriter = (CTsWriter *)CTsWriter::CreateInstance(NULL, &hr);
	if (!pCTsWriter) {
		OutputDebug(L"[C->W/T->W] Fail to create CTsWriter filter class instance.\n");
		hr = E_FAIL;
	}
	else {
		// �t�B���^���擾
		CComQIPtr<IBaseFilter> pTsWriter(pCTsWriter);
		if (!pTsWriter) {
			OutputDebug(L"[C->W/T->W] Fail to get TsWriter IBaseFilter interface.\n");
			hr = E_FAIL;
		}
		else {
			// �t�B���^�擾����
			OutputDebug(L"[C->W/T->W] %s is loaded.\n", FILTER_GRAPH_NAME_TSWRITER);
			// Graph Builder�Ƀt�B���^��ǉ�
			if (FAILED(hr = m_pIGraphBuilder->AddFilter(pTsWriter, FILTER_GRAPH_NAME_TSWRITER))) {
				OutputDebug(L"[C->W/T->W] Fail to add TsWriter IBaseFilter into graph.\n");
			}
			else {
				// �t�B���^�ǉ�����
				// ITsWriter interface���擾
				CComQIPtr<ITsWriter> pITsWriter(pTsWriter);
				if (!pITsWriter) {
					OutputDebug(L"[C->W/T->W] Fail to get ITsWriter interface.\n");
					hr = E_FAIL;
				}
				else {
					// ITsWriter interface�̎擾����
					// connect ���Ă݂�
					if (m_aTunerParam.bNotExistCaptureDevice) {
						// Capture�f�o�C�X�����݂��Ȃ��ꍇ��Tuner�Ɛڑ�
						if (FAILED(hr = Connect(pTunerDevice, pTsWriter))) {
							OutputDebug(L"[T->W] Failed to connect.\n");
						}
					}
					else {
						// Capture�f�o�C�X�Ɛڑ�
						if (FAILED(hr = Connect(pCaptureDevice, pTsWriter))) {
							OutputDebug(L"[C->W] Fail to connect.\n");
						}
					}
					if (SUCCEEDED(hr)) {
						// connect �����Ȃ̂ł��̂܂܏I��
						OutputDebug(L"[C->W/T->W] Connect OK.\n");
						m_pTsWriter = pTsWriter;
						m_pITsWriter = pITsWriter;
						return hr;
					}
				}
				m_pIGraphBuilder->RemoveFilter(pTsWriter);
			}
		}
	}

	// ���s
	return hr;
}

void CBonTuner::UnloadTsWriter(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pTsWriter)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTsWriter);

	m_pITsWriter.Release();
	m_pTsWriter.Release();
}

HRESULT CBonTuner::LoadAndConnectDemux(void)
{
	HRESULT hr = E_FAIL;

	if (!m_pTsWriter) {
			OutputDebug(L"[W->M] TsWriter NOT SET.\n");
			return E_POINTER;
	}

	CComPtr<IBaseFilter> pDemux;
	// �t�B���^���擾
	if (FAILED(hr = pDemux.CoCreateInstance(CLSID_MPEG2Demultiplexer, NULL, CLSCTX_INPROC_SERVER))) {
		OutputDebug(L"[W->M] Fail to get MPEG2Demultiplexer IBaseFilter interface.\n");
	}
	else {
		std::wstring strName = CDSFilterEnum::getRegistryName(pDemux);
		OutputDebug(L"[W->M] %s is loaded.\n", strName.c_str());
		// �t�B���^�擾����
		// Graph Builder�Ƀt�B���^��ǉ�
		if (FAILED(hr = m_pIGraphBuilder->AddFilter(pDemux, strName.c_str()))) {
			OutputDebug(L"[W->M] Fail to add MPEG2Demultiplexer IBaseFilter into graph.\n");
		}
		else {
			// �t�B���^�ǉ�����
			// connect ���Ă݂�
			if (FAILED(hr = Connect(m_pTsWriter, pDemux))) {
				OutputDebug(L"[W->M] Fail to connect.\n");
			}
			else {
				// connect �����Ȃ̂ł��̂܂܏I��
				OutputDebug(L"[W->M] Connect OK.\n");
				m_pDemux = pDemux;
				return hr;
			}
			m_pIGraphBuilder->RemoveFilter(pDemux);
		}
	}

	// ���s
	return hr;
}

void CBonTuner::UnloadDemux(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pDemux)
		hr = m_pIGraphBuilder->RemoveFilter(m_pDemux);

	m_pDemux.Release();
}

HRESULT CBonTuner::LoadAndConnectTif(void)
{
	// MPEG2 TIF��CLSID
	static constexpr CLSID CLSID_MPEG2TransportInformationFilter = { 0xfc772ab0, 0x0c7f, 0x11d3, 0x8f, 0xf2, 0x00, 0xa0, 0xc9, 0x22, 0x4c, 0xf4 };

	HRESULT hr = E_FAIL;

	if (!m_pDemux) {
			OutputDebug(L"[M->I] MPEG2Demultiplexer NOT SET.\n");
			return E_POINTER;
	}

	CComPtr<IBaseFilter> pTif;
	// �t�B���^���擾
	if (FAILED(hr = pTif.CoCreateInstance(CLSID_MPEG2TransportInformationFilter, NULL, CLSCTX_INPROC_SERVER))) {
		OutputDebug(L"[M->I] Fail to get TIF IBaseFilter interface.\n");
	}
	else {
		std::wstring strName = CDSFilterEnum::getRegistryName(pTif);
		OutputDebug(L"[M->I] %s is loaded.\n", strName.c_str());
		// �t�B���^�擾����
		// Graph Builder�Ƀt�B���^��ǉ�
		if (FAILED(hr = m_pIGraphBuilder->AddFilter(pTif, strName.c_str()))) {
			OutputDebug(L"[M->I] Fail to add TIF IBaseFilter into graph.\n");
		}
		else {
			// �t�B���^�ǉ�����
			// connect ���Ă݂�
			if (FAILED(hr = Connect(m_pDemux, pTif))) {
				OutputDebug(L"[M->I] Fail to connect.\n");
			}
			else {
				// connect �����Ȃ̂ł��̂܂܏I��
				OutputDebug(L"[M->I] Connect OK.\n");
				m_pTif = pTif;
				return hr;
			}
			m_pIGraphBuilder->RemoveFilter(pTif);
		}
	}

	// ���s
	return hr;
}

void CBonTuner::UnloadTif(void)
{
	HRESULT hr;
	if (m_pIGraphBuilder && m_pTif)
		hr = m_pIGraphBuilder->RemoveFilter(m_pTif);

	m_pTif.Release();
}

HRESULT CBonTuner::LoadTunerSignalStatisticsTunerNode(void)
{
	HRESULT hr = E_FAIL;

	if (!m_pTunerDevice) {
		OutputDebug(L"[LoadTunerSignalStatisticsTunerNode] TunerDevice NOT SET.\n");
		return E_POINTER;
	}

	CDSEnumNodes DSEnumNodes(m_pTunerDevice);
	CComPtr<IUnknown> pControlNode;
	if (FAILED(hr = DSEnumNodes.getControlNode(__uuidof(IBDA_FrequencyFilter), &pControlNode))) {
		OutputDebug(L"[LoadTunerSignalStatisticsTunerNode] Fail to get control node.\n");
		return E_FAIL;
	}

	CComQIPtr<IBDA_SignalStatistics> pIBDA_SignalStatistics(pControlNode);
	if (!pIBDA_SignalStatistics) {
		OutputDebug(L"[LoadTunerSignalStatisticsTunerNode] Fail to get IBDA_SignalStatistics interface.\n");
		return E_FAIL;
	}

	OutputDebug(L"[LoadTunerSignalStatisticsTunerNode] SUCCESS.\n");
	m_pIBDA_SignalStatisticsTunerNode = pIBDA_SignalStatistics;

	return S_OK;
}

HRESULT CBonTuner::LoadTunerSignalStatisticsDemodNode(void)
{
	HRESULT hr = E_FAIL;

	if (!m_pTunerDevice) {
		OutputDebug(L"[LoadTunerSignalStatisticsDemodNode] TunerDevice NOT SET.\n");
		return E_POINTER;
	}

	CDSEnumNodes DSEnumNodes(m_pTunerDevice);
	CComPtr<IUnknown> pControlNode;
	if (FAILED(hr = DSEnumNodes.getControlNode(__uuidof(IBDA_DigitalDemodulator), &pControlNode))) {
		OutputDebug(L"[LoadTunerSignalStatisticsDemodNode] Fail to get control node.\n");
		return E_FAIL;
	}

	CComQIPtr<IBDA_SignalStatistics> pIBDA_SignalStatistics(pControlNode);
	if (!pIBDA_SignalStatistics) {
		OutputDebug(L"[LoadTunerSignalStatisticsDemodNode] Fail to get IBDA_SignalStatistics interface.\n");
		return E_FAIL;
	}

	OutputDebug(L"[LoadTunerSignalStatisticsDemodNode] SUCCESS.\n");
	m_pIBDA_SignalStatisticsDemodNode = pIBDA_SignalStatistics;

	return S_OK;
}

void CBonTuner::UnloadTunerSignalStatistics(void)
{
	m_pIBDA_SignalStatisticsTunerNode.Release();
	m_pIBDA_SignalStatisticsDemodNode.Release();
}

// Connect pins (Common subroutine)
//  �S�Ẵs����ڑ����Đ���������I��
//
HRESULT CBonTuner::Connect(IBaseFilter* pFilterUp, IBaseFilter* pFilterDown)
{
	HRESULT hr;

	CDSEnumPins DSEnumPinsUp(pFilterUp);
	CDSEnumPins DSEnumPinsDown(pFilterDown);

	// �㗬�t�B���^��Output�s���̐��������[�v
	while (1) {
		CComPtr<IPin> pIPinUp;
		if (S_OK != (hr = DSEnumPinsUp.getNextPin(&pIPinUp, PIN_DIRECTION::PINDIR_OUTPUT))) {
			// ���[�v�I���
			break;
		}
		do {
			CComPtr<IPin> pIPinPeerOfUp;
			// �㗬�t�B���^�̒��ڃs�����ڑ���or�G���[�������玟�̏㗬�s����
			if (pIPinUp->ConnectedTo(&pIPinPeerOfUp) != VFW_E_NOT_CONNECTED){
				OutputDebug(L"  An already connected pin was found.\n");
				break;
			}

			// �����t�B���^��Input�s���̐��������[�v
			DSEnumPinsDown.Reset();
			while (1) {
				CComPtr<IPin> pIPinDown;
				if (S_OK != (hr = DSEnumPinsDown.getNextPin(&pIPinDown, PIN_DIRECTION::PINDIR_INPUT))) {
					// ���[�v�I���
					break;
				}
				do {
					CComPtr<IPin> pIPinPeerOfDown;
					// �����t�B���^�̒��ڃs�����ڑ���or�G���[�������玟�̉����s����
					if (pIPinDown->ConnectedTo(&pIPinPeerOfDown) != VFW_E_NOT_CONNECTED) {
						OutputDebug(L"  An already connected pin was found.\n");
						break;
					}

					// �ڑ������݂�
					if (SUCCEEDED(hr = m_pIGraphBuilder->ConnectDirect(pIPinUp, pIPinDown, NULL))) {
						// �ڑ�����
						return hr;
					} else {
						// �Ⴄ�`���[�i���j�b�g�̃t�B���^��ڑ����悤�Ƃ��Ă�ꍇ�Ȃ�
						// �R�l�N�g�ł��Ȃ��ꍇ�A���̉����s����
						OutputDebug(L"  A pair of pins that can not be connected was found.\n");
					}
				} while(0);
			} // while; ���̉����s����
		} while (0);
	} // while ; ���̏㗬�s����

	// �R�l�N�g�\�ȑg�ݍ��킹��������Ȃ�����
	OutputDebug(L"  Can not found a pair of connectable pins.\n");
	return E_FAIL;
}

void CBonTuner::DisconnectAll(IBaseFilter* pFilter)
{
	if (!m_pIGraphBuilder || !pFilter)
		return;
	
	HRESULT hr;

	CDSEnumPins DSEnumPins(pFilter);
	// �s���̐��������[�v
	while (1) {
		CComPtr<IPin> pIPin;
		CComPtr<IPin> pIPinPeerOf;
		if (S_OK != (hr = DSEnumPins.getNextPin(&pIPin))) {
			// ���[�v�I���
			break;
		}
		// �s�����ڑ��ς�������ؒf
		if (SUCCEEDED(hr = pIPin->ConnectedTo(&pIPinPeerOf))) {
			hr = m_pIGraphBuilder->Disconnect(pIPinPeerOf);
			hr = m_pIGraphBuilder->Disconnect(pIPin);
		}
	}
}
