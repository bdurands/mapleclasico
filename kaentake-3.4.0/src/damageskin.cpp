#include "pch.h"
#include "hook.h"
#include "debug.h"
#include "damageskin.h"
#include "wvs/packet.h"
#include "wvs/util.h"
#include "ztl/ztl.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <map>
#include <unordered_map>
#include <vector>
#include <windows.h>


void AttachDamageSkinNet();
void AttachUnitDamageRendering();


// ===========================================================================
// Damage Skin — in-game renderer + network glue.
//
// At load time we enumerate Effect/BasicEff.img/damageSkin/<id> into a map.
// At render time CAnimationDisplayer::Effect_HP picks a sprite pair based
// on lColorType (player damage = 0, mob → player = 1 or 2).
//
// We splice the lColorType == 0 branches to substitute the active skin's
// sprite pair into Effect_HP's local IWzPropertyPtrs. Skin selection is
// driven by g_nDamageSkin, which CMob::OnHit's hook sets to the attacker's
// skin for the duration of that mob's hit-processing call.
// ===========================================================================


// ---------------------------------------------------------------------------
// v83 addresses
// ---------------------------------------------------------------------------

// Effect_HP branch splices. Player damage fires under lColorType == 0 with
// bCriticalAttack toggling between the Normal and Crit branches.
static constexpr uintptr_t kSplice_Normal_Site = 0x00437DA2;
static constexpr uintptr_t kSplice_Crit_Site = 0x00437D8C;
static constexpr uintptr_t kSplice_MergeRet = 0x00437DB6;

// 6-byte `jz loc_4382F6` at 0x00438166 guards the crit Effect-lookup block.
// We overwrite it with `jmp rel32 + nop` so Effect_HP always skips that
// block — skin WZ doesn't provide a NoCri1/Effect child. Cost: crits lose
// the stock glow overlay; digits themselves still render fine.
static constexpr uintptr_t kPatch_SkipEffect_Site = 0x00438166;
static constexpr uintptr_t kPatch_SkipEffect_Tgt = 0x004382F6;

// Damage-number clipping. Effect_HP has three hardcoded imm8 constants that
// jointly control the scratch canvas and sprite positioning. Bumping the
// canvas height alone is insufficient — the blit Y anchor and screen offset
// must be adjusted together to preserve the anchor's absolute screen Y.
//
//   0x0043805F  39 → 7F : canvas Init() height           (57 → 127)
//   0x00438355  39 → 64 : blit Y anchor inside canvas    (57 → 100)
//   0x004383F6  D1 → A6 : screen render top offset       (-47 → -90)
//
// Net effect: sprite gets 100 rows of headroom above its origin (was 57)
// and 27 below (was 0).
static constexpr uintptr_t kPatch_CanvasH_Addr = 0x0043805F;
static constexpr uint8_t kPatch_CanvasH_Val = 0x7F;
static constexpr uintptr_t kPatch_BlitAnchor_Addr = 0x00438355;
static constexpr uint8_t kPatch_BlitAnchor_Val = 0x64;
static constexpr uintptr_t kPatch_ScreenOfs_Addr = 0x004383F6;
static constexpr uint8_t kPatch_ScreenOfs_Val = 0xA6;

// Effect_Miss sprite splice. The default-miss branch at 0x00438A51 loads
// the mob's NoRed0 ([esi+0x170]); we redirect it to the skin's NoRed0
// (which carries the Miss child). Alternate-miss path is left untouched.
static constexpr uintptr_t kSplice_Miss_Site = 0x00438A51;
static constexpr uintptr_t kSplice_Miss_Ret = 0x00438A57;

// CAnimationDisplayer sprite field offsets (from `this`).
static constexpr uintptr_t kOff_No0 = 0x170;    // regular small
static constexpr uintptr_t kOff_No1 = 0x174;    // regular large
static constexpr uintptr_t kOff_NoCri0 = 0x188; // crit small
static constexpr uintptr_t kOff_NoCri1 = 0x18C; // crit large

// _com_ptr_t::operator=(IUnknown*) — assigns with AddRef/Release. The
// engine uses this to store IWzProperty* into Effect_HP's stack locals.
static auto SmartPtrAssign =
        reinterpret_cast<void(__thiscall*)(void* /*pDest*/, IUnknown* /*pSrc*/)>(0x004027BE);


// ---------------------------------------------------------------------------
// Loaded skins
// ---------------------------------------------------------------------------

std::map<int, DamageSkinProp> g_mDamageSkinProp;
std::vector<int> g_vSkinIds;
int g_nDamageSkin = 0;

void SetActiveDamageSkin(int nID) { g_nDamageSkin = nID; }


static bool g_bLoaded = false;

