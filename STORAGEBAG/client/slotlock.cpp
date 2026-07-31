#include "pch.h"
#include "hook.h"
#include "ztl/ztl.h"
#include "wvs/util.h"
#include "wvs/packet.h"
#include "wvs/secure.h"
#include "wvs/wnd.h"
#include "wvs/ctrlwnd.h"
#include "wvs/wvsapp.h"
#include "wvs/iteminfo.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <climits>

// Defined in storagebag.cpp — a plain right-click in the inventory deposits the item into its category
// Storage Bag (only while the bag is OPEN). Returns true if it handled the click (caller consumes it).
bool BagWindow_DepositFromInventory(int invType, int slot);

namespace SlotLock {
// [ Mod Config ]

// --- Visual Colors ---
// Tip: 0x(Alpha)(Red)(Green)(Blue)
constexpr auto kInventoryLockBorderColor    = 0xAA4A90E2; // Light Blue
constexpr auto kShopLockBorderColor         = 0xCCFF3333; // Vivid Azure Blue
/* Example:
   0xCCDAA520 (Antique Gold), 0xCCFF7F50 (Soft Coral), 0xCC9966CC (Muted Purple)
*/
// --- UI Geometry ---
constexpr int kInventoryDrawThickness = 2;
constexpr int kShopDrawThickness      = 2;

// --- Input & Controls ---
constexpr int kDragLockKey            = VK_SHIFT;

// --- Shop Protection ---
constexpr bool kPreventSellingLockedItems = true;

// Refer to ChatType enum (e.g. ChatType::CHAT_TYPE_SYSTEM = 0xC)               = 0xC,
constexpr auto kSellBlockMessageType = 0xC;

constexpr const char* kSellBlockedMessage = "Não é possível vender o item, pois o slot está bloqueado.";

constexpr int kMaxInventorySlot = 96;
// [ ~Mod Config ]

class CClientSocket;
struct GW_ItemSlotBase;

class CharacterData {
public:
	// v83
	// 447
	// v95
	// 00000501     ZArray<ZRef<GW_ItemSlotBase> > aaItemSlot[6];
	using ItemSlotArray = std::array<ZArray<ZRef<GW_ItemSlotBase>>, 6>;
	MEMBER_AT(ItemSlotArray, 0x0447, aaItemSlot)


	// // v83
	// // ___:004282F7 ; int __thiscall CharacterData::GetItem(int this, int result, signed int nTI, int nPos)
	// // v95
	// // .text:0042B990 ; ZRef<GW_ItemSlotBase> *__thiscall CharacterData::GetItem(CharacterData *this, ZRef<GW_ItemSlotBase> *result, int nTI, int nPos)
	// ZRef<GW_ItemSlotBase>* GetItem(ZRef<GW_ItemSlotBase>* result, int nTI, int nPos) {
	//     return reinterpret_cast<ZRef<GW_ItemSlotBase>*(__thiscall*)(CharacterData*, ZRef<GW_ItemSlotBase>*, int, int)>(0x004282F7)(this, result, nTI, nPos);
	// }
};


// copy from src\resolution.cpp
// v83
// 0x00BE7918
// v95
// 0x00C64068
class CWvsContext : public TSingleton<CWvsContext, 0x00BE7918> {
public:
	// v83
	// 20A4
	// v95
	// 000020B8     int m_bExclRequestSent;
	MEMBER_AT(int, 0x20A4, m_bExclRequestSent)
	// v83
	// 20A8
	// v95
	// 000020BC     int m_tExclRequestSent;
	MEMBER_AT(int, 0x20A8, m_tExclRequestSent)

	// v83
	// ___:00425D0B ; _DWORD *__thiscall CWvsContext::GetCharacterData(_DWORD *this, _DWORD *result)
	// v95
	// .text:0042B960 ; ZRef<CharacterData> *__thiscall CWvsContext::GetCharacterData(CWvsContext *this, ZRef<CharacterData> *result)
	ZRef<CharacterData> * GetCharacterData( ZRef<CharacterData> *result) {
		return reinterpret_cast<ZRef<CharacterData> *(__thiscall*)(CWvsContext*, ZRef<CharacterData> *)>(0x00425D0B)(this, result);
	}
};

// Inventory tab types (nTI)
// Based on v95 enum
// enum $3681F0796203820C8E23634CF5E1463B : __int32
enum InventoryType {
	IT_EQUIP   = 0x1,
	IT_CONSUME = 0x2,
	IT_INSTALL = 0x3,
	IT_ETC     = 0x4,
	IT_CASH    = 0x5,

