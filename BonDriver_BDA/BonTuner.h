//------------------------------------------------------------------------------
// File: BonTuner.h
//   Define CBonTuner class
//------------------------------------------------------------------------------
#pragma once

#include "common.h"

#include <Windows.h>
#include <string>
#include <list>
#include <vector>
#include <map>

#include "TS_BUFF.h"
#include "IBdaSpecials2.h"
#include "IBonDriver2.h"
#include "LockChannel.h"
#include "DSFilterEnum.h"
#include "TSMF.h"

#pragma warning (push)
#pragma warning (disable: 4310)
#include "..\3rdParties\muparser\include\muParser.h"
#pragma warning (pop)

#pragma comment(lib, "muparser.lib")

struct ITsWriter;

// CBonTuner class
////////////////////////////////
class CBonTuner : public IBonDriver2
{
public:
	////////////////////////////////////////
	// �R���X�g���N�^ & �f�X�g���N�^
	////////////////////////////////////////
	CBonTuner();
	virtual ~CBonTuner();

	////////////////////////////////////////
	// IBonDriver �����o�֐�
	////////////////////////////////////////
	const BOOL OpenTuner(void);
	void CloseTuner(void);

	const BOOL SetChannel(const BYTE byCh);
	const float GetSignalLevel(void);

	const DWORD WaitTsStream(const DWORD dwTimeOut = 0);
	const DWORD GetReadyCount(void);

	const BOOL GetTsStream(BYTE *pDst, DWORD *pdwSize, DWORD *pdwRemain);
	const BOOL GetTsStream(BYTE **ppDst, DWORD *pdwSize, DWORD *pdwRemain);

	void PurgeTsStream(void);

	////////////////////////////////////////
	// IBonDriver2 �����o�֐�
	////////////////////////////////////////
	LPCTSTR GetTunerName(void);

	const BOOL IsTunerOpening(void);

	LPCTSTR EnumTuningSpace(const DWORD dwSpace);
	LPCTSTR EnumChannelName(const DWORD dwSpace, const DWORD dwChannel);

	const BOOL SetChannel(const DWORD dwSpace, const DWORD dwChannel);

	const DWORD GetCurSpace(void);
	const DWORD GetCurChannel(void);

	void Release(void);
	
	////////////////////////////////////////
	// �ÓI�����o�֐�
	////////////////////////////////////////

	// �K�v�ȐÓI�ϐ�������
	static void Init(HMODULE hModule);

	// �ÓI�ϐ��̉��
	static void Finalize(void);

	////////////////////////////////////////
	// �ÓI�����o�ϐ�
	////////////////////////////////////////

	// Dll�̃��W���[���n���h��
	static HMODULE st_hModule;

	// �쐬���ꂽCBontuner�C���X�^���X�̈ꗗ
	static std::list<CBonTuner*> st_InstanceList;

	// st_InstanceList����p
	static CRITICAL_SECTION st_LockInstanceList;

protected:
	////////////////////////////////////////
	// ���������o�֐�
	////////////////////////////////////////

	// COM������p�X���b�h
	static DWORD WINAPI COMProcThread(LPVOID lpParameter);

	// Decode������p�X���b�h
	static DWORD WINAPI DecodeProcThread(LPVOID lpParameter);

	// TsWriter �R�[���o�b�N�֐�
	static int CALLBACK RecvProc(void* pParam, BYTE* pbData, size_t size);

	// �f�[�^��M�X�^�[�g�E��~
	void StartRecv(void);
	void StopRecv(void);

	// ini �t�@�C���Ǎ�
	void ReadIniFile(void);

	// �d���v�����ύX�p
	void PowerSetOnOpened(void);
	void PowerSetOnClosing(void);

	// �M����Ԃ��擾
	void GetSignalState(int* pnStrength, int* pnQuality, int* pnLock);

	// �`�����l���ؑ�
	BOOL LockChannel(const TuningParam *pTuningParam, BOOL bLockTwice);

	// �`���[�i�ŗLDll�̃��[�h
	HRESULT CheckAndInitTunerDependDll(IBaseFilter * pTunerDevice, std::wstring tunerGUID, std::wstring tunerFriendlyName);

	// �`���[�i�ŗLDll�ł̃L���v�`���f�o�C�X�m�F
	HRESULT CheckCapture(std::wstring tunerGUID, std::wstring tunerFriendlyName, std::wstring captureGUID, std::wstring captureFriendlyName);
		
	// �`���[�i�ŗL�֐��̃��[�h
	void LoadTunerDependCode(std::wstring tunerGUID, std::wstring tunerFriendlyName, std::wstring captureGUID, std::wstring captureFriendlyName);

	// �`���[�i�ŗL�֐���Dll�̉��
	void ReleaseTunerDependCode(void);

	// GraphBuilder
	HRESULT InitializeGraphBuilder(void);
	void CleanupGraph(void);
	HRESULT RunGraph(void);
	void StopGraph(void);

	// TuningSpace
	HRESULT CreateTuningSpace(void);
	void UnloadTuningSpace(void);
	HRESULT InitTuningSpace(void);

	// NetworkProvider
	HRESULT LoadNetworkProvider(void);
	void UnloadNetworkProvider(void);

	// �`���[�i�E�L���v�`���f�o�C�X�̓Ǎ��݃��X�g�擾
	HRESULT InitDSFilterEnum(void);

	// �`���[�i�E�L���v�`���f�o�C�X���܂߂Ă��ׂẴt�B���^�O���t�����[�h����Run�����݂�
	HRESULT LoadAndConnectDevice(void);

	// TunerDevice
	void UnloadTunerDevice(void);
	
	// CaptureDevice
	void UnloadCaptureDevice(void);
	
	// TsWriter
	HRESULT LoadAndConnectTsWriter(IBaseFilter* pTunerDevice, IBaseFilter* pCaptureDevice);
	void UnloadTsWriter(void);

	// Demultiplexer
	HRESULT LoadAndConnectDemux(void);
	void UnloadDemux(void);
	
	// TIF (Transport Information Filter)
	HRESULT LoadAndConnectTif(void);
	void UnloadTif(void);

	// TsWriter/Demultiplexer/TIF��Load&Connect��Run����
	HRESULT LoadAndConnectMiscFilters(IBaseFilter* pTunerDevice, IBaseFilter* pCaptureDevice);

	// �`���[�i�M����Ԏ擾�p�C���^�[�t�F�[�X
	HRESULT LoadTunerSignalStatisticsTunerNode(void);
	HRESULT LoadTunerSignalStatisticsDemodNode(void);
	void UnloadTunerSignalStatistics(void);

