#include "pch.h"
#include "hook.h"

// ============================================================
// Fix Cash Items (Memory Patches for 10000 / 501 limits)
// ============================================================

void AttachFixCashItemsMod() {
    Patch4(0x0093C144 + 1, 0x2710); // mov ecx, 10000
    Patch4(0x0093C14F + 1, 0x1F5);  // cmp eax, 501
    Patch4(0x0093C67E + 1, 0x2710); // mov ecx, 10000
    Patch4(0x0093C689 + 1, 0x1F5);  // cmp eax, 501
    Patch4(0x0095B112 + 1, 0x2710); // mov ecx, 10000
    Patch4(0x0095B11F + 1, 0x1F5);  // cmp eax, 501
}