	// Aliases / Unknown (kept for compatibility & reversing reference)
	// IT_NO      = 0x5,
	// IT_EXNO    = 0x6,
};


// This is copy from src\wvs\config.h
// v83
// 0x00BEBF9C
// v95
// 0x00C687AC
class CConfig : public TSingleton<CConfig, 0x00BEBF9C> {
public:
	enum {
		GLOBAL_OPT = 0x0,
		LAST_CONNECT_INFO = 0x1,
		CHARACTER_OPT = 0x2,
	};

	// v83
	// 0x0049EF65
	// v93
	// 0x004B25F0
	int GetOpt_Int(int nType, const char* sKey, int nDefaultX, int nLowBound, int nHighBound) {
		return reinterpret_cast<int(__thiscall*)(CConfig*, int, const char*, int, int, int)>(0x0049EF65)(this, nType, sKey, nDefaultX, nLowBound, nHighBound);
	}
	// v83
	// 0x0049EFB5
	// v95
	// 0x004B2650
	void SetOpt_Int(int nType, const char* sKey, int nValue) {
		reinterpret_cast<void(__thiscall*)(CConfig*, int, const char*, int)>(0x0049EFB5)(this, nType, sKey, nValue);
	}

	// // v83
	// // ___:0049F079 ; int __thiscall sub_49F079(CConfig *this, int nType, int, int *)
	// // v95
	// // .text:004B2830 ; void __thiscall CConfig::SetOpt_Binary(CConfig *this, int nType, const char *sKey, ZArray<unsigned char> *abData)
	// void SetOpt_Binary(int nType, const char *sKey, ZArray<unsigned char> *abData) {
	//     reinterpret_cast<void(__thiscall*)(CConfig*, int, const char*, ZArray<unsigned char>*)>(0x0049F079)(this, nType, sKey, abData);
	// }

	// // v83
	// // ___:0049EFDE ; int __thiscall sub_49EFDE(CConfig *this, int nType, int, int arg0)
	// // v95
	// // .text:004B2F00 ; void __thiscall CConfig::GetOpt_Binary(CConfig *this, unsigned int nType, char *sKey, ZArray<unsigned char> *abData)
	// void GetOpt_Binary(int nType, char *sKey, ZArray<unsigned char> *abData) {
	//     reinterpret_cast<void(__thiscall*)(CConfig*, int, const char*, ZArray<unsigned char>*)>(0x0049EFDE)(this, nType, sKey, abData);
	// }

	// v83
	// ___:0049D90D ; void __thiscall CConfig::SaveCharacter(CConfig *this)
	// v95
	// .text:004B5B00 ; void __thiscall CConfig::SaveCharacter(CConfig *this)
	MEMBER_HOOK(void, 0x0049D90D, SaveCharacter)