	// Pin �̐ڑ�
	HRESULT Connect(IBaseFilter* pFrom, IBaseFilter* pTo);

	// �S�Ă� Pin ��ؒf����
	void DisconnectAll(IBaseFilter* pFilter);

	// CCOM������p�X���b�h����Ăяo�����֐�
	const BOOL _OpenTuner(void);
	void _CloseTuner(void);
	const BOOL _SetChannel(const DWORD dwSpace, const DWORD dwChannel);
	const float _GetSignalLevel(void);
	const BOOL _IsTunerOpening(void);
	const DWORD _GetCurSpace(void);
	const DWORD _GetCurChannel(void);


protected:
	////////////////////////////////////////
	// �����o�ϐ�
	////////////////////////////////////////

	////////////////////////////////////////
	// COM������p�X���b�h�p
	////////////////////////////////////////

	enum enumCOMRequest {
		eCOMReqOpenTuner = 1,
		eCOMReqCloseTuner,
		eCOMReqSetChannel,
		eCOMReqGetSignalLevel,
		eCOMReqIsTunerOpening,
		eCOMReqGetCurSpace,
		eCOMReqGetCurChannel,
	};

	struct COMReqParamSetChannel {
		DWORD dwSpace;
		DWORD dwChannel;
	};

	union COMReqParm {
		COMReqParamSetChannel SetChannel;
	};

	union COMReqRetVal {
		BOOL OpenTuner;
		BOOL SetChannel;
		float GetSignalLevel;
		BOOL IsTunerOpening;
		DWORD GetCurSpace;
		DWORD GetCurChannel;
	};

	struct COMProc {
		HANDLE hThread;					// �X���b�h�n���h��
		HANDLE hReqEvent;				// COMProc�X���b�h�ւ̃R�}���h���s�v��
		HANDLE hEndEvent;				// COMProc�X���b�h����̃R�}���h�����ʒm
		CRITICAL_SECTION csLock;		// �r���p
		enumCOMRequest nRequest;		// ���N�G�X�g
		COMReqParm uParam;				// �p�����[�^
		COMReqRetVal uRetVal;			// �߂�l
		DWORD dwTick;					// ���݂�TickCount
		DWORD dwTickLastCheck;			// �Ō�Ɉُ�Ď��̊m�F���s����TickCount
		DWORD dwTickSignalLockErr;		// SignalLock�ُ̈픭��TickCount
		DWORD dwTickBitRateErr;			// BitRate�ُ̈픭��TckCount
		BOOL bSignalLockErr;			// SignalLock�ُ̈픭����Flag
		BOOL bBitRateErr;				// BitRate�ُ̈픭����Flag
		BOOL bDoReLockChannel;			// �`�����l�����b�N�Ď��s��
		BOOL bDoReOpenTuner;			// �`���[�i�[�ăI�[�v����
		unsigned int nReLockFailCount;	// Re-LockChannel���s��
		DWORD dwReOpenSpace;			// �`���[�i�[�ăI�[�v�����̃J�����g�`���[�j���O�X�y�[�X�ԍ��ޔ�
		DWORD dwReOpenChannel;			// �`���[�i�[�ăI�[�v�����̃J�����g�`�����l���ԍ��ޔ�
		HANDLE hTerminateRequest;		// �X���b�h�I���v��
		
		COMProc(void)
			: hThread(NULL),
			  hReqEvent(NULL),
			  hEndEvent(NULL),
			  hTerminateRequest(NULL),
			  dwTick(0),
			  dwTickLastCheck(0),
			  dwTickSignalLockErr(0),
			  dwTickBitRateErr(0),
			  bSignalLockErr(FALSE),
			  bBitRateErr(FALSE),
			  bDoReLockChannel(FALSE),
			  bDoReOpenTuner(FALSE),
			  dwReOpenSpace(CBonTuner::SPACE_INVALID),
			  dwReOpenChannel(CBonTuner::CHANNEL_INVALID),
			  nReLockFailCount(0)
		{
			hReqEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			hEndEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			hTerminateRequest = ::CreateEvent(NULL, FALSE, FALSE, NULL);
			::InitializeCriticalSection(&csLock);
		}
		
		~COMProc(void)
		{
			SAFE_CLOSE_HANDLE(hReqEvent);
			SAFE_CLOSE_HANDLE(hEndEvent);
			SAFE_CLOSE_HANDLE(hTerminateRequest);
			::DeleteCriticalSection(&csLock);
		}
		
		inline BOOL CheckTick(void)
		{
			dwTick = ::GetTickCount();
			if (dwTick - dwTickLastCheck > 1000) {
				dwTickLastCheck = dwTick;
				return TRUE;
			}
			return FALSE;
		}
		
		inline void ResetWatchDog(void)
		{
			bSignalLockErr = FALSE;
			bBitRateErr = FALSE;
		}

		inline BOOL CheckSignalLockErr(BOOL state, DWORD threshold)
		{
			if (state) {
				//����
				bSignalLockErr = FALSE;
			} else {
				// �ُ�
				if (!bSignalLockErr) {
					// ���񔭐�
					bSignalLockErr = TRUE;
					dwTickSignalLockErr = dwTick;
				}
				else {
					// �O��ȑO�ɔ������Ă���
					if ((dwTick - dwTickSignalLockErr) > threshold) {
						// �ݒ莞�Ԉȏ�o�߂��Ă���
						ResetWatchDog();
						return TRUE;
					}
				}
			}
			return FALSE;
		}
		
		inline BOOL CheckBitRateErr(BOOL state, DWORD threshold)
		{
			if (state) {
				//����
				bSignalLockErr = FALSE;
			}
			else {
				// �ُ�
				if (!bBitRateErr) {
					// ���񔭐�
					bBitRateErr = TRUE;
					dwTickBitRateErr = dwTick;
				}
				else {
					// �O��ȑO�ɔ������Ă���
					if ((dwTick - dwTickBitRateErr) > threshold) {
						// �ݒ莞�Ԉȏ�o�߂��Ă���
						ResetWatchDog();
						return TRUE;
					}
				}
			}
			return FALSE;
		}

		inline void SetReLockChannel(void)
		{
			bDoReLockChannel = TRUE;
			nReLockFailCount = 0;
		}
		
		inline void ResetReLockChannel(void)
		{
			bDoReLockChannel = FALSE;
			nReLockFailCount = 0;
		}
		
		inline BOOL CheckReLockFailCount(unsigned int threshold)
		{
			return (++nReLockFailCount >= threshold);
		}
		