void LoadDamageSkin() {
    if (g_bLoaded)
        return;
    g_bLoaded = true;

    Ztl_variant_t vEmpty;
    Ztl_variant_t vDamageSkin;
    HRESULT hr = S_OK;
    try {
        hr = get_rm()->raw_GetObject(
                const_cast<wchar_t*>(L"Effect/BasicEff.img/damageSkin"),
                vEmpty, vEmpty, &vDamageSkin);
    } catch (...) {
        return;
    }
    if (FAILED(hr) || vDamageSkin.vt != VT_UNKNOWN)
        return;

    IWzPropertyPtr pDamageSkin(vDamageSkin.GetUnknown(false, false));
    if (!pDamageSkin)
        return;

    IUnknownPtr pEnumUnknown;
    if (FAILED(pDamageSkin->get__NewEnum(&pEnumUnknown)))
        return;
    IEnumVARIANTPtr pEnum(pEnumUnknown);
    if (!pEnum)
        return;

    while (true) {
        VARIANT rgVar[1];
        ULONG uFetched = 0;
        if (FAILED(pEnum->Next(1, rgVar, &uFetched)) || uFetched == 0)
            break;
        if (rgVar[0].vt != VT_BSTR || !rgVar[0].bstrVal)
            continue;

        int nID = wcstol(rgVar[0].bstrVal, nullptr, 10);
        Ztl_variant_t vProp;
        if (FAILED(pDamageSkin->get_item(rgVar[0].bstrVal, &vProp)))
            continue;
        IWzPropertyPtr pProp(vProp.GetUnknown(false, false));
        if (!pProp)
            continue;

        DamageSkinProp prop;
        Ztl_variant_t vNoRed0, vNoRed1, vNoCri0, vNoCri1;
        if (SUCCEEDED(pProp->get_item(const_cast<wchar_t*>(L"NoRed0"), &vNoRed0)))
            prop.pNoRed0 = IWzPropertyPtr(vNoRed0.GetUnknown(false, false));
        if (SUCCEEDED(pProp->get_item(const_cast<wchar_t*>(L"NoRed1"), &vNoRed1)))
            prop.pNoRed1 = IWzPropertyPtr(vNoRed1.GetUnknown(false, false));
        if (SUCCEEDED(pProp->get_item(const_cast<wchar_t*>(L"NoCri0"), &vNoCri0)))
            prop.pNoCri0 = IWzPropertyPtr(vNoCri0.GetUnknown(false, false));
        if (SUCCEEDED(pProp->get_item(const_cast<wchar_t*>(L"NoCri1"), &vNoCri1)))
            prop.pNoCri1 = IWzPropertyPtr(vNoCri1.GetUnknown(false, false));

        // Optional unit-damage glyphs. Only treated as a unit skin when
        // NoCustom/customType == L"glUnit"; some skins ship a NoCustom node
        // for other purposes and we don't want K/M/B substitution on those.
        Ztl_variant_t vCustom;
        if (SUCCEEDED(pProp->get_item(const_cast<wchar_t*>(L"NoCustom"), &vCustom))) {
            IWzPropertyPtr pCustom(vCustom.GetUnknown(false, false));
            if (pCustom) {
                bool bIsGlUnit = false;
                Ztl_variant_t vType;
                if (SUCCEEDED(pCustom->get_item(
                            const_cast<wchar_t*>(L"customType"), &vType)) &&
                        vType.vt == VT_BSTR && vType.bstrVal) {
                    bIsGlUnit = (wcscmp(vType.bstrVal, L"glUnit") == 0);
                }

                if (bIsGlUnit) {
                    Ztl_variant_t vCR0, vCC0;
                    if (SUCCEEDED(pCustom->get_item(const_cast<wchar_t*>(L"NoRed0"), &vCR0)))
                        prop.pNoCustomRed0 = IWzPropertyPtr(vCR0.GetUnknown(false, false));
                    if (SUCCEEDED(pCustom->get_item(const_cast<wchar_t*>(L"NoCri0"), &vCC0)))
                        prop.pNoCustomCri0 = IWzPropertyPtr(vCC0.GetUnknown(false, false));
                    prop.bHasCustom = prop.pNoCustomRed0 && prop.pNoCustomCri0;
                }
            }
        }

        if (prop.pNoRed0 && prop.pNoRed1 && prop.pNoCri0 && prop.pNoCri1) {
            g_mDamageSkinProp[nID] = prop;
            g_vSkinIds.push_back(nID);
        }
    }

    AttachUnitDamageRendering();
}


// ---------------------------------------------------------------------------
// Branch splice helpers
//
// Each helper is the replacement for a single branch of Effect_HP's
// lColorType switch. It writes the selected sprite0 to var_18 (via the
// engine's smart-ptr assign) and returns sprite1 in eax for the merge
// point to push and consume into var_20.
//
// Stack shape at merge point 0x00437DB6:
//   top of stack = sprite1 (to be assigned to var_20)
//   var_18 already populated by the helper
// Registers: esi = CAnimationDisplayer*, ebp = frame pointer.
// ---------------------------------------------------------------------------

struct SpritePair {
    IWzProperty* pLarge;
    IWzProperty* pSmall;
};

