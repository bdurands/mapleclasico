#include "pch.h"
#include "hook.h"

// ============================================================
// Face/Hair display restriction removal
// ============================================================

// --- Cave 1: Uncap Display ---
// Installed at 0x005C94F3, covers 18 bytes
static auto faceHairCaveRtn = 0x005C9505;
static auto faceRtn = 0x005C95BF;
static auto hairRtn = 0x005C958D;

static void __declspec(naked) faceHairCave()
{
    __asm {
        cmp eax, 2
        jz  label_face
        cmp eax, 5
        jz  label_face
        cmp eax, 3
        jz  label_hair
        cmp eax, 4
        jz  label_hair
        cmp eax, 6
        jz  label_hair

        jmp [faceHairCaveRtn]

    label_face:
        jmp [faceRtn]

    label_hair:
        jmp [hairRtn]
    }
}

// --- Cave 2: Uncap NPC Dialog ---
// Installed at 0x009ACA9B, covers 18 bytes
static auto faceHairCave2Rtn = 0x009ACAAD;

static void __declspec(naked) faceHairCave2()
{
    __asm {
        cmp eax, 2
        je  label_face
        cmp eax, 5
        je  label_face
        cmp eax, 3
        je  label_hair
        cmp eax, 4
        je  label_hair
        cmp eax, 6
        je  label_hair
        jmp label_skin

    label_face:
        mov eax, 0
        mov ecx, 0
        jmp label_end

    label_hair:
        mov eax, 1
        mov ecx, 1
        jmp label_end

    label_skin:
        mov eax, 2
        mov ecx, 2
        jmp label_end

    label_end:
        jmp [faceHairCave2Rtn]
    }
}

void AttachFaceHairMod()
{
    // Uncap Display
    PatchJmp(0x005C94F3, &faceHairCave);
    PatchNop(0x005C94F3 + 5, 0x005C94F3 + 18);

    // Uncap NPC Dialog
    PatchJmp(0x009ACA9B, &faceHairCave2);
    PatchNop(0x009ACA9B + 5, 0x009ACA9B + 18);
}