		inline void SetReOpenTuner(DWORD space, DWORD channel)
		{
			bDoReOpenTuner = TRUE;
			dwReOpenSpace = space;
			dwReOpenChannel = channel;
		}
		
		inline void ClearReOpenChannel(void)
		{
			dwReOpenSpace = CBonTuner::SPACE_INVALID;
			dwReOpenChannel = CBonTuner::CHANNEL_INVALID;
		}
		
		inline BOOL CheckReOpenChannel(void)
		{
			return (dwReOpenSpace != CBonTuner::SPACE_INVALID && dwReOpenChannel != CBonTuner::CHANNEL_INVALID);
		}

		inline void ResetReOpenTuner(void)
		{
			bDoReOpenTuner = FALSE;
			ClearReOpenChannel();
		}
	};
	COMProc m_aCOMProc;

	////////////////////////////////////////
	// Decode������p�X���b�h�p
	////////////////////////////////////////

	struct DecodeProc {
		HANDLE hThread;					// �X���b�h�n���h��
		HANDLE hTerminateRequest;		// �X���b�h�I���v��
		DecodeProc(void)
			: hThread(NULL),
			hTerminateRequest(NULL)
		{
			hTerminateRequest = ::CreateEvent(NULL, FALSE, FALSE, NULL);
		}
		~DecodeProc(void)
		{
			SAFE_CLOSE_HANDLE(hTerminateRequest);
		}
	};
	DecodeProc m_aDecodeProc;

	////////////////////////////////////////
	// �`���[�i�p�����[�^�֌W
	////////////////////////////////////////

	// INI�t�@�C���Ŏw��ł���GUID/FriendlyName�ő吔
	static constexpr unsigned int MAX_GUID = 100U;

	// �`���[�i�E�L���v�`�������Ɏg�p����GUID�������FriendlyName������̑g����
	struct TunerSearchData {
		std::wstring TunerGUID;
		std::wstring TunerFriendlyName;
		std::wstring CaptureGUID;
		std::wstring CaptureFriendlyName;
		TunerSearchData(void)
		{
		}
		TunerSearchData(std::wstring tunerGuid, std::wstring tunerFriendlyName, std::wstring captureGuid, std::wstring captureFriendlyName)
			: TunerFriendlyName(tunerFriendlyName),
			  CaptureFriendlyName(captureFriendlyName)
		{
			TunerGUID = common::WStringToLowerCase(tunerGuid);
			CaptureGUID = common::WStringToLowerCase(captureGuid);
		}
	};

	// INI �t�@�C���Ŏw�肷��`���[�i�p�����[�^
	struct TunerParam {
		std::map<unsigned int, TunerSearchData> Tuner;
												// Tuner��Capture��GUID/FriendlyName�w��
		BOOL bNotExistCaptureDevice;			// Tuner�f�o�C�X�݂̂�Capture�f�o�C�X�����݂��Ȃ��ꍇTRUE
		BOOL bCheckDeviceInstancePath;			// Tuner��Capture�̃f�o�C�X�C���X�^���X�p�X����v���Ă��邩�̊m�F���s�����ǂ���
		std::basic_string<TCHAR> sTunerName;	// GetTunerName�ŕԂ����O
		std::wstring sDLLBaseName;				// �ŗLDLL
		TunerParam(void)
			: bNotExistCaptureDevice(TRUE),
			  bCheckDeviceInstancePath(TRUE)
		{
		}
		~TunerParam(void)
		{
			Tuner.clear();
		}
	};
	TunerParam m_aTunerParam;

	// Tone�M���ؑ֎���Wait����
	unsigned int m_nToneWait;

	// CH�ؑ֌��Lock�m�F����
	unsigned int m_nLockWait;

	// CH�ؑ֌��Lock�m�FDelay����
	unsigned int m_nLockWaitDelay;

	// CH�ؑ֌��Lock�m�FRetry��
	unsigned int m_nLockWaitRetry;

	// CH�ؑ֓���������I��2�x�s�����ǂ���
	BOOL m_bLockTwice;

	// CH�ؑ֓���������I��2�x�s���ꍇ��Delay����(msec)
	unsigned int m_nLockTwiceDelay;

	// SignalLocked�̊Ď�����(msec) 0�ŊĎ����Ȃ�
	unsigned int m_nWatchDogSignalLocked;

	// BitRate�̊Ď�����(msec) 0�ŊĎ����Ȃ�
	unsigned int m_nWatchDogBitRate;

	// �ُ팟�m���A�`���[�i�̍ăI�[�v�������݂�܂ł�CH�ؑ֓��쎎�s��
	unsigned int m_nReOpenWhenGiveUpReLock;

	// �`���[�i�̍ăI�[�v�������݂�ꍇ�ɕʂ̃`���[�i��D�悵�Č������邩�ǂ���
	BOOL m_bTryAnotherTuner;

	// CH�ؑւɎ��s�����ꍇ�ɁA�ُ팟�m�����l�o�b�N�O�����h��CH�ؑ֓�����s�����ǂ���
	BOOL m_bBackgroundChannelLock;

