#pragma once

#include <Windows.h>

class CTSMFParser
{
private:
	int slot_counter;										// TSMF���d�t���[���X���b�g�ԍ�
	WORD TSID;												// ���o����X�g���[����TSID�܂��͑���TS�ԍ�
	WORD ONID;												// ���o����X�g���[����ONID
	BOOL IsRelative;										// ����TS�ԍ��Ŏw�肷�邩�ǂ��� FALSE..ONID/TSID�Ŏw��, TRUE..����TS�ԍ��Ŏw��
	BOOL IsClearCalled;										// ��͏����̃N���A���K�v���ǂ���
	CRITICAL_SECTION csClear;								// �N���A������r��
	size_t PacketSize;										// TS�p�P�b�g�T�C�Y
	BYTE * prevBuf;											// �O�񏈗�����TS�p�P�b�g�o�b�t�@(���������[���ۑ��p)
	size_t prevBufSize;										// �O�񏈗�����TS�p�P�b�g�o�b�t�@�̃T�C�Y
	size_t prevBufPos;										// �O�񏈗�����TS�p�P�b�g�o�b�t�@�̏����J�n�ʒu
	struct {
		BYTE continuity_counter;							// �A�����w�W
		BYTE version_number;								// �ύX�w��
		BYTE relative_stream_number_mode;					// �X���b�g�z�u�@�̋��
		BYTE frame_type;									// ���d�t���[���`��
		struct {
			BYTE stream_status;								// ���΃X�g���[���ԍ��ɑ΂���L���A�����w��
			WORD stream_id;									// �X�g���[�����ʁ^���΃X�g���[���ԍ��Ή����
			WORD original_network_id;						// �I���W�i���l�b�g���|�N���ʁ^���΃X�g���[���ԍ��Ή����
			BYTE receive_status;							// ��M���
		} stream_info[15];									// ���΃X�g���[���ԍ����̏��
		BYTE emergency_indicator;							// �ً}�x��w��
		BYTE relative_stream_number[52];					// ���΃X�g���[���ԍ��΃X���b�g�Ή����
	} TSMFData;												// TSMF���d�t���[���w�b�_���
	static constexpr BYTE TS_PACKET_SYNC_BYTE = 0x47;		// TS�p�P�b�g�w�b�_�����o�C�g�R�[�h

public:
	// �R���X�g���N�^
	CTSMFParser(void);
	// �f�X�g���N�^
	~CTSMFParser(void);
	// �X�g���[�����ʎq���Z�b�g
	void SetTSID(WORD onid, WORD tsid, BOOL relative);
	// TSMF�����𖳌��ɂ���
	void Disable(void);
	// TS�o�b�t�@��TSMF�������s��
	void ParseTsBuffer(BYTE * buf, size_t len, BYTE ** newBuf, size_t * newBufLen);

private:
	// �S�Ă̏����N���A
	void Clear(WORD onid = 0xffff, WORD tsid = 0xffff, BOOL relative = FALSE);
	// TSMF�w�b�_�̉�͂��s��
	BOOL ParseTSMFHeader(const BYTE * buf, size_t len);
	// 1�p�P�b�g(1�t���[��)�̏������s��
	BOOL ParseOnePacket(const BYTE * buf, size_t len, WORD onid, WORD tsid, BOOL relative);
	// TS�p�P�b�g�̓������s��
	BOOL SyncPacket(const BYTE * buf, size_t len, size_t * truncate, size_t * packetSize);
};