	// v83
	// ___:0049D0B6 ; void __thiscall CConfig::LoadCharacter(CConfig *this, int nWorldID, unsigned int dwCharacterId)
	// v95
	// .text:004B6C00 ; void __thiscall CConfig::LoadCharacter(CConfig *this, int nWorldID, unsigned int dwCharacterId)
	MEMBER_HOOK(void, 0x0049D0B6, LoadCharacter, int nWorldID, unsigned int dwCharacterId)
};
// from v95
// enum $073F1E0F15323ECCBAD9E6FC5DE95E6F : __int32
enum ChatType
{
	CHAT_TYPE_NORMAL                 = 0x0,
	CHAT_TYPE_WHISPER                = 0x1,
	CHAT_TYPE_GROUPPARTY             = 0x2,
	CHAT_TYPE_GROUPFRIEND            = 0x3,
	CHAT_TYPE_GROUPGUILD             = 0x4,
	CHAT_TYPE_GROUPALLIANCE          = 0x5,
	CHAT_TYPE_COUPLE                 = 0x6,
	CHAT_TYPE_GAMEDESC               = 0x7,
	CHAT_TYPE_TIP                    = 0x8,
	CHAT_TYPE_NOTICE                 = 0x9,
	CHAT_TYPE_NOTICE2                = 0xA,
	CHAT_TYPE_ADMINCHAT              = 0xB,
	CHAT_TYPE_SYSTEM                 = 0xC,
	CHAT_TYPE_SPEAKERCHANNEL         = 0xD,
	CHAT_TYPE_SPEAKERWORLD           = 0xE,
	CHAT_TYPE_SKULLSPEAKER           = 0xF,
	CHAT_TYPE_ITEMSPEAKER            = 0x10,
	CHAT_TYPE_ITEMSPEAKER_ITEM       = 0x11,
	CHAT_TYPE_AVATARMEGAPHONE        = 0x12,
	CHAT_TYPE_GACHAPONANNOUNCE       = 0x13,
	CHAT_TYPE_CASHGACHAPONANNOUNCE   = 0x14,
	CHAT_TYPE_CASHGACHAPON_OPEN_ANNOUNCE = 0x15,
	CHAT_TYPE_CASHGACHAPON_COPY_ANNOUNCE = 0x16,
	CHAT_TYPE_SPEAKERBRIDGE          = 0x17,
	CHAT_TYPE_SPEAKERWORLDEX_PREVIEW = 0x18,
	CHAT_TYPE_MOB                    = 0x19,
	CHAT_TYPE_EXPEDITION             = 0x1A,
	CHAT_TYPE_NO                     = 0x1B,
};
// copy from \src\wvs\statusbar.h
class CUIStatusBar : public CWnd, public TSingleton<CUIStatusBar, 0x00BEC208> {
public:
	// v83
	// ___:008DB070 ; void __thiscall CUIStatusBar::ChatLogAdd(char *this, int sChat, int lType, int nChannelID, int bWhisperIcon, int a6, VARIANTARG *pItem)
	// v95
	// .text:0087AEC0 ; void __thiscall CUIStatusBar::ChatLogAdd(CUIStatusBar *this, const char *sChat, int lType, int nChannelID, int bWhisperIcon, ZRef<GW_ItemSlotBase> pItem)
 void ChatLogAdd(const char* sChat, int lType, int nChannelID, int bWhisperIcon, int a6, void* pItem) {
		reinterpret_cast<void(__thiscall*)(CUIStatusBar*, const char*, int, int, int, int, void*)>(0x008DB070)(this, sChat, lType, nChannelID, bWhisperIcon, a6, pItem);
	}
};

class CCtrlTab : public CCtrlWnd {
public:
	// v95
	// 00000034     int m_bSameWidth;
	// 00000038     int m_nTabWidth;
	// 0000003C     int m_bDrawBaseImage;
	// 00000040     int m_nTabSpace;
	// 00000044     int m_nCurTab;
	// 00000048     ZList<CCtrlTab::TABINFO> m_lTabInfo;
	// 0000005C     _com_ptr_t<_com_IIID<IWzFont,&_GUID_2bef046d_ccd6_445a_88c4_929fc35d30ac> > m_pFontNormal;
	// 00000060     _com_ptr_t<_com_IIID<IWzFont,&_GUID_2bef046d_ccd6_445a_88c4_929fc35d30ac> > m_pFontSelected;
	// 00000064     int m_nInsertPos;
	// 00000068     int m_nType;
	// 0000006C     int m_nHeight;
	// 0~4 need to +1 to match enum InventoryType
	MEMBER_AT(int, 0x3C, m_nCurTab)
};
class CCtrlScrollBar : public CCtrlWnd {
public:
	// v95
	// 00000034     int m_nWheelRange;
	// 00000038     int m_nCurPos;
	// 0000003C     int m_nScrollRange;
	// 00000040     int m_nLastHT;
	// 00000044     unsigned int m_dwLastHT;
	// 00000048     int m_bCapture;
	// 0000004C     int m_grid;
	// 00000050     int m_length;
	// 00000054     int m_hv;
	// 00000058     int m_type;
	// 0000005C     int m_thumbPos;
	// 00000060     int m_thumbX;
	// 00000064     int m_thumbY;
	// 00000068     int m_bHideThumb;
	// 0000006C     int m_bTranslucent;
	// 00000070     ZXString<unsigned short> m_sUOL;
	MEMBER_AT(int, 0x34, m_nWheelRange)
	MEMBER_AT(int, 0x38, m_nCurPos)
	MEMBER_AT(int, 0x68, m_bHideThumb)
};
class CShopDlg {
public:
	struct ITEM {
		// v95
		// 00000000     TSecType<long> nItemID;
		// 0000000C     int nPos;
		// 00000010     Ztl_bstr_t sItemName;
		// 00000014     _com_ptr_t<_com_IIID<IWzCanvas,&_GUID_7600dc6c_9328_4bff_9624_5b0f5c01179e> > pIcon;
		// 00000018     int nPrice;
		// 0000001C     int nDiscountRate;
		// 00000020     int nTokenItemID;
		// 00000024     int nTokenPrice;
		// 00000028     int nItemPeriod;
		// 0000002C     int nLevelLimited;
		// 00000030     long double dUnitPrice;
		// 00000038     int nMaxPerSlot;
		// 0000003C     int nStock;
		// 00000040     int nQuantity;
		// 00000044     ZRef<GW_ItemSlotBase> pItem;
		unsigned char pad0[0x48];
		MEMBER_AT(int, 0xC, nPos)
	};
	// v95
	// 00000094     ZRef<CCtrlButton> m_pBtExit;
	// 0000009C     ZRef<CCtrlButton> m_pBtBuy;
	// 000000A4     ZRef<CCtrlButton> m_pBtSell;
	// 000000AC     ZRef<CCtrlTab> m_pTabBuy;
	// 000000B4     ZRef<CCtrlTab> m_pTabSell;
	// 000000BC     ZArray<ZRef<CCtrlButton> > m_apBtRecharge;
	// 000000C0     ZRef<CCtrlScrollBar> m_pSBBuy;
	// 000000C8     ZRef<CCtrlScrollBar> m_pSBSell;
	// 000000D0     int m_dwNpcTemplateID;
	// 000000D4     int m_bCashNpc;
	// 000000D8     ZArray<CShopDlg::ITEM> m_aBuyItem;
	// 000000DC     ZArray<CShopDlg::ITEM> m_aBuyItem_Recommended;
	// 000000E0     ZArray<long> m_anOriginalIndex;
	// 000000E4     ZArray<CShopDlg::ITEM> m_aRechargeItem;
	// 000000E8     ZArray<CShopDlg::ITEM> m_aSellItem;
	// 000000EC     ZArray<CShopDlg::ITEM> m_aSnapShot;
	// 000000F0     _com_ptr_t<_com_IIID<IWzFont,&_GUID_2bef046d_ccd6_445a_88c4_929fc35d30ac> > m_pFont;
	// 000000F4     _com_ptr_t<_com_IIID<IWzProperty,&_GUID_986515d9_0a0b_4929_8b4f_718682177b92> > m_pImgFontNumber;
	// 000000F8     _com_ptr_t<_com_IIID<IWzCanvas,&_GUID_7600dc6c_9328_4bff_9624_5b0f5c01179e> > m_pCanvasSelect;
	// 000000FC     _com_ptr_t<_com_IIID<IWzCanvas,&_GUID_7600dc6c_9328_4bff_9624_5b0f5c01179e> > m_pCanvasMeso;
	// 00000100     int m_nBuySelected;
	// 00000104     int m_nSellSelected;
	// 00000108     int m_nSnapshotTI;
	// 0000010C     int m_bShopRequestSent;
	// 00000110     int m_nLastSellIdx;
	// 00000114     int m_nLastTab_Buy;
	// 00000118     int m_anBuyStartIdx[2];
	// 00000120     ZRef<CAvatar> m_pAvatar;
	// 00000128     _com_ptr_t<_com_IIID<IWzGr2DLayer,&_GUID_6dc8c7ce_8e81_4420_b4f6_4b60b7d5fcdf> > m_pLayerNPC;
	// 0000012C     CUIToolTip m_uiToolTip;
	// 00000B74     CLayoutMan m_lm;
	// .text:006E56F0 ; void __thiscall CShopDlg::DrawSellItem(CShopDlg *this, _com_ptr_t<_com_IIID<IWzCanvas,&_GUID_7600dc6c_9328_4bff_9624_5b0f5c01179e> > pCanvas)
	// .text:006E7260 ; void __thiscall CShopDlg::SendSellRequest(CShopDlg *this)