	// SignalLevel �Z�o���@
	enum enumSignalLevelCalcType {
		eSignalLevelCalcTypeSSMin = 0,
		eSignalLevelCalcTypeSSStrength = 0,			// RF Tuner Node��IBDA_SignalStatistics����擾����Strength�l �� StrengthCoefficient �{ StrengthBias
		eSignalLevelCalcTypeSSQuality = 1,			// RF Tuner Node��IBDA_SignalStatistics����擾����Quality�l �� QualityCoefficient �{ QualityBias
		eSignalLevelCalcTypeSSMul = 2,				// RF Tuner Node��IBDA_SignalStatistics����擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �~ (Quality�l �� QualityCoefficient �{ QualityBias)
		eSignalLevelCalcTypeSSAdd = 3,				// RF Tuner Node��IBDA_SignalStatistics����擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �{ (Quality�l �� QualityCoefficient �{ QualityBias)
		eSignalLevelCalcTypeSSFormula = 9,			// RF Tuner Node��IBDA_SignalStatistics����擾����Strength/Quality�l��SignalLevelCalcFormula�ɐݒ肵�����[�U�[��`�����ŎZ�o
		eSignalLevelCalcTypeSSMax = 9,
		eSignalLevelCalcTypeTunerMin = 10,
		eSignalLevelCalcTypeTunerStrength = 10,		// ITuner::get_SignalStrength�Ŏ擾����Strength�l �� StrengthCoefficient �{ StrengthBias
		eSignalLevelCalcTypeTunerQuality = 11,		// ITuner::get_SignalStrength�Ŏ擾����Quality�l �� QualityCoefficient �{ QualityBias
		eSignalLevelCalcTypeTunerMul = 12,			// ITuner::get_SignalStrength�Ŏ擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �~ (Quality�l �� QualityCoefficient �{ QualityBias)
		eSignalLevelCalcTypeTunerAdd = 13,			// ITuner::get_SignalStrength�Ŏ擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �{ (Quality�l �� QualityCoefficient �{ QualityBias)
		eSignalLevelCalcTypeTunerFormula = 19,		// ITuner::get_SignalStrength�Ŏ擾����Strength/Quality�l��SignalLevelCalcFormula�ɐݒ肵�����[�U�[��`�����ŎZ�o
		eSignalLevelCalcTypeTunerMax = 19,
		eSignalLevelCalcTypeDemodSSMin = 20,
		eSignalLevelCalcTypeDemodSSStrength = 20,	// Demodulator Node��IBDA_SignalStatistics����擾����Strength�l �� StrengthCoefficient �{ StrengthBias
		eSignalLevelCalcTypeDemodSSQuality = 21,	// Demodulator Node��IBDA_SignalStatistics����擾����Quality�l �� QualityCoefficient �{ QualityBias
		eSignalLevelCalcTypeDemodSSMul = 22,		// Demodulator Node��IBDA_SignalStatistics����擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �~ (Quality�l �� QualityCoefficient �{ QualityBias)
		eSignalLevelCalcTypeDemodSSAdd = 23,		// Demodulator Node��IBDA_SignalStatistics����擾����(Strength�l �� StrengthCoefficient �{ StrengthBias) �{ (Quality�l �� QualityCoefficient �{ QualityBias)
		eSignalLevelCalcTypeDemodSSFormula = 29,	// Demodulator Node��IBDA_SignalStatistics����擾����Strength/Quality�l��SignalLevelCalcFormula�ɐݒ肵�����[�U�[��`�����ŎZ�o
		eSignalLevelCalcTypeDemodSSMax = 29,
		eSignalLevelCalcTypeBR = 100,				// �r�b�g���[�g�l(Mibps)
	};
	enumSignalLevelCalcType m_nSignalLevelCalcType;
	BOOL m_bSignalLevelGetTypeSS;			// SignalLevel �Z�o�� RF Tuner Node �� IBDA_SignalStatistics ���g�p����
	BOOL m_bSignalLevelGetTypeTuner;		// SignalLevel �Z�o�� ITuner ���g�p����
	BOOL m_bSignalLevelGetTypeDemodSS;		// SignalLevel �Z�o�� Demodulator Node �� IBDA_SignalStatistics ���g�p����
	BOOL m_bSignalLevelGetTypeBR;			// SignalLevel �Z�o�� �r�b�g���[�g�l���g�p����
	BOOL m_bSignalLevelNeedStrength;		// SignalLevel �Z�o�� SignalStrength �l���g�p����
	BOOL m_bSignalLevelNeedQuality;			// SignalLevel �Z�o�� SignalQuality �l���g�p����

	// Strength �l�␳�W��
	double m_fStrengthCoefficient;

	// Quality �l�␳�W��
	double m_fQualityCoefficient;

	// Strength �l�␳�o�C�A�X
	double m_fStrengthBias;

	// Quality �l�␳�o�C�A�X
	double m_fQualityBias;

	// SignalLevel�Z�o�p���[�U�[��`����
	std::wstring m_sSignalLevelCalcFormula;

	// SignalLevel�Z�o�p
	mu::Parser m_muParser;	// muparser
	double m_fStrength;		// muparser�pStrength�l�Q�ƕϐ�
	double m_fQuality;		// muparser�pQuality�l�Q�ƕϐ�

	// �`���[�j���O��Ԃ̔��f���@
	enum enumSignalLockedJudgeType {
		eSignalLockedJudgeTypeAlways = 0,	// ��Ƀ`���[�j���O�ɐ������Ă����ԂƂ��Ĕ��f����
		eSignalLockedJudgeTypeSS = 1,		// RF Tuner Node �� IBDA_SignalStatistics::get_SignalLocked�Ŏ擾�����l�Ŕ��f����
		eSignalLockedJudgeTypeTuner = 2,	// ITuner::get_SignalStrength�Ŏ擾�����l�Ŕ��f����
		eSignalLockedJudgeTypeDemodSS = 3,	// Demodulator Node �� IBDA_SignalStatistics::get_SignalLocked�Ŏ擾�����l�Ŕ��f����
	};
	enumSignalLockedJudgeType m_nSignalLockedJudgeType;
	BOOL m_bSignalLockedJudgeTypeSS;		// �`���[�j���O��Ԃ̔��f�� IBDA_SignalStatistics ���g�p����
	BOOL m_bSignalLockedJudgeTypeTuner;		// �`���[�j���O��Ԃ̔��f�� ITuner ���g�p����
	BOOL m_bSignalLockedJudgeTypeDemodSS;	// �`���[�j���O��Ԃ̔��f�� Demodulator Node �� IBDA_SignalStatistics ���g�p����

	////////////////////////////////////////
	// BonDriver �p�����[�^�֌W
	////////////////////////////////////////

	// �o�b�t�@1������̃T�C�Y
	size_t m_nBuffSize;

	// �ő�o�b�t�@��
	size_t m_nMaxBuffCount;

	// m_hOnDecodeEvent���Z�b�g����f�[�^�o�b�t�@��
	unsigned int m_nWaitTsCount;

	// WaitTsStream�ōŒ���ҋ@���鎞��
	unsigned int m_nWaitTsSleep;

	// �k���p�P�b�g���폜���邩�ǂ���
	BOOL m_bDeleteNullPackets;

	// SetChannel()�Ń`�����l�����b�N�Ɏ��s�����ꍇ�ł�FALSE��Ԃ��Ȃ��悤�ɂ��邩�ǂ���
	BOOL m_bAlwaysAnswerLocked;

	// COMProcThread�̃X���b�h�v���C�I���e�B
	int m_nThreadPriorityCOM;

	// DecodeProcThread�̃X���b�h�v���C�I���e�B
	int m_nThreadPriorityDecode;

	// �X�g���[���X���b�h�v���C�I���e�B
	int m_nThreadPriorityStream;

	// timeBeginPeriod()�Őݒ肷��Windows�̍ŏ��^�C�}����\(msec)
	unsigned int m_nPeriodicTimer;

	////////////////////////////////////////
	// �`�����l���p�����[�^
	////////////////////////////////////////

