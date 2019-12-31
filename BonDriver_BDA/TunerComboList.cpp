#include <regex>

// IBDA_Topology
#include <bdaiface.h>

#include "TunerComboList.h"
#include "common.h"
#include "DSFilterEnum.h"

static constexpr CLSID KSCATEGORY_BDA_NETWORK_TUNER =		{ 0x71985f48, 0x1ca1, 0x11d3, 0x9c, 0xc8, 0x00, 0xc0, 0x4f, 0x79, 0x71, 0xe0 };
static constexpr CLSID KSCATEGORY_BDA_RECEIVER_COMPONENT =	{ 0xfd0a5af4, 0xb41d, 0x11d2, 0x9c, 0x95, 0x00, 0xc0, 0x4f, 0x79, 0x71, 0xe0 };

void CTunerComboList::ClearSearchData(void)
{
	TunerSearchDB.clear();
	SAFE_DELETE(pDSFilterEnumTuner);
	SAFE_DELETE(pDSFilterEnumCapture);
}

void CTunerComboList::AddSearchData(unsigned int tunerGroup, unsigned int order, std::wstring tunerGuid, std::wstring tunerFriendlyName, std::wstring captureGuid, std::wstring captureFriendlyName)
{
	TunerSearchDB[tunerGroup].emplace(order, TunerSearchData(tunerGuid, tunerFriendlyName, captureGuid, captureFriendlyName));
}