// Player normal damage. For unit-damage skins we hand the wrapper out for
// both small and large slots so every glyph in the run renders at the same
// (small) size — NoCustom only ships the NoRed0 variant.
static inline SpritePair PickNormal(void* pDisp) {
    if (g_nDamageSkin != 0) {
        auto it = g_mDamageSkinProp.find(g_nDamageSkin);
        if (it != g_mDamageSkinProp.end()) {
            if (it->second.bHasCustom && it->second.pWrapNormal) {
                auto* w = static_cast<IWzProperty*>(it->second.pWrapNormal);
                return { w, w };
            }
            return { it->second.pNoRed1, it->second.pNoRed0 };
        }
    }
    auto fields = reinterpret_cast<char*>(pDisp);
    return {
        *reinterpret_cast<IWzProperty**>(fields + kOff_No1),
        *reinterpret_cast<IWzProperty**>(fields + kOff_No0),
    };
}

// Player crit damage. Safe to replace the crit pair only because we also
// patch out the downstream NoCri1/Effect lookup (kPatch_SkipEffect_Site).
static inline SpritePair PickCrit(void* pDisp) {
    if (g_nDamageSkin != 0) {
        auto it = g_mDamageSkinProp.find(g_nDamageSkin);
        if (it != g_mDamageSkinProp.end()) {
            if (it->second.bHasCustom && it->second.pWrapCrit) {
                auto* w = static_cast<IWzProperty*>(it->second.pWrapCrit);
                return { w, w };
            }
            return { it->second.pNoCri1, it->second.pNoCri0 };
        }
    }
    auto fields = reinterpret_cast<char*>(pDisp);
    return {
        *reinterpret_cast<IWzProperty**>(fields + kOff_NoCri1),
        *reinterpret_cast<IWzProperty**>(fields + kOff_NoCri0),
    };
}


// Gate flag handed from the splice helper to the Format hook. Set when the
// helper actually installs the unit-damage wrapper on this Effect_HP call,
// cleared after Format consumes it. The Format hook MUST only rewrite the
// damage string when this is set — otherwise (mob → player damage path
// reaches Format without going through our splice) the encoded ':' / ';'
// bytes would land in the engine's digit loop against the mob's default
// digit sheet, which has no NoCustom mapping for them, and the resulting
// invalid IUnknowns get queued into the animation layer and AV inside
// oleaut32!VariantClear ~2 seconds later when the layer cleans up.
static volatile LONG g_bSpliceWrapperActive = 0;

static IWzProperty* Normal_helper_impl(void* pDisp, void* pVar18) {
    LoadDamageSkin();
    SpritePair sp = PickNormal(pDisp);

    bool bIsWrapper = false;
    if (g_nDamageSkin != 0) {
        auto it = g_mDamageSkinProp.find(g_nDamageSkin);
        if (it != g_mDamageSkinProp.end() &&
                it->second.bHasCustom &&
                sp.pSmall == it->second.pWrapNormal) {
            bIsWrapper = true;
        }
    }
    InterlockedExchange(&g_bSpliceWrapperActive, bIsWrapper ? 1 : 0);

    SmartPtrAssign(pVar18, sp.pLarge);
    return sp.pSmall;
}

static IWzProperty* __stdcall Normal_helper(void* pDisp, void* pVar18) {
    __try {
        return Normal_helper_impl(pDisp, pVar18);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_bSpliceWrapperActive, 0);
        return nullptr;
    }
}

static IWzProperty* Crit_helper_impl(void* pDisp, void* pVar18) {
    LoadDamageSkin();
    SpritePair sp = PickCrit(pDisp);

    bool bIsWrapper = false;
    if (g_nDamageSkin != 0) {
        auto it = g_mDamageSkinProp.find(g_nDamageSkin);
        if (it != g_mDamageSkinProp.end() &&
                it->second.bHasCustom &&
                sp.pSmall == it->second.pWrapCrit) {
            bIsWrapper = true;
        }
    }
    InterlockedExchange(&g_bSpliceWrapperActive, bIsWrapper ? 1 : 0);

    SmartPtrAssign(pVar18, sp.pLarge);
    return sp.pSmall;
}

static IWzProperty* __stdcall Crit_helper(void* pDisp, void* pVar18) {
    __try {
        return Crit_helper_impl(pDisp, pVar18);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        InterlockedExchange(&g_bSpliceWrapperActive, 0);
        return nullptr;
    }
}

// Effect_Miss helper. Returns the skin's NoRed0 (carries Miss child) when
// a skin is active, otherwise the mob's NoRed0 at [pDisp + kOff_No0].
static IWzProperty* Miss_helper_impl(void* pDisp) {
    LoadDamageSkin();
    if (g_nDamageSkin != 0) {
        auto it = g_mDamageSkinProp.find(g_nDamageSkin);
        if (it != g_mDamageSkinProp.end() && it->second.pNoRed0) {
            return it->second.pNoRed0;
        }
    }
    return *reinterpret_cast<IWzProperty**>(
            reinterpret_cast<char*>(pDisp) + kOff_No0);
}

static IWzProperty* __stdcall Miss_helper(void* pDisp) {
    __try {
        return Miss_helper_impl(pDisp);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return nullptr;
    }
}


// Each naked splice replaces the first instruction of its branch with a
// 5-byte rel32 jmp into this trampoline. The trampoline calls the helper,
// pushes the small sprite, and jumps to the engine's merge point which
// consumes it into var_20.