	// �`�����l���f�[�^
	struct ChData {
		std::basic_string<TCHAR> sServiceName;	// EnumChannelName�ŕԂ��`�����l����
		unsigned int Satellite;			// �q����M�ݒ�ԍ�
		unsigned int Polarisation;		// �Δg��ޔԍ� (0 .. ���w��, 1 .. H, 2 .. V, 3 .. L, 4 .. R)
		unsigned int ModulationType;	// �ϒ������ݒ�ԍ�
		long Frequency;					// ���g��(KHz)
		union {
			long SID;					// �T�[�r�XID
			long PhysicalChannel;		// ATSC / Digital Cable�p
		};
		union {
			long TSID;					// �g�����X�|�[�g�X�g���[��ID
			long Channel;				// ATSC / Digital Cable�p
		};
		union {
			long ONID;					// �I���W�i���l�b�g���[�NID
			long MinorChannel;			// ATSC / Digital Cable�p
		};
		long MajorChannel;				// Digital Cable�p
		long SourceID;					// Digital Cable�p
		BOOL LockTwiceTarget;			// CH�ؑ֓���������I��2�x�s���Ώ�
		ChData(void)
			: Satellite(0),
			  Polarisation(0),
			  ModulationType(0),
			  Frequency(0),
			  SID(-1),
			  TSID(-1),
			  ONID(-1),
			  MajorChannel(-1),
			  SourceID(-1),
			  LockTwiceTarget(FALSE)
		{
		};
	};

	// �`���[�j���O��ԃf�[�^
	struct TuningSpaceData {
		std::basic_string<TCHAR> sTuningSpaceName;		// EnumTuningSpace�ŕԂ�Tuning Space��
		long FrequencyOffset;							// ���g���I�t�Z�b�g�l
		unsigned int DVBSystemTypeNumber;				// TuningSpace�̎�ޔԍ�
		unsigned int TSMFMode;							// TSMF�̏������[�h
		std::map<unsigned int, ChData> Channels;		// �`�����l���ԍ��ƃ`�����l���f�[�^
		DWORD dwNumChannel;								// �`�����l����
		TuningSpaceData(void)
			: FrequencyOffset(0),
			  dwNumChannel(0)
		{
		};
		~TuningSpaceData(void)
		{
			Channels.clear();
		};
	};

	// �`���[�j���O�X�y�[�X�ꗗ
	struct TuningData {
		std::map<unsigned int, TuningSpaceData> Spaces;		// �`���[�j���O�X�y�[�X�ԍ��ƃf�[�^
		DWORD dwNumSpace;									// �`���[�j���O�X�y�[�X��
		TuningData(void)
			: dwNumSpace(0)
		{
		};
		~TuningData(void)
		{
			Spaces.clear();
		};
	};
	TuningData m_TuningData;

	////////////////////////////////////////
	// �q����M�p�����[�^
	////////////////////////////////////////

	// ini�t�@�C���Ŏ�t����Δg��ސ�
	static constexpr unsigned int POLARISATION_SIZE = 5U;

	// CBonTuner�Ŏg�p����Δg��ޔԍ���Polarisation�^��Mapping
	static constexpr Polarisation PolarisationMapping[POLARISATION_SIZE] = {
		BDA_POLARISATION_NOT_SET,
		BDA_POLARISATION_LINEAR_H,
		BDA_POLARISATION_LINEAR_V,
		BDA_POLARISATION_CIRCULAR_L,
		BDA_POLARISATION_CIRCULAR_R
	};

	// �Δg��ޖ���ini�t�@�C���ł̋L��
	static constexpr WCHAR PolarisationChar[POLARISATION_SIZE] = {
		L' ',
		L'H',
		L'V',
		L'L',
		L'R'
	};

	// ini�t�@�C���Őݒ�ł���ő�q���� + 1
	static constexpr unsigned int MAX_SATELLITE = 10U;

	// �q����M�ݒ�f�[�^
	struct Satellite {
		AntennaParam Polarisation[POLARISATION_SIZE];	// �Δg��ޖ��̃A���e�i�ݒ�
	};
	Satellite m_aSatellite[MAX_SATELLITE];

	// �`�����l�����̎��������Ɏg�p����q���̖���
	std::wstring m_sSatelliteName[MAX_SATELLITE];

	////////////////////////////////////////
	// �ϒ������p�����[�^
	////////////////////////////////////////

	// ini�t�@�C���Őݒ�ł���ő�ϒ�������
	static constexpr unsigned int MAX_MODULATION = 10U;

	// �ϒ������ݒ�f�[�^
	ModulationMethod m_aModulationType[MAX_MODULATION];

	// �`�����l�����̎��������Ɏg�p����ϒ������̖���
	std::wstring m_sModulationName[MAX_MODULATION];

	////////////////////////////////////////
	// BonDriver �֘A
	////////////////////////////////////////

	// ini�t�@�C����Path
	std::wstring m_sIniFilePath;

	// ��M�C�x���g
	HANDLE m_hOnStreamEvent;

	// �f�R�[�h�C�x���g
	HANDLE m_hOnDecodeEvent;

	// ��MTS�f�[�^�o�b�t�@
	TS_BUFF m_TsBuff;

	// Decode�����̏I�����TS�f�[�^�o�b�t�@
	TS_BUFF m_DecodedTsBuff;

	// GetTsStream�ŎQ�Ƃ����o�b�t�@
	TS_DATA* m_LastBuff;

	// �f�[�^��M��
	BOOL m_bRecvStarted;

	// �v���Z�X�n���h��
	HANDLE m_hProcess;

	// �X�g���[���X���b�h�̃n���h��
	HANDLE m_hStreamThread;

	// �X�g���[���X���b�h�n���h���ʒm�t���O
	BOOL m_bIsSetStreamThread;

	// �r�b�g���[�g�v�Z�p
	class BitRate {
	private:
		DWORD Rate1sec;					// 1�b�Ԃ̃��[�g���Z�p (bytes/sec)
		DWORD RateLast[5];				// ����5�b�Ԃ̃��[�g (bytes/sec)
		DWORD DataCount;				// ����5�b�Ԃ̃f�[�^�� (0�`5)
		double Rate;					// ���σr�b�g���[�g (Mibps)
		DWORD LastTick;					// �O���TickCount�l
		CRITICAL_SECTION csRate1Sec;	// nRate1sec �r���p
		CRITICAL_SECTION csRateLast;	// nRateLast �r���p