void CTunerComboList::BuildComboDB(void)
{
	pDSFilterEnumTuner = new CDSFilterEnum(KSCATEGORY_BDA_NETWORK_TUNER, CDEF_DEVMON_PNP_DEVICE);
	pDSFilterEnumCapture = new CDSFilterEnum(KSCATEGORY_BDA_RECEIVER_COMPONENT, CDEF_DEVMON_PNP_DEVICE);

	HRESULT hr;
	ULONG order;

	// システムに存在するTunerデバイスの一覧
	std::vector<DSListData> availableTunerList;
	order = 0;
	while (SUCCEEDED(hr = pDSFilterEnumTuner->next()) && hr == S_OK) {
		std::wstring sDisplayName;
		std::wstring sFriendlyName;

		// チューナの DisplayName, FriendlyName を得る
		pDSFilterEnumTuner->getDisplayName(&sDisplayName);
		pDSFilterEnumTuner->getFriendlyName(&sFriendlyName);

		// 一覧に追加
		availableTunerList.emplace_back(sDisplayName, sFriendlyName, order);

		order++;
	}

	// システムに存在するCaptureデバイスの一覧
	std::vector<DSListData> availableCaptureList;
	order = 0;
	while (SUCCEEDED(hr = pDSFilterEnumCapture->next()) && hr == S_OK) {
		std::wstring sDisplayName;
		std::wstring sFriendlyName;

		// チューナの DisplayName, FriendlyName を得る
		pDSFilterEnumCapture->getDisplayName(&sDisplayName);
		pDSFilterEnumCapture->getFriendlyName(&sFriendlyName);

		// 一覧に追加
		availableCaptureList.emplace_back(sDisplayName, sFriendlyName, order);

		order++;
	}

	TunerComboDB.clear();

	for (auto itGroup = TunerSearchDB.begin(); itGroup != TunerSearchDB.end(); itGroup++) {
		unsigned int groupNum = itGroup->first;
		OutputDebug(L"[CTunerComboList::BuildComboDB] TunerGroup%d:\n", groupNum);

		unsigned int total = 0;

		for (auto itSearchDB = itGroup->second.begin(); itSearchDB != itGroup->second.end(); itSearchDB++) {
			for (auto itTunerList = availableTunerList.begin(); itTunerList != availableTunerList.end(); itTunerList++) {
				// DisplayName に GUID が含まれるか検査して、NOだったら次のチューナへ
				if (itSearchDB->second.TunerGUID.compare(L"") != 0 && itTunerList->GUID.find(itSearchDB->second.TunerGUID) == std::wstring::npos) {
					continue;
				}

				// FriendlyName が含まれるか検査して、NOだったら次のチューナへ
				if (itSearchDB->second.TunerFriendlyName.compare(L"") != 0 && itTunerList->FriendlyName.find(itSearchDB->second.TunerFriendlyName) == std::wstring::npos) {
					continue;
				}

				// 対象のチューナデバイスだった
				OutputDebug(L"[CTunerComboList::BuildComboDB] Found tuner device=FriendlyName:%s,  GUID:%s\n", itTunerList->FriendlyName.c_str(), itTunerList->GUID.c_str());
				if (!bNotExistCaptureDevice) {
					// Captureデバイスを使用する
					std::vector<DSListData> TempCaptureList;
					for (auto itCaptureList = availableCaptureList.begin(); itCaptureList != availableCaptureList.end(); itCaptureList++) {
						// DisplayName に GUID が含まれるか検査して、NOだったら次のキャプチャへ
						if (itSearchDB->second.CaptureGUID.compare(L"") != 0 && itCaptureList->GUID.find(itSearchDB->second.CaptureGUID) == std::wstring::npos) {
							continue;
						}

						// FriendlyName が含まれるか検査して、NOだったら次のキャプチャへ
						if (itSearchDB->second.CaptureFriendlyName.compare(L"") != 0 && itCaptureList->FriendlyName.find(itSearchDB->second.CaptureFriendlyName) == std::wstring::npos) {
							continue;
						}

						// 対象のキャプチャデバイスだった
						OutputDebug(L"[CTunerComboList::BuildComboDB]   Found capture device=FriendlyName:%s,  GUID:%s\n", itCaptureList->FriendlyName.c_str(), itCaptureList->GUID.c_str());
						TempCaptureList.emplace_back(*itCaptureList);
					}

					if (TempCaptureList.empty()) {
						// キャプチャデバイスが見つからなかったので次のチューナへ
						OutputDebug(L"[CTunerComboList::BuildComboDB]   No combined capture devices.\n");
						continue;
					}

					// チューナをListに追加
					TunerComboDB[groupNum].emplace_back(*itTunerList);

					unsigned int count = 0;
					if (bCheckDeviceInstancePath) {
						// チューナデバイスとキャプチャデバイスのデバイスインスタンスパスが一致しているか確認
						OutputDebug(L"[CTunerComboList::BuildComboDB]   Checking device instance path.\n");
						std::wstring dip1 = CDSFilterEnum::getDeviceInstancePathrFromDisplayName(itTunerList->GUID);
						ReassembleDeviceInstancePath(&dip1);
						for (auto itCaptureList = TempCaptureList.begin(); itCaptureList != TempCaptureList.end(); itCaptureList++) {
							std::wstring dip2 = CDSFilterEnum::getDeviceInstancePathrFromDisplayName(itCaptureList->GUID);
							ReassembleDeviceInstancePath(&dip2);
							if (dip2 == dip1) {
								// デバイスパスが一致するものをListに追加
								OutputDebug(L"[CTunerComboList::BuildComboDB]     Adding matched tuner and capture device.\n");
								OutputDebug(L"[CTunerComboList::BuildComboDB]       tuner=FriendlyName:%s,  GUID:%s\n", itTunerList->FriendlyName.c_str(), itTunerList->GUID.c_str());
								OutputDebug(L"[CTunerComboList::BuildComboDB]       capture=FriendlyName:%s,  GUID:%s\n", itCaptureList->FriendlyName.c_str(), itCaptureList->GUID.c_str());
								TunerComboDB[groupNum].back().CaptureList.emplace_back(*itCaptureList);
								count++;
							}
						}
					}

					if (count == 0) {
						// デバイスパスが一致するものがなかった or 確認しない
						if (bCheckDeviceInstancePath) {
							// 確認有りで一致しているものが無い
							OutputDebug(L"[CTunerComboList::BuildComboDB]     No matched devices.\n");
						}
						for (auto itCaptureList = TempCaptureList.begin(); itCaptureList != TempCaptureList.end(); itCaptureList++) {
							// 確認無しなのですべてListに追加
							OutputDebug(L"[CTunerComboList::BuildComboDB]   Adding tuner and capture device.\n");
							OutputDebug(L"[CTunerComboList::BuildComboDB]     tuner=FriendlyName:%s,  GUID:%s\n", itTunerList->FriendlyName.c_str(), itTunerList->GUID.c_str());
							OutputDebug(L"[CTunerComboList::BuildComboDB]     capture=FriendlyName:%s,  GUID:%s\n", itCaptureList->FriendlyName.c_str(), itCaptureList->GUID.c_str());
							TunerComboDB[groupNum].back().CaptureList.emplace_back(*itCaptureList);
							count++;
						}
					}

					OutputDebug(L"[CTunerComboList::BuildComboDB]   %d of combination was added.\n", count);
					total += count;
				}
				else
				{
					// Captureデバイスを使用しない
					OutputDebug(L"[CTunerComboList::BuildComboDB]   Adding tuner device only.\n");
					OutputDebug(L"[CTunerComboList::BuildComboDB]     tuner=FriendlyName:%s,  GUID:%s\n", itTunerList->FriendlyName.c_str(), itTunerList->GUID.c_str());
					TunerComboDB[groupNum].emplace_back(*itTunerList);
				}
			}
		}

		OutputDebug(L"[CTunerComboList::BuildComboDB] Total %d of combination was added to TunerGroup%d.\n", total, groupNum);
	}
}

BOOL CTunerComboList::ResetTuner(unsigned int group)
{
	CurrentGroup = group;
	itTuner = TunerComboDB[CurrentGroup].begin();
	return itTuner != TunerComboDB[CurrentGroup].end();
}