	MEMBER_AT(ZRef<CCtrlTab>, 0xA0, m_pTabSell)
	// -1 nonselect 0~4
	MEMBER_AT(int, 0xF4, m_nSellSelected)
	// i dont know what is this maybe using when selling
	MEMBER_AT(int, 0xF8, m_nSnapshotTI) // need check dadress
	MEMBER_AT(ZRef<CCtrlScrollBar>, 0xB4, m_pSBSell)
	MEMBER_AT(ZArray<CShopDlg::ITEM>, 0xD4, m_aSellItem) // need check dadress



	MEMBER_HOOK(void, 0x0075577A, DrawSellItem, IWzCanvasPtr pCanvas)

	MEMBER_HOOK(void, 0x00756A04, SendSellRequest)
};



// CConfig::GetOpt_Binary is bug can't get Binary
// CConfig::GetOpt_String is 256 limit

// made by Chronos
constexpr int kFirstInventoryType = InventoryType::IT_EQUIP;
constexpr int kLastInventoryType = InventoryType::IT_CASH;
constexpr int kMaskCount = (kMaxInventorySlot + 31) / 32;
uint32_t g_slotLockMasks[kLastInventoryType + 1][kMaskCount]{};


// made by Chronos
bool IsValidInventoryType(int inventoryType) {
	return inventoryType >= kFirstInventoryType && inventoryType <= kLastInventoryType;
}

bool IsValidSlot(int slot) {
	return slot >= 1 && slot <= kMaxInventorySlot;
}

bool IsSlotLocked(int inventoryType, int slot) {
	if (!IsValidInventoryType(inventoryType) || !IsValidSlot(slot)) {
		return false;
	}

	const int maskIndex = (slot - 1) / 32;
	const int bitIndex = (slot - 1) % 32;
	return (g_slotLockMasks[inventoryType][maskIndex] & (1u << bitIndex)) != 0;
}

void SetSlotLocked(int inventoryType, int slot, bool locked) {
	if (!IsValidInventoryType(inventoryType) || !IsValidSlot(slot)) {
		return;
	}

	const int maskIndex = (slot - 1) / 32;
	const int bitIndex = (slot - 1) % 32;
	const uint32_t bit = 1u << bitIndex;

	if (locked) {
		g_slotLockMasks[inventoryType][maskIndex] |= bit;
	} else {
		g_slotLockMasks[inventoryType][maskIndex] &= ~bit;
	}
}

void SaveSlotLocks(CConfig *config) {
	if (!config) {
		return;
	}

	char key[32];
	for (int tab = kFirstInventoryType; tab <= kLastInventoryType; ++tab) {
		for (int maskIndex = 0; maskIndex < kMaskCount; ++maskIndex) {
			std::snprintf(key, sizeof(key), "SlotLock_%d_%d", tab, maskIndex);
			config->SetOpt_Int(CConfig::CHARACTER_OPT, key, static_cast<int32_t>(g_slotLockMasks[tab][maskIndex]));
		}
	}
}

void LoadSlotLocks(CConfig *config) {
	for (auto &masks: g_slotLockMasks) {
		for (uint32_t &mask: masks) {
			mask = 0;
		}
	}

	if (!config) {
		return;
	}

	char key[32];
	for (int tab = kFirstInventoryType; tab <= kLastInventoryType; ++tab) {
		for (int maskIndex = 0; maskIndex < kMaskCount; ++maskIndex) {
			std::snprintf(key, sizeof(key), "SlotLock_%d_%d", tab, maskIndex);
			g_slotLockMasks[tab][maskIndex] = static_cast<uint32_t>(
				config->GetOpt_Int(CConfig::CHARACTER_OPT, key, 0, INT_MIN, INT_MAX));
		}
	}
}
// ~ made by Chronos

void CConfig::SaveCharacter_hook() {
	SaveCharacter(this);
	SaveSlotLocks(this);
}
void CConfig::LoadCharacter_hook(int nWorldID, unsigned int dwCharacterId) {
	LoadCharacter(this, nWorldID, dwCharacterId);
	LoadSlotLocks(this);
}
// v83 00BED654
// v95 00C6ABE0
class CUIItem : CUIWnd, public TSingleton<CUIItem, 0x00BED654> {
public:
	// v95
	// 00000B08     ZRef<CCtrlTab> m_pTab;
	// 00000B10     ZRef<CCtrlScrollBar> m_pSBItem;
	// 00000B18     ZRef<CCtrlOriginButton> m_pBtArrange;
	// 00000B20     ZRef<CCtrlOriginButton> m_pBtExtend;
	// 00000B28     ZRef<CCtrlOriginButton> m_pBtCashShop;
	// 00000B30     int m_nFirstPosition;
	// 00000B34     int m_nItemTI;
	// 00000B38     int m_anSortBtState[6];
	// 00000B50     _com_ptr_t<_com_IIID<IWzProperty,&_GUID_986515d9_0a0b_4929_8b4f_718682177b92> > m_pImgFontNumber;
	// 00000B54     int m_bExtended;
	// 00000B58     _com_ptr_t<_com_IIID<IWzCanvas,&_GUID_7600dc6c_9328_4bff_9624_5b0f5c01179e> > m_pCanvasDisabled;
	// 00000B5C     _com_ptr_t<_com_IIID<IWzGr2DLayer,&_GUID_6dc8c7ce_8e81_4420_b4f6_4b60b7d5fcdf> > m_pLastestItemTabEffectLayer;
	// 00000B60     _com_ptr_t<_com_IIID<IWzGr2DLayer,&_GUID_6dc8c7ce_8e81_4420_b4f6_4b60b7d5fcdf> > m_pLastestItemSlotEffectLayer;
	// 00000B64     int m_nLastestGetItemID;
	// 00000B68     int m_nLastestGetItemPos;
	// 00000B6C     int m_bTryToReleaseItem;
	// 00000B70     int m_nReleaseUItemPos;
	// 00000B74     CLayoutMan m_lm;
	// .text:007CC590 ; void __thiscall CUIItem::OnMouseButton(CUIItem *this, unsigned int msg, unsigned int wParam, int rx, int ry)
	// .text:007CCA90 ; int __thiscall CUIItem::OnMouseMove(CUIItem *this, int rx, int ry)
	// .text:007CCF50 ; void __thiscall CUIItem::Draw(CUIItem *this, const tagRECT *pRect)