	public:
		BitRate(void)
			: Rate1sec(0),
			  RateLast(),
			  DataCount(0),
			  Rate(0.0)
		{
			::InitializeCriticalSection(&csRate1Sec);
			::InitializeCriticalSection(&csRateLast);
			LastTick = ::GetTickCount();
		};

		~BitRate(void)
		{
			::DeleteCriticalSection(&csRateLast);
			::DeleteCriticalSection(&csRate1Sec);
		}

		inline void AddRate(DWORD Count)
		{
			::EnterCriticalSection(&csRate1Sec);
			Rate1sec += Count;
			::LeaveCriticalSection(&csRate1Sec);
		}

		inline DWORD CheckRate(void)
		{
			DWORD total = 0;
			DWORD Tick = ::GetTickCount();
			if (Tick - LastTick > 1000) {
				::EnterCriticalSection(&csRateLast);
				for (unsigned int i = (sizeof(RateLast) / sizeof(RateLast[0])) - 1; i > 0; i--) {
					RateLast[i] = RateLast[i - 1];
					total += RateLast[i];
				}
				::EnterCriticalSection(&csRate1Sec);
				RateLast[0] = Rate1sec;
				Rate1sec = 0;
				::LeaveCriticalSection(&csRate1Sec);
				total += RateLast[0];
				if (DataCount < 5)
					DataCount++;
				if (DataCount)
					Rate = ((double)total / (double)DataCount) / 131072.0;
				LastTick = Tick;
				::LeaveCriticalSection(&csRateLast);
			}
			DWORD remain = 1000 - (Tick - LastTick);
			return (remain > 1000) ? 1000 : remain;
		}

		inline void Clear(void)
		{
			::EnterCriticalSection(&csRateLast);
			::EnterCriticalSection(&csRate1Sec);
			Rate1sec = 0;
			for (unsigned int i = 0; i < sizeof(RateLast) / sizeof(RateLast[0]); i++) {
				RateLast[i] = 0;
			}
			DataCount = 0;
			Rate = 0.0;
			LastTick = ::GetTickCount();
			::LeaveCriticalSection(&csRate1Sec);
			::LeaveCriticalSection(&csRateLast);
		}

		inline double GetRate(void)
		{
			return Rate;
		}
	};
	BitRate m_BitRate;

	// TSNF�����p
	CTSMFParser m_TSMFParser;

	////////////////////////////////////////
	// �`���[�i�֘A
	////////////////////////////////////////

	// �`���[�i�f�o�C�X�r�������p
	HANDLE m_hSemaphore;

	// �d���v�����ύX�p�̃~���[�e�b�N�X�I�u�W�F�N�g��
	// ���c�[���Ƌ�������ꍇ�ɔ����āA��ʓI�Ȗ��O"PowerSet�`"�Ƀ����_��GUID��t�����Ė���
	static constexpr WCHAR POWER_SET_FLAG_NAME[] = L"Global\\PowerSetFlag-D112FE9C-2CC3-4AEE-83E3-458C65CF592C";
	static constexpr WCHAR POWER_SET_LOCK_NAME[] = L"Global\\PowerSetLock-D112FE9C-2CC3-4AEE-83E3-458C65CF592C";
	static constexpr DWORD POWER_SET_WAIT_MSEC = 10000;

	// �d���v�����ύX�p
	HANDLE m_hPowerSetFlag;
	HANDLE m_hPowerSetLock;

	// Graph
	CComPtr<IGraphBuilder> m_pIGraphBuilder;	// Filter Graph Manager �� IGraphBuilder interface
	CComPtr<IMediaControl> m_pIMediaControl;	// Filter Graph Manager �� IMediaControl interface
	CComPtr<IBaseFilter> m_pNetworkProvider;	// NetworkProvider �� IBaseFilter interface
	CComPtr<ITuner> m_pITuner;					// NetworkProvider �� ITuner interface
	CComPtr<IBaseFilter> m_pTunerDevice;		// Tuner Device �� IBaseFilter interface
	CComPtr<IBaseFilter> m_pCaptureDevice;		// Capture Device �� IBaseFilter interface
	CComPtr<IBaseFilter> m_pTsWriter;			// CTsWriter �� IBaseFilter interface
	CComPtr<ITsWriter> m_pITsWriter;			// CTsWriter �� ITsWriter interface
	CComPtr<IBaseFilter> m_pDemux;				// MPEG2 Demultiplexer �� IBaseFilter interface
	CComPtr<IBaseFilter> m_pTif;				// MPEG2 Transport Information Filter �� IBaseFilter interface

	// RunningObjectTable�̓o�^ID
	DWORD m_dwROTRegister;

	// �`���[�i�M����Ԏ擾�p�C���^�[�t�F�[�X
	CComPtr<IBDA_SignalStatistics> m_pIBDA_SignalStatisticsTunerNode;
	CComPtr<IBDA_SignalStatistics> m_pIBDA_SignalStatisticsDemodNode;

	// DS�t�B���^�[�� CDSFilterEnum
	CDSFilterEnum *m_pDSFilterEnumTuner;
	CDSFilterEnum *m_pDSFilterEnumCapture;

	// DS�t�B���^�[�̏��
	struct DSListData {
		std::wstring GUID;
		std::wstring FriendlyName;
		ULONG Order;
		DSListData(std::wstring _GUID, std::wstring _FriendlyName, ULONG _Order)
			: GUID(_GUID),
			FriendlyName(_FriendlyName),
			Order(_Order)
		{
		}
	};

	// ���[�h���ׂ��`���[�i�E�L���v�`���̃��X�g
	struct TunerCaptureList {
		DSListData Tuner;
		std::vector<DSListData> CaptureList;
		TunerCaptureList(std::wstring TunerGUID, std::wstring TunerFriendlyName, ULONG TunerOrder)
			: Tuner(TunerGUID, TunerFriendlyName, TunerOrder)
		{
		}
		TunerCaptureList(DSListData _Tuner)
			: Tuner(_Tuner)
		{
		}
	};
	std::list<TunerCaptureList> m_UsableTunerCaptureList;

	// �`���[�i�[�̎g�p����TuningSpace�̎��
	enum enumTunerType {
		eTunerTypeNone = -1,
		eTunerTypeDVBS = 1,				// DBV-S/DVB-S2
		eTunerTypeDVBT = 2,				// DVB-T
		eTunerTypeDVBC = 3,				// DVB-C
		eTunerTypeDVBT2 = 4,			// DVB-T2
		eTunerTypeISDBS = 11,			// ISDB-S
		eTunerTypeISDBT = 12,			// ISDB-T
		eTunerTypeISDBC = 13,			// ISDB-C
		eTunerTypeATSC_Antenna = 21,	// ATSC
		eTunerTypeATSC_Cable = 22,		// ATSC Cable
		eTunerTypeDigitalCable = 23,	// Digital Cable
	};

