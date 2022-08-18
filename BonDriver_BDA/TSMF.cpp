#include "common.h"

#include "crc32.h"

#include "TSMF.h"

CTSMFParser::CTSMFParser(void)
	: slot_counter(-1),
	TSID(0xffff),
	ONID(0xffff),
	IsRelative(FALSE),
	IsClearCalled(FALSE),
	PacketSize(0),
	prevBuf(NULL),
	prevBufSize(0),
	prevBufPos(0)
{
	::InitializeCriticalSection(&csClear);
}

CTSMFParser::~CTSMFParser(void)
{
	::DeleteCriticalSection(&csClear);
}

void CTSMFParser::SetTSID(WORD onid, WORD tsid, BOOL relative)
{
	Clear(onid, tsid, relative);
}

void CTSMFParser::Disable(void)
{
	Clear();
}

void CTSMFParser::ParseTsBuffer(BYTE * buf, size_t len, BYTE ** newBuf, size_t * newBufLen)
{
	if (!buf || len <= 0)
		return;

	::EnterCriticalSection(&csClear);
	WORD onid = ONID;
	WORD tsid = TSID;
	BOOL relative = IsRelative;
	BOOL clearCalled = IsClearCalled;
	IsClearCalled = FALSE;
	::LeaveCriticalSection(&csClear);

	if (clearCalled) {
		// TSMF���d�t���[���̓��������N���A
		slot_counter = -1;

		// TS�p�P�b�g���������N���A
		PacketSize = 0;

		// ������TS�p�P�b�g�o�b�t�@���N���A
		SAFE_DELETE_ARRAY(prevBuf);
		prevBufSize = 0;
		prevBufPos = 0;
	}

	// �O��̎c��f�[�^�ƐV�K�f�[�^���������ĐV����Read�o�b�t�@���쐬
	size_t readBufSize = (prevBufSize - prevBufPos) + len;
	size_t readBufPos = 0;
	BYTE * readBuf = new BYTE[readBufSize];
	if (prevBuf) {
		// �O��̎c��f�[�^���R�s�[
		memcpy(readBuf, prevBuf + prevBufPos, prevBufSize - prevBufPos);
		// �O��̎c��f�[�^��j��
		SAFE_DELETE_ARRAY(prevBuf);
	}
	// �V�K�f�[�^���R�s�[
	memcpy(readBuf + (prevBufSize - prevBufPos), buf, len);

	// Write�p�e���|�����o�b�t�@���쐬
	size_t tempBufPos = 0;
	BYTE * tempBuf = new BYTE[readBufSize];

	// Read�o�b�t�@������
	while (readBufSize - readBufPos > PacketSize) {
		if (PacketSize == 0) {
			// TS�p�P�b�g�̓���
			size_t truncate = 0;	// �؎̂ăT�C�Y
			SyncPacket(readBuf + readBufPos, readBufSize - readBufPos, &truncate, &PacketSize);
			// TS�p�P�b�g�擪�܂ł̃f�[�^��؂�̂Ă�
			readBufPos += truncate;
			if (PacketSize == 0)
				// TS�o�b�t�@�̃f�[�^�T�C�Y�����������ē����ł��Ȃ�
				break;

			continue;
		}
		// �����ł��Ă���
		if (ParseOnePacket(readBuf + readBufPos, readBufSize - readBufPos, onid, tsid, relative)) {
			// �K�v��TSMF�t���[�����e���|�����o�b�t�@�֒ǉ�
			memcpy(tempBuf + tempBufPos, readBuf + readBufPos, 188);
			tempBufPos += 188;
		}
		// ����Read�ʒu��
		readBufPos += PacketSize;
	}

	// Write�p�e���|�����o�b�t�@����łȂ����Result�p�o�b�t�@���쐬
	if (tempBufPos > 0) {
		::EnterCriticalSection(&csClear);
		clearCalled = IsClearCalled;
		::LeaveCriticalSection(&csClear);

		// �r���ŃN���A���ꂽ��̂Ă�
		if (!clearCalled) {
			BYTE * resultBuf = new BYTE[tempBufPos];
			memcpy(resultBuf, tempBuf, tempBufPos);
			*newBuf = resultBuf;
			*newBufLen = tempBufPos;
		}
	}
	// Write�p�e���|�����o�b�t�@��j��
	SAFE_DELETE_ARRAY(tempBuf);

	// ���[��TS�f�[�^���c���Ă���ꍇ�͎��񏈗��p�ɕۑ�
	if (readBuf && readBufSize - readBufPos > 0) {
		// �c���TS�f�[�^��ۑ�
		prevBuf = readBuf;
		prevBufSize = readBufSize;
		prevBufPos = readBufPos;
	}
	else {
		// �S�Ďg�p�ς݂̃o�b�t�@�͔j��
		SAFE_DELETE_ARRAY(readBuf);
		prevBufSize = 0;
		prevBufPos = 0;
	}

	return;
}