	// when scroll down showing first slot number
	MEMBER_AT(int, 0x05E0, m_nFirstPosition)
	// current inv Type
	MEMBER_AT(int, 0x05E4, m_nItemTI)
	MEMBER_AT(int, 0x0604, m_bExtended)


	MEMBER_HOOK(void, 0x0081D42E, OnMouseButton, unsigned int msg, unsigned int wParam, int rx, int ry)
	MEMBER_HOOK(int, 0x0081D8AD, OnMouseMove, int rx, int ry)
	MEMBER_HOOK(void, 0x0081DC20, Draw, const tagRECT *pRect)

	// v83
	// ___:0081DB7E ; int __thiscall CUIItem::GetSlotPositionFromPoint(CUIItem *this, int rx, int ry)
	// v95
	// .text:007CC220 ; int __thiscall CUIItem::GetSlotPositionFromPoint(CUIItem *this, tagPOINT rx)
	int GetSlotPositionFromPoint(int rx, int ry) {
		return reinterpret_cast<int(__thiscall*)(CUIItem*, int, int)>(0x0081DB7E)(this, rx, ry);
	}
	// v83
	// ___:0081E2C8 ; int __thiscall CUIItem::GetItemSlotRect(CUIItem *this, int nSlotPosition, tagRECT *pRc)
	// v95
	// .text:007CBE90 ; int __thiscall CUIItem::GetItemSlotRect(CUIItem *this, int nSlotPosition, tagRECT *pRc)
	int GetItemSlotRect(int nSlotPosition, tagRECT *pRc) {
		return reinterpret_cast<int(__thiscall*)(CUIItem*, int, tagRECT*)>(0x0081E2C8)(this, nSlotPosition, pRc);
	}
};

static struct DragLockState {
	bool active = false;
	bool lockMode = false;      // true=lock, false=unlock
	int invType = 0;
	void Reset() {
		active = false;
		lockMode = false;
		invType = 0;
	}
} s_DragState;

void CUIItem::OnMouseButton_hook(unsigned int msg, unsigned int wParam, int rx, int ry) {
	if (msg == WM_RBUTTONDOWN) {
		// Decompiled: GetSlotPositionFromPoint receives (this - 4) as CUIItem*
		// The hook's `this` points into the vtable dispatch, -4 corrects to the actual object base
		CUIItem* ui = (CUIItem*)((char*)this - 4);

		int slotPos = ui->GetSlotPositionFromPoint(rx, ry);
		int inventoryType = ui->m_nItemTI;

		if (IsValidInventoryType(inventoryType) && IsValidSlot(slotPos)) {
			if (GetKeyState(VK_CONTROL) & 0x8000) {
				// Ctrl + right-click = toggle the slot lock (this was a plain right-click before).
				const auto currentLockState = IsSlotLocked(inventoryType, slotPos);
				SetSlotLocked(inventoryType, slotPos, !currentLockState);
				SaveSlotLocks(CConfig::GetInstance());

				if (GetKeyState(kDragLockKey) & 0x8000) {
					s_DragState.Reset();
					s_DragState.active = true;
					s_DragState.lockMode = !currentLockState;
					s_DragState.invType = inventoryType;
				}

				// Trigger a redraw of the UI components
				ui->InvalidateRect(nullptr);
			}
			// Plain right-click (no Ctrl): if the Storage Bag is OPEN, deposit this item into its category
			// bag and consume the click. (Bag closed -> falls through, so a plain right-click does nothing.)
			else if (::BagWindow_DepositFromInventory(inventoryType, slotPos)) {
				ui->InvalidateRect(nullptr);
				return;
			}
		}
	}
	// done drag
	else if (msg == WM_RBUTTONUP) {
		s_DragState.Reset();
	}

	// DEBUG_MESSAGE("OnMouseButton_hook | msg: 0x%X, wParam: 0x%X, rx: %d, ry: %d", msg, wParam, rx, ry);
	OnMouseButton(this, msg, wParam, rx, ry);
}


int CUIItem::OnMouseMove_hook(int rx, int ry) {
	// early exit if not dragging or key released
	if (!s_DragState.active || !(GetKeyState(kDragLockKey) & 0x8000)) {
		if (s_DragState.active)
			s_DragState.Reset();
		return OnMouseMove(this, rx, ry);
	}

	CUIItem* ui = (CUIItem*)((char*)this - 4);

	const auto currentInventoryType = ui->m_nItemTI;
	// skip if moved to different inventory
	if (currentInventoryType != s_DragState.invType)
		return OnMouseMove(this, rx, ry);

	int slotPos = ui->GetSlotPositionFromPoint(rx, ry);

	// apply lock/unlock to unvisited slots only

	if (IsValidSlot(slotPos) && s_DragState.lockMode != IsSlotLocked(currentInventoryType, slotPos)) {
		SetSlotLocked(currentInventoryType, slotPos, s_DragState.lockMode);
		SaveSlotLocks(CConfig::GetInstance());
		ui->InvalidateRect(nullptr);
	}

	return OnMouseMove(this, rx, ry);
}


// v83
// ___:0049637B ; void __fastcall CClientSocket::SendPacket(void *arg0)
// v95
// .text:004AF9F0 ; void __fastcall CClientSocket::SendPacket(CClientSocket *this, int, const COutPacket *oPacket)
constexpr auto CClientSocket_SendPacket = 0x0049637B;


// made by Chronos
static void AppendSlotLockData(COutPacket* pPacket) {
	const int inventoryType = CUIItem::GetInstance()->m_nItemTI;

	if (!pPacket || !IsValidInventoryType(inventoryType)) {
		if (pPacket) {
			pPacket->Encode1(0);
		}
		return;
	}

	uint8_t count = 0;
	for (int slot = 1; slot <= kMaxInventorySlot; ++slot) {
		if (IsSlotLocked(inventoryType, slot)) {
			++count;
		}
	}

	pPacket->Encode1(count);
	for (int slot = 1; slot <= kMaxInventorySlot; ++slot) {
		if (IsSlotLocked(inventoryType, slot)) {
			pPacket->Encode1(static_cast<uint8_t>(slot));
		}
	}
}

// v83
// ___:00A08FD5     call    ?SendPacket@CClientSocket@@QAEXABVCOutPacket@@@Z ; Call Procedure
// v95
// .text:009D5D07                 call    ?SendPacket@CClientSocket@@QAEXABVCOutPacket@@@Z ; CClientSocket::SendPacket(COutPacket const &)
constexpr auto CWvsContext_SendSortItemRequest_SendPacket_call = 0x00A08FD5;

void __fastcall CWvsContext_SendSortItemRequest_SendPacket_hook(void* pSocket, void* edx, COutPacket* pPacket) {
	AppendSlotLockData(pPacket);

	// CClientSocket::SendPacket(CClientSocket::GetInstance(), oPacket);
	reinterpret_cast<void(__thiscall*)(CClientSocket*, const COutPacket&)>(CClientSocket_SendPacket)((CClientSocket*)pSocket, *pPacket);
}

// v83
// ___:00A08F43     call    ?SendPacket@CClientSocket@@QAEXABVCOutPacket@@@Z ; Call Procedure
// v95
// .text:009D5C17                 call    ?SendPacket@CClientSocket@@QAEXABVCOutPacket@@@Z ; CClientSocket::SendPacket(COutPacket const &)
constexpr auto CWvsContext_SendGatherItemRequest_SendPacket_call = 0x00A08F43;

void __fastcall CWvsContext_SendGatherItemRequest_SendPacket_hook(void* pSocket, void* edx, COutPacket* pPacket) {
	AppendSlotLockData(pPacket);

	// CClientSocket::SendPacket(CClientSocket::GetInstance(), oPacket);
	reinterpret_cast<void(__thiscall*)(CClientSocket*, const COutPacket&)>(CClientSocket_SendPacket)((CClientSocket*)pSocket, *pPacket);
}

// made by Chronos
void DrawLockBorder(IWzCanvas* canvas, const tagRECT& rc, int thickness, uint32_t color) {
	if (!canvas) {
		return;
	}

	const int width = rc.right - rc.left;
	const int height = rc.bottom - rc.top;
	if (width <= 0 || height <= 0) {
		return;
	}

	canvas->raw_DrawRectangle(rc.left, rc.top, width, thickness, color);
	canvas->raw_DrawRectangle(rc.left, rc.bottom - thickness, width, thickness, color);
	canvas->raw_DrawRectangle(rc.left, rc.top, thickness, height, color);
	canvas->raw_DrawRectangle(rc.right - thickness, rc.top, thickness, height, color);
}

// made by Chronos
void CUIItem::Draw_hook(const tagRECT *pRect) {
	Draw(this, pRect);

	const int inventoryType = m_nItemTI;
	if (!IsValidInventoryType(inventoryType)) {
		return;
	}

	IWzCanvasPtr canvas = reinterpret_cast<CWnd *>(this)->GetCanvas();
	if (!canvas) {
		return;
	}

	const int firstSlot = m_nFirstPosition;
	const int visibleSlots = m_bExtended ? kMaxInventorySlot : 24;
	for (int slot = firstSlot; slot < firstSlot + visibleSlots && slot <= kMaxInventorySlot; ++slot) {
		if (!IsSlotLocked(inventoryType, slot)) {
			continue;
		}

		tagRECT rc{};
		if (GetItemSlotRect(slot, &rc)) {
			DrawLockBorder(canvas, rc, kInventoryDrawThickness, kInventoryLockBorderColor);
		}
	}
}

// made by mujy
void CShopDlg::DrawSellItem_hook(IWzCanvasPtr pCanvas) {
	DrawSellItem(this, pCanvas);
	if (!pCanvas) return;

	if (!m_pSBSell) return;
	const int scrollBarCurPos = m_pSBSell->m_nCurPos;
	if (scrollBarCurPos < 0) return;

	const auto currentInventoryType = m_pTabSell->m_nCurTab + 1;
	// DEBUG_MESSAGE("currentTab: %d",currentInventoryType);

	const auto& aSellData = m_aSellItem;

	// v95
	// constexpr int SHOP_SELL_X       = 241;
	// constexpr int SHOP_SELL_Y_FIRST = 115;
	// constexpr int SHOP_SELL_STRIDE  = 42;
	// constexpr int SHOP_SELL_Y_END   = 325;
	// constexpr int SHOP_SELL_ROW_W   = 220;
	// constexpr int SHOP_SELL_ROW_H   = 36;

	// v83
	constexpr int SHOP_SELL_X       = 236;
	constexpr int SHOP_SELL_Y_FIRST = 146 - 19;
	constexpr int SHOP_SELL_STRIDE  = 40;
	constexpr int SHOP_SELL_Y_END   = SHOP_SELL_Y_FIRST + (40 * 5); // made by gpt
	constexpr int SHOP_SELL_ROW_W   = 200; // calculated with eyeballs
	constexpr int SHOP_SELL_ROW_H   = 35; // need check

	int rowIdx = 0;
	for (int y = SHOP_SELL_Y_FIRST; y < SHOP_SELL_Y_END; y += SHOP_SELL_STRIDE, ++rowIdx) {
		unsigned int actualIdx = scrollBarCurPos + rowIdx;
		if (actualIdx >= aSellData.GetCount()) break;
		const auto& item = aSellData[actualIdx];
		const int nPos = item.nPos;
		// DEBUG_MESSAGE("nPos:%d",nPos);
		if (!IsSlotLocked(currentInventoryType, nPos)) continue;

		RECT rc{
			SHOP_SELL_X,
			y,
			SHOP_SELL_X + SHOP_SELL_ROW_W,
			y + SHOP_SELL_ROW_H };
		DrawLockBorder(pCanvas, rc, kShopDrawThickness, kShopLockBorderColor);
	}

}

// made by mujy
void AddChatHint(const char* msg) {
	if (auto pBar = CUIStatusBar::GetInstance(); pBar) {
		pBar->ChatLogAdd(msg, kSellBlockMessageType, -1, 0, 0, nullptr);
	}
}
// made by mujy
void CShopDlg::SendSellRequest_hook() {
	const auto currentInventoryType = m_pTabSell->m_nCurTab + 1;
	int nSellSelected = m_nSellSelected;
	const auto& aSellData = m_aSellItem;
	if (nSellSelected >= 0 && !aSellData.IsEmpty()) {
		int aCount = aSellData.GetCount();
		if (nSellSelected < aCount) {
			const auto& item = aSellData[nSellSelected];
			const int nPos = item.nPos;
			if (IsSlotLocked(currentInventoryType, nPos)) {
				AddChatHint(kSellBlockedMessage);
				// printf("[SlotLock] sell blocked tab=%d slot=%d\n",
				//        currentInventoryType, nPos);
				return;
			}
		}
	}
	SendSellRequest(this);
}


void AttachSlotLockModInternal() {
	ATTACH_HOOK(CUIItem::OnMouseButton, CUIItem::OnMouseButton_hook);
	ATTACH_HOOK(CUIItem::OnMouseMove, CUIItem::OnMouseMove_hook);

	PatchCall(CWvsContext_SendGatherItemRequest_SendPacket_call, CWvsContext_SendGatherItemRequest_SendPacket_hook);
	PatchCall(CWvsContext_SendSortItemRequest_SendPacket_call, CWvsContext_SendSortItemRequest_SendPacket_hook);

	ATTACH_HOOK(CUIItem::Draw, CUIItem::Draw_hook);
	ATTACH_HOOK(CShopDlg::DrawSellItem, CShopDlg::DrawSellItem_hook);

	if (kPreventSellingLockedItems) {
		ATTACH_HOOK(CShopDlg::SendSellRequest, CShopDlg::SendSellRequest_hook);
	}

	ATTACH_HOOK(CConfig::SaveCharacter, CConfig::SaveCharacter_hook);
	ATTACH_HOOK(CConfig::LoadCharacter, CConfig::LoadCharacter_hook);
}

};

void AttachSlotLockMod() {
	SlotLock::AttachSlotLockModInternal();
}
