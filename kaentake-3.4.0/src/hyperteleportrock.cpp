#include "pch.h"

#include "hook.h"
#include "worldmap.h"
#include "wvs/packet.h"
#include "ztl/tsingleton.h"
#include "ztl/ztl.h"

namespace {

constexpr unsigned int WM_OnMouseButton = 0x009EE6E5;
constexpr unsigned int CUtilDlg_YesNo = 0x00992BFD;
constexpr unsigned int CUtilDlg_Notice = 0x009929DD;
constexpr unsigned int CWvsContext_GetItemCount = 0x00A2858C;
constexpr unsigned int CClientSocket_SendPacket = 0x0049637B;
constexpr unsigned int CDialog_SetRet = 0x004B5F05;

constexpr unsigned short CP_HyperTeleportRock = 0x105;
constexpr unsigned int kHyperTeleportRockItemId = 5590001;

using t_OnMouseButton = void(__thiscall*)(void*, unsigned int, unsigned int, int, int);
using t_YesNo = int(__cdecl*)(ZXString<char>, int, int, int, int);
using t_Notice = void(__cdecl*)(ZXString<char>, int, int, int, int);
using t_GetItemCount = int(__thiscall*)(void*, int);
using t_SendPacket = void(__thiscall*)(void*, const COutPacket&);
using t_SetRet = void(__thiscall*)(void*, int);

static auto OnMouseButton_Orig = reinterpret_cast<t_OnMouseButton>(WM_OnMouseButton);

class CWvsContext : public TSingleton<CWvsContext, 0x00BE7918> {
public:
    int GetItemCount(int itemId) {
        return reinterpret_cast<t_GetItemCount>(CWvsContext_GetItemCount)(this, itemId);
    }
};

class CClientSocket : public TSingleton<CClientSocket, 0x00BE7914> {
public:
    void SendPacket(const COutPacket& packet) {
        reinterpret_cast<t_SendPacket>(CClientSocket_SendPacket)(this, packet);
    }
};

bool HasHyperTeleportRock() {
    return CWvsContext::IsInstantiated() &&
           CWvsContext::GetInstance()->GetItemCount(kHyperTeleportRockItemId) > 0;
}

void ShowNotice(const char* message) {
    reinterpret_cast<t_Notice>(CUtilDlg_Notice)(ZXString<char>(message), 0, 0, true, false);
}

bool ConfirmMove(int mapId) {
#ifdef HAS_GETMAPBYID
    std::string fullName = GetMapById(mapId);
    std::string mapName = fullName.empty() ? std::to_string(mapId) : fullName;
#else
    std::string mapName = std::to_string(mapId);
#endif

    ZXString<char> text;
    text.Format("Are you sure you want to move to %s?", mapName.c_str());

    return reinterpret_cast<t_YesNo>(CUtilDlg_YesNo)(text, 0, 0, true, false) == 1; // 1 = IDYES
}

bool SendHyperTeleportRockRequest(int targetMapId) {
    if (!CClientSocket::IsInstantiated()) {
        return false;
    }

    COutPacket packet(CP_HyperTeleportRock);
    packet.Encode4(static_cast<unsigned int>(targetMapId));
    CClientSocket::GetInstance()->SendPacket(packet);
    return true;
}

bool TryHyperTeleport(void* worldMapDlg, int rx, int ry) {
    WorldMap::Spot spot = WorldMap::GetSpot(reinterpret_cast<char*>(worldMapDlg) - 4, rx, ry);
    if (spot.index < 0 || spot.mapId <= 0) {
        return false;
    }

    if (!HasHyperTeleportRock()) {
        ShowNotice("You need a Hyper Teleport Rock to move there.");
        return true;
    }

    if (!ConfirmMove(spot.mapId)) {
        return true;
    }

    if (!SendHyperTeleportRockRequest(spot.mapId)) {
        return true;
    }

    reinterpret_cast<t_SetRet>(CDialog_SetRet)(reinterpret_cast<char*>(worldMapDlg) - 4, 2);
    return true;
}

void __fastcall OnMouseButton_Hook(void* ecx, void*, unsigned int msg, unsigned int wParam, int rx, int ry) {
    if (msg == 0x0202 && TryHyperTeleport(ecx, rx, ry)) { // 0x0202 = WM_LBUTTONUP
        return;
    }

    OnMouseButton_Orig(ecx, msg, wParam, rx, ry);
}

} // namespace

void AttachHyperTeleportRockMod() {
    ATTACH_HOOK(OnMouseButton_Orig, OnMouseButton_Hook);
}