void CTSMFParser::Clear(WORD onid, WORD tsid, BOOL relative)
{
	// �X�g���[�����ʎq�������܂��͕ύX
	::EnterCriticalSection(&csClear);
	ONID = onid;
	TSID = tsid;
	IsRelative = relative;
	IsClearCalled = TRUE;
	::LeaveCriticalSection(&csClear);
}

BOOL CTSMFParser::ParseTSMFHeader(const BYTE * buf, size_t len)
{
	static constexpr WORD FRAME_SYNC_MASK = 0x1fff;
	static constexpr WORD FRAME_SYNC_F = 0x1a86;
	static constexpr WORD FRAME_SYNC_I = ~FRAME_SYNC_F & FRAME_SYNC_MASK;

	// �p�P�b�g�T�C�Y
	if (len < 188)
		return FALSE;

	// �����o�C�g
	BYTE sync_byte = buf[0];
	if (sync_byte != TS_PACKET_SYNC_BYTE)
		return FALSE;

	// ���d�t���[��PID
	WORD frame_PID = (buf[1] << 8) | buf[2];
	if (frame_PID != 0x002F)
		return FALSE;

	// �Œ�l
	if ((buf[3] & 0xf0) != 0x10)
		return FALSE;

	// ���d�t���[�������M��
	WORD frame_sync = ((buf[4] << 8) | buf[5]) & FRAME_SYNC_MASK;
	if (frame_sync != FRAME_SYNC_F && frame_sync != FRAME_SYNC_I)
		return FALSE;

	// CRC
	if (crc32(&buf[4], 184) != 0)
		return FALSE;

	// �A�����w�W
	TSMFData.continuity_counter = buf[3] & 0x0f;

	// �ύX�w��
	TSMFData.version_number = (buf[6] & 0xE0) >> 5;

	// �X���b�g�z�u�@�̋��
	TSMFData.relative_stream_number_mode = (buf[6] & 0x10) >> 4;
	if (TSMFData.relative_stream_number_mode != 0x0)
		return FALSE;

	// ���d�t���[���`��
	TSMFData.frame_type = (buf[6] & 0x0f);
	if (TSMFData.frame_type != 0x1)
		return FALSE;

	// ���΃X�g���[���ԍ����̏��
	for (int i = 0; i < 15; i++) {
		// ���΃X�g���[���ԍ��ɑ΂���L���A�����w��
		TSMFData.stream_info[i].stream_status = (buf[7 + (i / 8)] & (0x80 >> (i % 8))) >> (7 - (i % 8));
		// �X�g���[�����ʁ^���΃X�g���[���ԍ��Ή����
		TSMFData.stream_info[i].stream_id = (buf[9 + (i * 4)] << 8) | buf[10 + (i * 4)];
		// �I���W�i���l�b�g���|�N���ʁ^���΃X�g���[���ԍ��Ή����
		TSMFData.stream_info[i].original_network_id = (buf[11 + (i * 4)] << 8) | buf[12 + (i * 4)];
		// ��M���
		TSMFData.stream_info[i].receive_status = (buf[69 + (i / 4)] & (0xc0 >> ((i % 4) * 2))) >> ((3 - (i % 4)) * 2);
	}

	// �ً}�x��w��
	TSMFData.emergency_indicator = buf[72] & 0x01;

	// ���΃X�g���[���ԍ��΃X���b�g�Ή����
	for (int i = 0; i < 52; i++) {
		TSMFData.relative_stream_number[i] = (buf[73 + (i / 2)] & (0xf0 >> ((i % 2) * 4))) >> ((1 - (i % 2)) * 4);
	}

	return TRUE;
}