static void __declspec(naked) Normal_splice_hook() {
    __asm {
        lea     eax, [ ebp - 0x18 ]
        push    eax
        push    esi
        call    Normal_helper
        push    eax
        jmp     [ kSplice_MergeRet ]
    }
}

static void __declspec(naked) Crit_splice_hook() {
    __asm {
        lea     eax, [ ebp - 0x18 ]
        push    eax
        push    esi
        call    Crit_helper
        push    eax
        jmp     [ kSplice_MergeRet ]
    }
}

// Effect_Miss default-branch replacement. Original at 0x00438A51 is a
// 6-byte `mov eax, [esi+170h]`; we overwrite the first 5 bytes with a jmp
// here and the 6th becomes dead. Helper returns the sprite in eax,
// matching what the original mov produced; merge at 0x00438A57.
static void __declspec(naked) Miss_splice_hook() {
    __asm {
        push    esi
        call    Miss_helper
        jmp     [ kSplice_Miss_Ret ]
    }
}


// ---------------------------------------------------------------------------
// Attach
// ---------------------------------------------------------------------------

static void PatchSkipEffectBlock() {
    auto* p = reinterpret_cast<unsigned char*>(kPatch_SkipEffect_Site);
    DWORD old, restore;
    VirtualProtect(p, 6, PAGE_EXECUTE_READWRITE, &old);
    p[0] = 0xE9;
    *reinterpret_cast<int32_t*>(p + 1) =
            static_cast<int32_t>(kPatch_SkipEffect_Tgt - (kPatch_SkipEffect_Site + 5));
    p[5] = 0x90;
    VirtualProtect(p, 6, old, &restore);
}

static void PatchImm8(uintptr_t uAddr, uint8_t uVal) {
    auto* p = reinterpret_cast<unsigned char*>(uAddr);
    DWORD old, restore;
    VirtualProtect(p, 1, PAGE_EXECUTE_READWRITE, &old);
    *p = uVal;
    VirtualProtect(p, 1, old, &restore);
}

// Expand the damage-number scratch canvas so tall skin sprites render
// fully. All three sites must be patched together to preserve the
// anchor's absolute screen Y.
static void PatchDamageCanvas() {
    PatchImm8(kPatch_CanvasH_Addr, kPatch_CanvasH_Val);
    PatchImm8(kPatch_BlitAnchor_Addr, kPatch_BlitAnchor_Val);
    PatchImm8(kPatch_ScreenOfs_Addr, kPatch_ScreenOfs_Val);
}

void AttachDamageSkinMod() {
    PatchJmp(kSplice_Normal_Site, &Normal_splice_hook);
    PatchJmp(kSplice_Crit_Site, &Crit_splice_hook);
    PatchJmp(kSplice_Miss_Site, &Miss_splice_hook);
    PatchSkipEffectBlock();
    PatchDamageCanvas();
    AttachDamageSkinNet();
}


// ===========================================================================
// Network — server↔client packets for catalog / inventory / apply / purchase
// / broadcast. Wire formats match Server-v83/.../PacketCreator.java.
// ===========================================================================

static constexpr uintptr_t kAddr_ClientSocket_ProcessPacket = 0x004965F1;
static constexpr uintptr_t kAddr_ClientSocket_SendPacket = 0x0049637B;
static constexpr uintptr_t kAddr_ClientSocket_Instance = 0x00BE7914;
static constexpr uintptr_t kAddr_InPacket_Decode1 = 0x004065F3;
static constexpr uintptr_t kAddr_InPacket_Decode2 = 0x0042470C;
static constexpr uintptr_t kAddr_InPacket_Decode4 = 0x00406629;
static constexpr uintptr_t kAddr_InPacket_DecodeBuffer = 0x00432257;

static auto InPacket_Decode1 =
        reinterpret_cast<uint8_t(__thiscall*)(CInPacket*)>(kAddr_InPacket_Decode1);
static auto InPacket_Decode2 =
        reinterpret_cast<uint16_t(__thiscall*)(CInPacket*)>(kAddr_InPacket_Decode2);
static auto InPacket_Decode4 =
        reinterpret_cast<uint32_t(__thiscall*)(CInPacket*)>(kAddr_InPacket_Decode4);
static auto InPacket_DecodeBuffer =
        reinterpret_cast<void(__thiscall*)(CInPacket*, void*, unsigned int)>(
                kAddr_InPacket_DecodeBuffer);

static inline uint64_t InPacket_Decode8(CInPacket* p) {
    uint64_t lo = InPacket_Decode4(p);
    uint64_t hi = InPacket_Decode4(p);
    return lo | (hi << 32);
}

// Offset of m_uOffset inside CInPacket (size_t at 0x14, verified by
// static_assert(sizeof(CInPacket) == 0x18) in wvs/packet.h).
static constexpr size_t kInPacket_OffsetField = 0x14;
static inline size_t& PacketOffset(CInPacket* p) {
    return *reinterpret_cast<size_t*>(reinterpret_cast<char*>(p) + kInPacket_OffsetField);
}

typedef void(__thiscall* t_SendPacket)(void* pClientSocket, COutPacket* pPacket);
static auto ClientSocket_SendPacket =
        reinterpret_cast<t_SendPacket>(kAddr_ClientSocket_SendPacket);