	// �g�p����TuningSpace �I�u�W�F�N�g
	enum enumTuningSpace {
		eTuningSpaceAuto = -1,			// DVBSystemType�̒l�ɂ���Ď����I��
		eTuningSpaceDVB = 1,			// DVBTuningSpace
		eTuningSpaceDVBS = 2,			// DVBSTuningSpace
		eTuningSpaceAnalogTV = 21,		// AnalogTVTuningSpace
		eTuningSpaceATSC = 22,			// ATSCTuningSpace
		eTuningSpaceDigitalCable = 23,	// DigitalCableTuningSpace
	};

	// �g�p����Locator �I�u�W�F�N�g
	enum enumLocator {
		eLocatorAuto = -1,				// DVBSystemType�̒l�ɂ���Ď����I��
		eLocatorDVBT = 1,				// DVBTLocator
		eLocatorDVBT2 = 2,				// DVBTLocator2
		eLocatorDVBS = 3,				// DVBSLocator
		eLocatorDVBC = 4,				// DVBCLocator
		eLocatorISDBS = 11,				// ISDBSLocator
		eLocatorATSC = 21,				// ATSCLocator
		eLocatorDigitalCable = 22,		// DigitalCableLocator
	};

	// ITuningSpace�ɐݒ肷��NetworkType
	enum enumNetworkType {
		eNetworkTypeAuto = -1,			// DVBSystemType�̒l�ɂ���Ď����I��
		eNetworkTypeDVBT = 1,			// STATIC_DVB_TERRESTRIAL_TV_NETWORK_TYPE
		eNetworkTypeDVBS = 2,			// STATIC_DVB_SATELLITE_TV_NETWORK_TYPE
		eNetworkTypeDVBC = 3,			// STATIC_DVB_CABLE_TV_NETWORK_TYPE
		eNetworkTypeISDBT = 11,			// STATIC_ISDB_TERRESTRIAL_TV_NETWORK_TYPE
		eNetworkTypeISDBS = 12,			// STATIC_ISDB_SATELLITE_TV_NETWORK_TYPE
		eNetworkTypeISDBC = 13,			// STATIC_ISDB_CABLE_TV_NETWORK_TYPE
		eNetworkTypeATSC = 21,			// STATIC_ATSC_TERRESTRIAL_TV_NETWORK_TYPE
		eNetworkTypeDigitalCable = 22,	// STATIC_DIGITAL_CABLE_NETWORK_TYPE
		eNetworkTypeBSkyB = 101,		// STATIC_BSKYB_TERRESTRIAL_TV_NETWORK_TYPE
		eNetworkTypeDIRECTV = 102,		// STATIC_DIRECT_TV_SATELLITE_TV_NETWORK_TYPE
		eNetworkTypeEchoStar = 103,		// STATIC_ECHOSTAR_SATELLITE_TV_NETWORK_TYPE
	};

	// IDVBTuningSpace�ɐݒ肷��SystemType
	enum enumDVBSystemType {
		eDVBSystemTypeAuto = -1,								// DVBSystemType�̒l�ɂ���Ď����I��
		eDVBSystemTypeDVBC = DVBSystemType::DVB_Cable,			// DVB_Cable
		eDVBSystemTypeDVBT = DVBSystemType::DVB_Terrestrial,	// DVB_Terrestrial
		eDVBSystemTypeDVBS = DVBSystemType::DVB_Satellite,		// DVB_Satellite
		eDVBSystemTypeISDBT = DVBSystemType::ISDB_Terrestrial,	// ISDB_Terrestrial
		eDVBSystemTypeISDBS = DVBSystemType::ISDB_Satellite,	// ISDB_Satellite
	};

	// IAnalogTVTuningSpace�ɐݒ肷��InputType
	enum enumTunerInputType {
		eTunerInputTypeAuto = -1,										// DVBSystemType�̒l�ɂ���Ď����I��
		eTunerInputTypeCable = tagTunerInputType::TunerInputCable,		// TunerInputCable
		eTunerInputTypeAntenna = tagTunerInputType::TunerInputAntenna,	// TunerInputAntenna
	};

	// �`���[�i�[�̎g�p����TuningSpace�̎�ރf�[�^
	struct DVBSystemTypeData {
		enumTunerType nDVBSystemType;						// �`���[�i�[�̎g�p����TuningSpace�̎��
		enumTuningSpace nTuningSpace;						// �g�p����TuningSpace �I�u�W�F�N�g
		enumLocator nLocator;								// �g�p����Locator �I�u�W�F�N�g
		enumNetworkType nITuningSpaceNetworkType;			// ITuningSpace�ɐݒ肷��NetworkType
		enumDVBSystemType nIDVBTuningSpaceSystemType;		// IDVBTuningSpace�ɐݒ肷��SystemType
		enumTunerInputType nIAnalogTVTuningSpaceInputType;	// IAnalogTVTuningSpace�ɐݒ肷��InputType
		CComPtr<ITuningSpace> pITuningSpace;				// Tuning Space �� ITuningSpace interface
		DVBSystemTypeData(void)
			: nDVBSystemType(eTunerTypeNone),
			  nTuningSpace(eTuningSpaceAuto),
			  nLocator(eLocatorAuto),
			  nITuningSpaceNetworkType(eNetworkTypeAuto),
			  nIDVBTuningSpaceSystemType(eDVBSystemTypeAuto),
			  nIAnalogTVTuningSpaceInputType(eTunerInputTypeAuto)
		{
		}
		~DVBSystemTypeData(void)
		{
			pITuningSpace.Release();
		}
	};

	// TuningSpace�̎�ރf�[�^�x�[�X
	struct DVBSystemTypeDB {
		std::map<unsigned int, DVBSystemTypeData> SystemType;	// TuningSpace�̎�ޔԍ���TuningSpace�̎�ރf�[�^
		unsigned int nNumType;									// TuningSpace�̎�ސ�
		DVBSystemTypeDB(void)
			: nNumType(0)
		{
		}
		~DVBSystemTypeDB(void)
		{
			SystemType.clear();
		}
		BOOL IsExist(unsigned int number)
		{
			auto it = SystemType.find(number);
			if (it == SystemType.end())
				return FALSE;
			if (!it->second.pITuningSpace)
				return FALSE;
			return TRUE;
		}
		void ReleaseAll(void)
		{
			for (auto it = SystemType.begin(); it != SystemType.end(); it++) {
				it->second.pITuningSpace.Release();
			}
		}
	};
	DVBSystemTypeDB m_DVBSystemTypeDB;

