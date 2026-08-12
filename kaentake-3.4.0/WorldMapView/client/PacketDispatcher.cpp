#include "pch.h"
#include "hook.h"
#include "wvs/packet.h"
#include "nxfloatnotice.h"

#include "uiDamageRank.h"
#include <cstdio>
#include <cstdarg>
#include <string>

static constexpr unsigned short kDptTrackerOpcode         = 0x3714;
static constexpr unsigned short kRequestLockedSlotsOpcode = 0x167;
static constexpr unsigned short kServerMessageOpcode      = 0x44;

extern void SlotLock_HandleRequestLockedSlots(int invType);

// World-map "Players" tooltip reply (LP 0x178), decoded in worldmapinfo.cpp. We route it through
// THIS central inbound dispatcher rather than a separate Detours hook on the heavily-chained
// 0x004965F1 (7+ detours deep), where worldmap's own hook was not reliably firing - the request
// (CP 0x115) reached the server and it replied, but the client never decoded the response.
namespace players { void HandleResponse(CInPacket* p); }
static constexpr unsigned short kWorldMapPlayersReply = 0x178; // LP_WORLD_MAP_PLAYERS

// Bot Stats/Skills window reply (LP 0x17A), decoded in botstats.cpp. Routed through
// this central dispatcher for the same reliability reason as the world-map reply
// above (BOT_STATS_SCOPE.md §1 - do NOT add a fresh detour on 0x004965F1).
namespace botstats { void HandleSnapshot(CInPacket* p); }
static constexpr unsigned short kBotStatsReply = 0x17A; // LP_BOT_STATS

// void __thiscall CClientSocket::ProcessPacket(CClientSocket* this, CInPacket* iPacket)
static auto oriCClientSocketProcessPacket =
        reinterpret_cast<void(__thiscall*)(void*, CInPacket*)>(0x004965F1);

static void AppendDptLog(const char* /*fmt*/, ...) {
    // Logging disabled.
}

static void __fastcall CClientSocketProcessPacket_hook(
        void* pThis,
        void* /*edx*/,
        CInPacket* iPacket) {
    if (!iPacket) {
        oriCClientSocketProcessPacket(pThis, iPacket);
        return;
    }

    // The opcode sits at the packet's CURRENT offset (m_uOffset), which is NOT guaranteed
    // to be 0 at this detour's entry — confirmed by worldmapinfo.cpp's HandleResponse (called
    // from this hook with the offset untouched, it had to switch from SetOffset(2) to
    // SetOffset(entryOff + 2) after decoding garbage) and by every sibling hook on 0x004965F1
    // (damageskin/androidequip/beautyshop/CancelBuccaner) reading the opcode relative to the
    // saved offset. Peek2Public/Decode all work off m_uOffset, so skipping the opcode means
    // entryOff + 2, never absolute 2. Capture it once for the body-parsing branches below.
    const size_t entryOff = iPacket->GetOffset();

    const unsigned short nType = iPacket->Peek2Public();
    AppendDptLog("[ProcessPacket] peek nType=0x%04X packet=%p", nType, iPacket);

    if (nType == kWorldMapPlayersReply) {
        players::HandleResponse(iPacket);   // worldmapinfo.cpp resets offset + decodes
        return;                              // ours — consume
    }

    if (nType == kBotStatsReply) {
        botstats::HandleSnapshot(iPacket);   // botstats.cpp resets offset + decodes
        return;                              // ours — consume
    }

    if (nType == kDptTrackerOpcode) {
        AppendDptLog("[ProcessPacket] DPT_TRACKER hit");
        CUIDamageRank::GetInstance().HandleTrackerPacket(iPacket);
        return;
    }

    if (nType == kRequestLockedSlotsOpcode) {
        iPacket->SetOffset(entryOff + 2);  // skip opcode (relative — see entryOff note)
        int invType = iPacket->Decode<uint8_t>();
        SlotLock_HandleRequestLockedSlots(invType);
        return;
    }

    if (nType == kServerMessageOpcode) {
        iPacket->SetOffset(entryOff + 2); // skip opcode (relative — see entryOff note)
        if (iPacket->CanRead(1)) {
            uint8_t msgType = iPacket->Decode<uint8_t>();
            if (msgType == 6 && iPacket->CanRead(2)) {
                uint16_t len = iPacket->Decode<uint16_t>();
                if (iPacket->CanRead(len)) {
                    std::string msg(reinterpret_cast<const char*>(iPacket->CurrentPublic()), len);
                    if (msg.find("NX earned") != std::string::npos) {
                        NxFloat_Push(msg.c_str());
                        return; // swallow — shown as green float notice instead
                    }
                }
            }
        }
        iPacket->SetOffset(entryOff); // not an NX toast — restore and forward
    }

    oriCClientSocketProcessPacket(pThis, iPacket);
}

void AttachClientSocketProcessMod() {
    AppendDptLog("[AttachClientSocketProcessMod] begin");
    const bool ok = ATTACH_HOOK(oriCClientSocketProcessPacket, CClientSocketProcessPacket_hook);
    AppendDptLog("[AttachClientSocketProcessMod] hook=%d", ok ? 1 : 0);
    NxFloat_Init();
}