static void DmgSendPacket(COutPacket& oPacket) {
    void* pSocket = *reinterpret_cast<void**>(kAddr_ClientSocket_Instance);
    if (pSocket)
        ClientSocket_SendPacket(pSocket, &oPacket);
}

// Engine's native single-button Notice dialog (same surface the picker
// uses for "already owned"). Used by OnPacket_Result to confirm a
// successful server-side apply.
typedef int(__cdecl* t_CUtilDlg_Notice)(
        ZXString<char>, const wchar_t*, void*, int, int);
static auto CUtilDlg_Notice =
        reinterpret_cast<t_CUtilDlg_Notice>(0x009929DD);


// ---------------------------------------------------------------------------
// Opcodes — must match Server-v83/.../SendOpcode.java and RecvOpcode.java
// ---------------------------------------------------------------------------
static constexpr uint16_t kOp_S2C_Catalog = 0x170;
static constexpr uint16_t kOp_S2C_Inventory = 0x171;
static constexpr uint16_t kOp_S2C_Result = 0x172;
static constexpr uint16_t kOp_S2C_Broadcast = 0x173;
static constexpr uint16_t kOp_C2S_Apply = 0x110;
static constexpr uint16_t kOp_C2S_Purchase = 0x111;


// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
std::vector<DamageSkinCatalogEntry> g_vShopCatalog;
std::vector<int> g_vOwnedSkins;
int g_nLocalCharId = 0;

static std::unordered_map<int, int> g_mCharIdToSkin;

int GetSkinForChar(int nCharId) {
    auto it = g_mCharIdToSkin.find(nCharId);
    return it == g_mCharIdToSkin.end() ? 0 : it->second;
}
void SetSkinForChar(int nCharId, int nSkinId) {
    if (nSkinId == 0)
        g_mCharIdToSkin.erase(nCharId);
    else
        g_mCharIdToSkin[nCharId] = nSkinId;
}


// ---------------------------------------------------------------------------
// Send helpers
// ---------------------------------------------------------------------------
void Send_DamageSkinApply(int nSkinId) {
    COutPacket p(kOp_C2S_Apply);
    p.Encode4(static_cast<unsigned int>(nSkinId));
    DmgSendPacket(p);
}

void Send_DamageSkinPurchase(int nSkinId) {
    COutPacket p(kOp_C2S_Purchase);
    p.Encode4(static_cast<unsigned int>(nSkinId));
    DmgSendPacket(p);
}


// ---------------------------------------------------------------------------
// Receive handlers
// ---------------------------------------------------------------------------
static void OnPacket_Catalog(CInPacket* p) {
    uint16_t n = InPacket_Decode2(p);
    g_vShopCatalog.clear();
    g_vShopCatalog.reserve(n);
    for (uint16_t i = 0; i < n; ++i) {
        DamageSkinCatalogEntry e;
        e.nID = (int)InPacket_Decode4(p);
        e.llPrice = (long long)InPacket_Decode8(p);
        g_vShopCatalog.push_back(e);
    }
    RefreshDamageSkinPicker();
}

static void OnPacket_Inventory(CInPacket* p) {
    int nActive = (int)InPacket_Decode4(p);
    uint16_t n = InPacket_Decode2(p);
    g_vOwnedSkins.clear();
    g_vOwnedSkins.reserve(n);
    for (uint16_t i = 0; i < n; ++i) {
        g_vOwnedSkins.push_back((int)InPacket_Decode4(p));
    }
    g_nDamageSkin = nActive;
    RefreshDamageSkinPicker();
}

static void OnPacket_Result(CInPacket* p) {
    uint8_t op = InPacket_Decode1(p);
    uint8_t ok = InPacket_Decode1(p);
    int sid = (int)InPacket_Decode4(p);
    (void)InPacket_Decode4(p); // mesos field, unused on client
    if (!ok)
        return;

    if (op == 1 /*apply*/) {
        g_nDamageSkin = sid;
        try {
            ZXString<char> zmsg("Damage skin has been applied.");
            CUtilDlg_Notice(zmsg, nullptr, nullptr, 0, 0);
        } catch (...) {
        }
    } else if (op == 2 /*purchase*/) {
        bool found = false;
        for (int id : g_vOwnedSkins)
            if (id == sid) {
                found = true;
                break;
            }
        if (!found)
            g_vOwnedSkins.push_back(sid);
        RefreshDamageSkinPicker();
    }
}

static void OnPacket_Broadcast(CInPacket* p) {
    int charId = (int)InPacket_Decode4(p);
    int skinId = (int)InPacket_Decode4(p);
    SetSkinForChar(charId, skinId);
    if (charId == g_nLocalCharId) {
        g_nDamageSkin = skinId;
    }
}


// ---------------------------------------------------------------------------
// CClientSocket::ProcessPacket hook — peeks the opcode; if it's ours,
// dispatches and swallows. Otherwise rewinds the packet offset and
// delegates to the stock handler so the engine's normal routing runs.
// ---------------------------------------------------------------------------
static auto CClientSocket_ProcessPacket =
        reinterpret_cast<void(__thiscall*)(void*, CInPacket*)>(kAddr_ClientSocket_ProcessPacket);

