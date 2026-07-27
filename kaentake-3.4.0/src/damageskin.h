#pragma once
#include "pch.h"
#include "WzLib/IWzProperty.h"

#include <map>
#include <vector>


// Shared state between the in-game renderer (damageskin.cpp) and the
// picker UI (damageskinpicker.cpp).

struct DamageSkinProp {
    IWzPropertyPtr pNoRed0;
    IWzPropertyPtr pNoRed1;
    IWzPropertyPtr pNoCri0;
    IWzPropertyPtr pNoCri1;

    // Optional unit-damage glyphs under the skin's NoCustom node:
    //   NoCustom/NoRed0/0 = "."  (decimal)
    //   NoCustom/NoRed0/1 = "K"
    //   NoCustom/NoRed0/2 = "M"
    //   NoCustom/NoRed0/3 = "B"
    // (and the matching NoCri0 children for crit). bHasCustom is true
    // iff both NoRed0 and NoCri0 unit folders resolved. The picker's
    // "unit-only" filter reads this flag, and DrawPreview paints a "1.8K"
    // sample when the hovered skin has them.
    IWzPropertyPtr pNoCustomRed0;
    IWzPropertyPtr pNoCustomCri0;
    bool bHasCustom = false;

    // In-game unit-damage property wrappers. When bHasCustom is true we
    // build CUnitPropertyWrapper instances that re-resolve digit and unit
    // glyphs via the resman on each get_item call. Raw pointers — wrappers
    // are intentionally never freed; they live for the DLL's lifetime.
    void* pWrapNormal = nullptr;
    void* pWrapCrit = nullptr;
};

extern std::map<int, DamageSkinProp> g_mDamageSkinProp;
extern std::vector<int> g_vSkinIds;
extern int g_nDamageSkin;

// Idempotent. Loads the WZ on first call.
void LoadDamageSkin();

// Set the active skin id used by subsequent ShowDamage calls. 0 = default.
void SetActiveDamageSkin(int nID);

// charId → skinId map populated from server broadcasts. Used by the
// attacker-attribution hook to pick the correct skin before Effect_HP fires.
int GetSkinForChar(int nCharId);
void SetSkinForChar(int nCharId, int nSkinId);

// Shop catalog and owned inventory, populated from server packets at login
// and kept fresh on subsequent updates. Consumed by the picker UI.
struct DamageSkinCatalogEntry {
    int nID;
    long long llPrice;
};
extern std::vector<DamageSkinCatalogEntry> g_vShopCatalog;
extern std::vector<int> g_vOwnedSkins;
extern int g_nLocalCharId;

// Outgoing requests. Fire-and-forget; server replies via DAMAGE_SKIN_RESULT.
void Send_DamageSkinApply(int nSkinId);
void Send_DamageSkinPurchase(int nSkinId);

// Installed once from AttachDamageSkinMod().
void AttachDamageSkinNet();

// Defined in damageskinpicker.cpp. Rebuilds m_vMyList / m_vShopList from
// g_vOwnedSkins / g_vShopCatalog if the picker is currently open. No-op
// otherwise. Called by the net packet handlers after server state moves.
void RefreshDamageSkinPicker();