BOOL CTSMFParser::ParseOnePacket(const BYTE * buf, size_t len, WORD onid, WORD tsid, BOOL relative)
{
	if (buf[0] != TS_PACKET_SYNC_BYTE) {
		// TS�p�P�b�g�̓����O��
		PacketSize = 0;
		SAFE_DELETE_ARRAY(prevBuf);
		prevBufSize = 0;
		prevBufPos = 0;
		slot_counter = -1;
		return FALSE;
	}

	if (tsid == 0xffff)
		// TSID�w�肪0xffff�Ȃ�ΑS�ẴX���b�g��Ԃ�
		return TRUE;

	if (ParseTSMFHeader(buf, len)) {
		// TSMF���d�t���[���w�b�_
		slot_counter = 0;
		return FALSE;
	}

	if (slot_counter < 0 || slot_counter > 51)
		// TSMF���d�t���[���̓������Ƃ�Ă��Ȃ�
		return FALSE;

	slot_counter++;

	int ts_number = 0;
	if (relative) {
		// ����TS�ԍ��𒼐ڎw��
		ts_number = (int)tsid + 1;
	}
	else {
		// ONID��TSID�Ŏw��
		for (int i = 0; i < 15; i++) {
			if (TSMFData.stream_info[i].stream_id == tsid && (onid == 0xffff || TSMFData.stream_info[i].original_network_id == onid)) {
				ts_number = i + 1;
				break;
			}
		}
	}
	if (ts_number < 1 || ts_number > 15)
		// �Y�����鑊��TS�ԍ�������
		return FALSE;

	if (TSMFData.stream_info[ts_number - 1].stream_status == 0)
		// ���̑���TS�ԍ��͖��g�p
		return FALSE;

	if (TSMFData.relative_stream_number[slot_counter - 1] != ts_number)
		// ���̃X���b�g�͑��̑���TS�ԍ��p�X���b�g��������
		return FALSE;

	return TRUE;
}

BOOL CTSMFParser::SyncPacket(const BYTE * buf, size_t len, size_t * truncate, size_t * packetSize)
{
	static size_t constexpr TS_PACKET_SEARCH_SIZE = 208;
	if (!truncate || !packetSize)
		return FALSE;

	*truncate = 0;
	*packetSize = 0;

	if (len < TS_PACKET_SEARCH_SIZE * 3 + 1)
		// TS�f�[�^�����������ē��������Ȃ�
		return FALSE;

	for (size_t i = 0; i < TS_PACKET_SEARCH_SIZE; i++) {
		if (buf[i] == TS_PACKET_SYNC_BYTE) {
			// �����o�C�g����������
			// 188�o�C�g�p�P�b�g�̊m�F(�ʏ��TS�p�P�b�g)
			if (buf[i + 188] == TS_PACKET_SYNC_BYTE && buf[i + 188 * 2] == TS_PACKET_SYNC_BYTE) {
				*truncate = i;
				*packetSize = 188;
				return TRUE;
			}
			// 204�o�C�g�p�P�b�g�̊m�F(FEC�t)
			if (buf[i + 204] == TS_PACKET_SYNC_BYTE && buf[i + 204 * 2] == TS_PACKET_SYNC_BYTE) {
				*truncate = i;
				*packetSize = 204;
				return TRUE;
			}
			// 192�o�C�g�p�P�b�g�̊m�F(�^�C���X�^���v�t)
			if (buf[i + 192] == TS_PACKET_SYNC_BYTE && buf[i + 192 * 2] == TS_PACKET_SYNC_BYTE) {
				*truncate = i;
				*packetSize = 192;
				return TRUE;
			}
			// 208�o�C�g�p�P�b�g�̊m�F(�^�C���X�^���v�t)
			if (buf[i + 208] == TS_PACKET_SYNC_BYTE && buf[i + 208 * 2] == TS_PACKET_SYNC_BYTE) {
				*truncate = i;
				*packetSize = 208;
				return TRUE;
			}
		}
	}

	*truncate = TS_PACKET_SEARCH_SIZE;
	return FALSE;
}