extern void BagWindow_HandleSnapshotPacket(CInPacket* pPacket);
#include "discord.h"
#include "discord_ui.h"
#include "cashshopwnd.h"

static void __fastcall CClientSocket_ProcessPacket_hook(
        void* pThis, void* /*edx*/, CInPacket* pPacket) {
    size_t saved = PacketOffset(pPacket);
    uint16_t op = InPacket_Decode2(pPacket);
    switch (op) {
    //case 0x3724: // BAG_WINDOW
    case 0x3725: // BAG_WINDOW (server -> client)
        PacketOffset(pPacket) = saved;
        BagWindow_HandleSnapshotPacket(pPacket);
        return;
    case 0x3726: // DISCORD_UPDATE (server -> client)
        PacketOffset(pPacket) = saved;
        DiscordAPI::HandleUpdatePacket(pPacket);
        return;
    case 0x3727: // OPEN_DISCORD_UI
        PacketOffset(pPacket) = saved;
        DiscordUI_Toggle();
        return;
    case kCashShopSyncOpcode: // CASHSHOP_WINDOW_SYNC (server -> client)
        CashShopWnd_HandleSync(pPacket);
        return;
    case kOp_S2C_Catalog:
        OnPacket_Catalog(pPacket);
        return;
    case kOp_S2C_Inventory:
        OnPacket_Inventory(pPacket);
        return;
    case kOp_S2C_Result:
        OnPacket_Result(pPacket);
        return;
    case kOp_S2C_Broadcast:
        OnPacket_Broadcast(pPacket);
        return;
    default:
        PacketOffset(pPacket) = saved;
        CClientSocket_ProcessPacket(pThis, pPacket);
        return;
    }
}


// ===========================================================================
// Attacker attribution. CMob::OnHit receives the attacker's char id as the
// first stack arg. We swap g_nDamageSkin to that attacker's skin for the
// duration of the call so Effect_HP renders each player's damage in their
// own skin, not ours.
// ===========================================================================

static constexpr uintptr_t kAddr_CMob_OnHit = 0x00668B83;
static constexpr uintptr_t kAddr_CUserLocal_Instance = 0x00BEBF98;
static constexpr uintptr_t kOff_UserLocal_CharId = 1130 * 4; // 0x11A8

// CMob::OnHit(this, attackerCid, a3..a14) — __thiscall, 13 stack args
// (verified by `retn 34h` epilogue: 0x34 / 4 = 13). Hex-Rays reports 14,
// which counts the `this` pointer in ecx as a stack arg.
typedef void(__thiscall* t_CMob_OnHit)(
        void*, unsigned int, int, int, int, int, int, int, int,
        int, int, int, int, int);
static t_CMob_OnHit CMob_OnHit_orig =
        reinterpret_cast<t_CMob_OnHit>(kAddr_CMob_OnHit);

static int ReadLocalCharId() {
    auto pUser = *reinterpret_cast<char**>(kAddr_CUserLocal_Instance);
    if (!pUser)
        return 0;
    __try {
        return *reinterpret_cast<int*>(pUser + kOff_UserLocal_CharId);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return 0;
    }
}

void __fastcall CMob_OnHit_hook(
        void* pThis, void* /*edx*/,
        unsigned int attackerCid,
        int a3, int a4, int a5, int a6, int a7, int a8, int a9, int a10,
        int a11, int a12, int a13, int a14) {
    int saved = g_nDamageSkin;
    int localId = ReadLocalCharId();
    if (localId)
        g_nLocalCharId = localId;

    int attackerSkin = GetSkinForChar((int)attackerCid);
    if ((int)attackerCid == g_nLocalCharId) {
        // Local attacker: prefer the just-applied skin even if the
        // broadcast round-trip hasn't landed yet.
        if (attackerSkin != 0)
            g_nDamageSkin = attackerSkin;
    } else {
        g_nDamageSkin = attackerSkin;
    }

    CMob_OnHit_orig(pThis, attackerCid, a3, a4, a5, a6, a7, a8, a9, a10,
            a11, a12, a13, a14);
    g_nDamageSkin = saved;
}

void AttachDamageSkinNet() {
    ATTACH_HOOK(CClientSocket_ProcessPacket, CClientSocket_ProcessPacket_hook);
    ATTACH_HOOK(CMob_OnHit_orig, CMob_OnHit_hook);
}


// ===========================================================================
// Unit-damage rendering (NoCustom)
//
// For skins carrying a NoCustom node, damage values ≥ 1000 render as "1.8K"
// / "1M" / "1B" rather than the raw integer. Three pieces:
//
//   1. A property wrapper that exposes both the base digit sheet and the
//      NoCustom glyphs through a single IWzProperty interface. Effect_HP's
//      digit loop queries get_item per character; the wrapper resolves the
//      right node (NoRed0/n for digits, NoCustom/NoRed0/n for unit chars).
//
//   2. A hook on the ZXString::Format call inside Effect_HP (0x00437DFB).
//      When the wrapper is installed on the current call, we run the real
//      Format then rewrite the produced string in place with the unit-form
//      encoding (see FormatUnitDamage for the byte mapping).
//
//   3. PickNormal / PickCrit return the wrapper in BOTH small and large
//      slots when a unit-damage skin is active, so the glyph run renders
//      at a consistent size regardless of which slot the engine queries.
// ===========================================================================