BOOL CTunerComboList::NextTuner(void)
{
	itTuner++;
	return itTuner != TunerComboDB[CurrentGroup].end();
}

void CTunerComboList::GetTunerData(const std::wstring** guid, const std::wstring** friendlyName, const ULONG** order)
{
	if (guid)
		*guid = &(itTuner->Tuner.GUID);
	if (friendlyName)
		*friendlyName = &(itTuner->Tuner.FriendlyName);
	if (order)
		*order = &(itTuner->Tuner.Order);
}

HRESULT CTunerComboList::GetTunerFilter(IBaseFilter** ppFilter)
{
	if (!pDSFilterEnumTuner)
		return E_FAIL;

	return pDSFilterEnumTuner->getFilter(ppFilter, itTuner->Tuner.Order);
}

BOOL CTunerComboList::ResetCapture(void)
{
	itCapture = itTuner->CaptureList.begin();
	return itCapture != itTuner->CaptureList.end();
}

BOOL CTunerComboList::NextCapture(void)
{
	itCapture++;
	return itCapture != itTuner->CaptureList.end();
}

void CTunerComboList::GetCaptureData(const std::wstring** guid, const std::wstring** friendlyName, const ULONG** order)
{
	if (guid)
		*guid = &(itCapture->GUID);
	if (friendlyName)
		*friendlyName = &(itCapture->FriendlyName);
	if (order)
		*order = &(itCapture->Order);
}

HRESULT CTunerComboList::GetCaptureFilter(IBaseFilter** ppFilter)
{
	if (!pDSFilterEnumCapture)
		return E_FAIL;

	return pDSFilterEnumCapture->getFilter(ppFilter, itCapture->Order);
}

void CTunerComboList::Remenber(void)
{
	LastConnecttedTuner = itTuner;
	LastConnecttedGroup = CurrentGroup;
}

void CTunerComboList::Forget()
{
	LastConnecttedGroup = NO_MEMORY;
}

void CTunerComboList::PutOff()
{
	if (LastConnecttedGroup != NO_MEMORY) {
		TunerComboDB[LastConnecttedGroup].splice(TunerComboDB[LastConnecttedGroup].end(), TunerComboDB[LastConnecttedGroup], LastConnecttedTuner);
	}
}

void CTunerComboList::ReassembleDeviceInstancePath(std::wstring* deviceInstancePath)
{
	std::wstring enumerator;
	std::wstring deviceId;
	std::wstring instanceId;

	CDSFilterEnum::disassembleDeviceInstancePath(*deviceInstancePath, &enumerator, &deviceId, &instanceId);

	// 末尾の"&tuner"/"&capture"等、Device-IDの余分な文字列を取り除く
	if (enumerator == L"USB") {
		std::wsmatch match;
		if (std::regex_match(deviceId, match, std::wregex(LR"(^(VID_[0-9A-Z]{4}&PID_[0-9A-Z]{4}(?:&MI_[0-9A-Z]{2})?)(?:&.+)?$)"))) {
			deviceId = match.str(1);
		}
	}
	else if (enumerator == L"PCI" || enumerator == L"DD_DVB" || enumerator == L"NGENE") {
		std::wsmatch match;
		if (std::regex_match(deviceId, match, std::wregex(LR"(^(VEN_[0-9A-Z]{4}&DEV_[0-9A-Z]{4}&SUBSYS_[0-9A-Z]{8}(?:&REV_[0-9A-Z]{2})?)(?:&.+)?$)"))) {
			deviceId = match.str(1);
		}
	}

	// Enumeratorが"dd_dvb"/"Ngene"のTuner/CaptureはInstance-IDの末尾が"&2"/"&4"のように区別されているので取り除く
	if (enumerator == L"DD_DVB" || enumerator == L"NGENE") {
		std::wsmatch match;
		if (std::regex_match(instanceId, match, std::wregex(LR"(^([0-9A-Z]+(?:&[0-9A-Z]+)?(?:&[0-9A-Z]+)?)(?:&[0-9A-Z]+)?$)"))) {
			instanceId = match.str(1);
		}
	}

	*deviceInstancePath = enumerator + L"\\" + deviceId + L"\\" + instanceId;

	return;
}

CTunerComboList::TunerSearchData::TunerSearchData(void)
{
}

CTunerComboList::TunerSearchData::TunerSearchData(std::wstring tunerGuid, std::wstring tunerFriendlyName, std::wstring captureGuid, std::wstring captureFriendlyName)
	: TunerFriendlyName(tunerFriendlyName),
	CaptureFriendlyName(captureFriendlyName)
{
	TunerGUID = common::WStringToLowerCase(tunerGuid);
	CaptureGUID = common::WStringToLowerCase(captureGuid);
}