	// ini�t�@�C���Œ�`�ł���ő�TuningSpace�̎�ރf�[�^��
	static constexpr unsigned int MAX_DVB_SYSTEM_TYPE = 10U;

	// �`���[�i�[�Ɏg�p����NetworkProvider 
	enum enumNetworkProvider {
		eNetworkProviderAuto = 0,		// DVBSystemType�̒l�ɂ���Ď����I��
		eNetworkProviderGeneric = 1,	// Microsoft Network Provider
		eNetworkProviderDVBS = 2,		// Microsoft DVB-S Network Provider
		eNetworkProviderDVBT = 3,		// Microsoft DVB-T Network Provider
		eNetworkProviderDVBC = 4,		// Microsoft DVB-C Network Provider
		eNetworkProviderATSC = 5,		// Microsoft ATSC Network Provider
	};
	enumNetworkProvider m_nNetworkProvider;

	// �q����M�p�����[�^/�ϒ������p�����[�^�̃f�t�H���g�l
	enum enumDefaultNetwork {
		eDefaultNetworkNone = 0,		// �ݒ肵�Ȃ�
		eDefaultNetworkSPHD = 1,		// SPHD
		eDefaultNetworkBSCS = 2,		// BS/CS110
		eDefaultNetworkUHF = 3,			// UHF/CATV
		eDefaultNetworkDual = 4,		// Dual Mode (BS/CS110��UHF/CATV)
	};
	enumDefaultNetwork m_nDefaultNetwork;

	// �t�B���^�O���t��RunningObjectTable�ɓo�^���邩�ǂ���
	BOOL m_bRegisterGraphInROT;

	// �d���v�����ύX��GUID
	std::wstring m_sPowerSetOnOpenedGUID;
	std::wstring m_sPowerSetOnClosingGUID;

	// Tuner is opened
	BOOL m_bOpened;

	// SetChannel()�����݂��`���[�j���O�X�y�[�X�ԍ�
	DWORD m_dwTargetSpace;

	// �J�����g�`���[�j���O�X�y�[�X�ԍ�
	DWORD m_dwCurSpace;

	// �`���[�j���O�X�y�[�X�ԍ��s��
	static constexpr DWORD SPACE_INVALID = 0xFFFFFFFFUL;

	// SetChannel()�����݂��`�����l���ԍ�
	DWORD m_dwTargetChannel;

	// �J�����g�`�����l���ԍ�
	DWORD m_dwCurChannel;

	// �`�����l���ԍ��s��
	static constexpr DWORD CHANNEL_INVALID = 0xFFFFFFFFUL;

	// ���݂̃g�[���ؑ֏��
	long m_nCurTone; // current tone signal state

	// �g�[���ؑ֏�ԕs��
	static constexpr long TONE_UNKNOWN = -1L;

	// TSMF�������K�v
	BOOL m_bIsEnabledTSMF;

	// �k���p�P�b�g�폜���������Z�b�g
	LONG m_lResetFilter;

	// TS�p�P�b�g�T�C�Y(�k���p�P�b�g�폜�p)
	size_t m_PacketSize;

	// �O�񏈗�����TS�p�P�b�g�o�b�t�@����э�Ɨp(�k���p�P�b�g�폜�p)
	std::vector<BYTE> m_FilterBuf;

	// �Ō��LockChannel���s�������̃`���[�j���O�p�����[�^
	TuningParam m_LastTuningParam;

	// TunerSpecial DLL module handle
	HMODULE m_hModuleTunerSpecials;

	// �`���[�i�ŗL�֐� IBdaSpecials
	IBdaSpecials *m_pIBdaSpecials;
	IBdaSpecials2b5 *m_pIBdaSpecials2;

	// �`���[�i�ŗL�̊֐����K�v���ǂ������������ʂ���DB
	// GUID ���L�[�� DLL ���𓾂�
	struct TUNER_SPECIAL_DLL {
		const WCHAR * const sTunerGUID;
		const WCHAR * const sDLLBaseName;
	};
	static constexpr TUNER_SPECIAL_DLL aTunerSpecialData[] = {
		// �����̓v���O���}����������Ȃ��Ǝv���̂ŁA�v���O��������GUID ���������ɐ��K�����Ȃ��̂ŁA
		// �ǉ�����ꍇ�́AGUID�͏������ŏ����Ă�������

		/* TBS6980A */
		{ L"{e9ead02c-8b8c-4d9b-97a2-2ec0324360b1}", L"TBS" },

		/* TBS6980B, Prof 8000 */
		{ L"{ed63ec0b-a040-4c59-bc9a-59b328a3f852}", L"TBS" },

		/* Prof 7300, 7301, TBS 8920 */
		{ L"{91b0cc87-9905-4d65-a0d1-5861c6f22cbf}", L"TBS" },	// 7301 �͌ŗL�֐��łȂ��Ă�OK������

		/* TBS 6920 */
		{ L"{ed63ec0b-a040-4c59-bc9a-59b328a3f852}", L"TBS" },

		/* Prof Prof 7500, Q-BOX II */
		{ L"{b45b50ff-2d09-4bf2-a87c-ee4a7ef00857}", L"TBS" },

		/* DVBWorld 2002, 2004, 2006 */
		{ L"{4c807f36-2db7-44ce-9582-e1344782cb85}", L"DVBWorld" },

		/* DVBWorld 210X, 2102X, 2104X */
		{ L"{5a714cad-60f9-4124-b922-8a0557b8840e}", L"DVBWorld" },

		/* DVBWorld 2005 */
		{ L"{ede18552-45e6-469f-93b5-27e94296de38}", L"DVBWorld" }, // 2005 �͌ŗL�֐��͕K�v�Ȃ�����

		{ L"", L"" }, // terminator
	};

	// �`�����l������������ inline �֐�
	inline std::basic_string<TCHAR> MakeChannelName(const CBonTuner::ChData* const pChData)
	{
		std::basic_string<TCHAR> format;
		long m = pChData->Frequency / 1000;
		long k = pChData->Frequency % 1000;
		if (k == 0)
			format = _T("%s/%05ld%c/%s");
		else
			format = _T("%s/%05ld.%03ld%c/%s");
		return common::TStringPrintf(format.c_str(), m_sSatelliteName[pChData->Satellite].c_str(), m, PolarisationChar[pChData->Polarisation], m_sModulationName[pChData->ModulationType].c_str());
	}
};