// CUnitPropertyWrapper holds no long-lived IWzPropertyPtr. Each get_item
// call re-resolves the target node through get_rm()->raw_GetObject against
// the current WzNameSpace cache, so there's nothing for the cache to
// invalidate behind us.
class CUnitPropertyWrapper : public IWzProperty {
public:
    CUnitPropertyWrapper(int nSkinId, bool bCrit)
        : m_nSkinId(nSkinId), m_bCrit(bCrit) { m_nRef.store(1); }

    // IUnknown ---------------------------------------------------------
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppv) override {
        if (!ppv)
            return E_POINTER;
        if (riid == IID_IUnknown ||
                riid == __uuidof(IWzSerialize) ||
                riid == __uuidof(IWzProperty)) {
            *ppv = static_cast<IWzProperty*>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }
    ULONG __stdcall AddRef() override { return (ULONG)(++m_nRef); }
    ULONG __stdcall Release() override {
        // Wrappers are owned by the DamageSkinProp map and live for the
        // DLL's lifetime. Clamp at 1 so the engine's Release can't free us.
        long n = --m_nRef;
        if (n <= 0) {
            m_nRef.store(1);
            n = 1;
        }
        return (ULONG)n;
    }

    // IWzSerialize -----------------------------------------------------
    HRESULT __stdcall get_persistentUOL(BSTR*) override { return E_NOTIMPL; }
    HRESULT __stdcall raw_Serialize(IWzArchive*) override { return E_NOTIMPL; }

    // IWzProperty ------------------------------------------------------
    // Effect_HP's digit loop reads each byte b of the damage string and
    // looks up `_itow(b - '0')` against this property. FormatUnitDamage
    // emits bytes 48..61 so the lookup keys land on "0".."13":
    //
    //   "0".."9"   -> damageSkin/<id>/NoRed0|NoCri0/<n>
    //   "10".."13" -> damageSkin/<id>/NoCustom/NoRed0|NoCri0/<n-10>
    HRESULT __stdcall get_item(BSTR sPath, VARIANT* pvValue) override {
        // SEH and C++ object unwinding can't coexist in the same frame
        // (MSVC C2712). The resolve owns Ztl_variant_t locals, so it lives
        // in a separate function and this thunk just wraps the SEH.
        __try {
            return DoResolve(m_nSkinId, m_bCrit, sPath, pvValue);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            if (pvValue)
                VariantInit(pvValue);
            return E_FAIL;
        }
    }
    HRESULT __stdcall put_item(BSTR, VARIANT) override { return E_NOTIMPL; }
    HRESULT __stdcall get__NewEnum(IUnknown**) override { return E_NOTIMPL; }
    HRESULT __stdcall get_count(unsigned int* p) override {
        if (p)
            *p = 0;
        return S_OK;
    }
    HRESULT __stdcall raw_Add(BSTR, VARIANT, VARIANT) override { return E_NOTIMPL; }
    HRESULT __stdcall raw_Remove(BSTR) override { return E_NOTIMPL; }
    HRESULT __stdcall raw_Import(BSTR) override { return E_NOTIMPL; }

private:
    static HRESULT DoResolve(int nSkinId, bool bCrit, BSTR sPath, VARIANT* pvValue) {
        if (!pvValue)
            return E_POINTER;
        VariantInit(pvValue);
        if (!sPath)
            return E_INVALIDARG;

        const wchar_t* sheet = bCrit ? L"NoCri0" : L"NoRed0";
        wchar_t uol[192];

        if (sPath[0] == L'1' &&
                sPath[1] >= L'0' && sPath[1] <= L'3' &&
                sPath[2] == 0) {
            _snwprintf_s(uol, _countof(uol), _TRUNCATE,
                    L"Effect/BasicEff.img/damageSkin/%d/NoCustom/%ls/%c",
                    nSkinId, sheet, (wchar_t)sPath[1]);
        } else {
            _snwprintf_s(uol, _countof(uol), _TRUNCATE,
                    L"Effect/BasicEff.img/damageSkin/%d/%ls/%ls",
                    nSkinId, sheet, (const wchar_t*)sPath);
        }

        Ztl_variant_t vEmpty;
        Ztl_variant_t vResult;
        HRESULT hr = get_rm()->raw_GetObject(uol, vEmpty, vEmpty, &vResult);
        if (FAILED(hr) || vResult.vt != VT_UNKNOWN || !vResult.punkVal) {
            return E_FAIL;
        }
        // Transfer ownership of vResult's IUnknown ref into pvValue and
        // disarm vResult so its destructor doesn't release the ref we
        // just handed to the caller.
        pvValue->vt = vResult.vt;
        pvValue->punkVal = vResult.punkVal;
        vResult.vt = VT_EMPTY;
        vResult.punkVal = nullptr;
        return S_OK;
    }

    int m_nSkinId;
    bool m_bCrit;
    std::atomic<long> m_nRef;
};

static void BuildUnitWrappers() {
    for (auto& kv : g_mDamageSkinProp) {
        DamageSkinProp& prop = kv.second;
        if (!prop.bHasCustom)
            continue;
        if (prop.pWrapNormal || prop.pWrapCrit)
            continue;
        prop.pWrapNormal = new CUnitPropertyWrapper(kv.first, /*bCrit=*/false);
        prop.pWrapCrit = new CUnitPropertyWrapper(kv.first, /*bCrit=*/true);
    }
}

static bool ShouldRenderUnit(int damage) {
    if (damage < 1000)
        return false;
    if (g_nDamageSkin == 0)
        return false;
    auto it = g_mDamageSkinProp.find(g_nDamageSkin);
    return it != g_mDamageSkinProp.end() && it->second.bHasCustom;
}

// Encode damage as a byte string that the engine's digit loop can iterate
// without producing negative integer keys:
//
//   digits 0..9   -> bytes 48..57 ('0'..'9')     keys "0".."9"
//   decimal '.'   -> byte 58 (':')                key  "10"  -> NoCustom/0
//   unit K        -> byte 59 (';')                key  "11"  -> NoCustom/1
//   unit M        -> byte 60 ('<')                key  "12"  -> NoCustom/2
//   unit B        -> byte 61 ('=')                key  "13"  -> NoCustom/3
//
// Keeps one fractional digit when the next denomination isn't whole.
static void FormatUnitDamage(long long damage, char* out, size_t cap) {
    long long abs = damage < 0 ? -damage : damage;
    char unitByte = 0;
    long long divisor = 1;
    if (abs >= 1000000000LL) {
        unitByte = '=';
        divisor = 1000000000LL;
    } else if (abs >= 1000000LL) {
        unitByte = '<';
        divisor = 1000000LL;
    } else {
        unitByte = ';';
        divisor = 1000LL;
    }

    long long whole = abs / divisor;
    long long remainder = abs - whole * divisor;
    long long tenths = (remainder * 10) / divisor;

    if (tenths == 0) {
        _snprintf_s(out, cap, _TRUNCATE, "%lld%c", whole, unitByte);
    } else {
        _snprintf_s(out, cap, _TRUNCATE, "%lld:%lld%c",
                whole, tenths, unitByte);
    }
}

// In-place rewrite of a ZXString<char>. ZXString layout:
//   _ZXStringData { long nRef; int nCap; int nByteLen; char buf[nCap+1]; }
//   _m_pStr points at buf, so the header sits at [_m_pStr - 12].
// Returns false if the new string won't fit inside the existing nCap.
static bool OverwriteZXString(ZXString<char>* pZStr, const char* newStr) {
    if (!pZStr)
        return false;
    char* pBuf = *reinterpret_cast<char**>(pZStr);
    if (!pBuf)
        return false;
    int* pCap = reinterpret_cast<int*>(pBuf - 8);
    int* pByteLen = reinterpret_cast<int*>(pBuf - 4);
    int newLen = (int)strlen(newStr);
    if (newLen + 1 > *pCap)
        return false;
    memcpy(pBuf, newStr, newLen + 1);
    *pByteLen = newLen;
    return true;
}

typedef void(__cdecl* t_ZXStringFormat)(ZXString<char>*, const char*, int);
static auto RealZXStringFormat =
        reinterpret_cast<t_ZXStringFormat>(0x00445B4B);

// Patched in place of the `call Format` at 0x00437DFB. Runs the real
// Format, then — only when our wrapper was installed by Normal_helper /
// Crit_helper on this same Effect_HP call — overwrites the produced
// ZXString with the unit-form encoding.
static void __cdecl ZXStringFormat_hook(
        ZXString<char>* pRet, const char* fmt, int damage) {
    // Atomically read and clear the splice gate. Whatever the helper set
    // for this Effect_HP, consume it here so the next Format call starts
    // from a clean state.
    LONG bWrapperActive = InterlockedExchange(&g_bSpliceWrapperActive, 0);

    __try {
        RealZXStringFormat(pRet, fmt, damage);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return;
    }

    // Non-spliced Effect_HP branches (e.g. mob → player damage) reach this
    // hook too. Rewriting the string in that case would queue invalid
    // IUnknowns into the animation layer, since the engine would resolve
    // ':' / ';' bytes against the mob's default digit sheet which has no
    // NoCustom mapping.
    if (!bWrapperActive)
        return;

    __try {
        if (!ShouldRenderUnit(damage))
            return;
        char unitStr[32];
        FormatUnitDamage((long long)damage, unitStr, sizeof(unitStr));
        OverwriteZXString(pRet, unitStr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
    }
}

static void PatchEffectHPFormatCall() {
    constexpr uintptr_t kSite = 0x00437DFB;
    PatchCall(kSite, &ZXStringFormat_hook, 5);
}

void AttachUnitDamageRendering() {
    BuildUnitWrappers();
    PatchEffectHPFormatCall();
}