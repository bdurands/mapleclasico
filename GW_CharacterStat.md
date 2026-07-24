======================================================================
    4-BYTE HP/MP EXPANSION - COMPLETE REFERENCE (v83)
                  Status: Working
======================================================================

OVERVIEW
----------------------------------------------------------------------
Removes the 32,767 (short/2-byte) limit for HP, MaxHP, MP, MaxMP.
Supports values up to 99,999 (int/4-byte) across the entire client.

Requires synchronized server-side changes:
  - Server must send HP/MP as writeInt (4 bytes) instead of writeShort
  - Server must not clamp HP/MP values to short range

Implementation File: src/maxHPmaxMP.cpp
Entry Point:         AttachMaxHPMPMod()


======================================================================
ARCHITECTURE: HYBRID FAKETEAR + FUSE HOOK
======================================================================

Why not just redirect Tear_short -> Tear_long globally?
----------------------------------------------------------------------
ZtlSecureTear_long stores 8 bytes: [ptr+0..3] random key, [ptr+4..7]
encrypted data. But the CALLER stores the checksum return value at
[ptr+4], overwriting Tear_long's encrypted data. ZtlSecureFuse_long
then reads garbage at [ptr+4] -> ZException crash.

The hybrid approach:
----------------------------------------------------------------------
1. FAKETEAR: At HP/MP Tear call sites, replace ZtlSecureTear_short
   with HPMP_FakeTear which stores raw 4-byte value at [ptr] and
   returns checksum=0 as a sentinel.

2. FUSE HOOK: Global Detours hook on ZtlSecureFuse_short AND
   ZtlSecureFuse_long. If checksum==0, return *(int*)pTear (raw
   4-byte). Otherwise call original Fuse for normal encrypted values.

3. FIXMOVSX: Patch all movsx instructions after Fuse calls that
   truncate EAX back to 16-bit.

This preserves full compatibility - only HP/MP use raw storage, all
other ZtlSecure<short> stats remain encrypted and unchanged.


======================================================================
KEY FUNCTIONS & ADDRESSES
======================================================================

ZtlSecureTear_short:  0x004E80EB  (__fastcall: ECX=value, EDX=ptr)
ZtlSecureFuse_short:  0x004746DD  (__cdecl: ptr, checksum) [HOOKED]
ZtlSecureTear_long:   0x004165B1  (DO NOT USE - struct incompatible)
ZtlSecureFuse_long:   0x00416563  (__cdecl: ptr, checksum) [HOOKED]
CInPacket::Decode4:   0x00406629
CInPacket::Decode2:   0x0042470C


======================================================================
STAT OFFSETS IN GW_CharacterStat
======================================================================

  Stat     Data Offset   Checksum Offset   Bitmask
  ------   -----------   ---------------   -------
  HP       +0x61         +0x65             0x0400
  MaxHP    +0x69         +0x6D             0x0800
  MP       +0x71         +0x75             0x1000
  MaxMP    +0x79         +0x7D             0x2000


======================================================================
PATCH CATEGORY 1: PACKET DECODING (Decode2 -> Decode4)
======================================================================
13 sites where the client reads HP/MP from incoming packets.
Server must send 4 bytes at each corresponding location.

  GW_CharacterStat::Decode (Login & Full Map Change)
  ---------------------------------------------------
  0x004E2B9A  Decode2 -> Decode4   HP
  0x004E2BAE  Decode2 -> Decode4   MaxHP
  0x004E2BC2  Decode2 -> Decode4   MP
  0x004E2BD6  Decode2 -> Decode4   MaxMP

  CStage::OnSetField (Partial Map Change)
  ----------------------------------------
  0x0077621A  Decode2 -> Decode4   HP

  OnStatChanged / sub_4E2FBA (Gameplay Updates)
  ----------------------------------------------
  0x004E30DA  Decode2 -> Decode4   HP
  0x004E30F4  Decode2 -> Decode4   MaxHP
  0x004E310E  Decode2 -> Decode4   MP
  0x004E3128  Decode2 -> Decode4   MaxMP

  CWvsContext::OnPartyResult Case 37 (Party Member Update)
  ---------------------------------------------------------
  0x00A3ECF5  Decode2 -> Decode4   MP   (+ FixMovsx 0x00A3ECFA)
  0x00A3ED02  Decode2 -> Decode4   MaxMP (+ FixMovsx 0x00A3ED09)

  CUserRemote::OnAttack Case 0xBB (Remote Player Attack)
  -------------------------------------------------------
  0x00980656  Decode2 -> Decode4   MP   (+ FixMovsx 0x0098065B)
  0x00980663  Decode2 -> Decode4   MaxMP (+ FixMovsx 0x00980668)

NOTE: The 4 FixMovsx calls above are applied immediately after their
corresponding Decode4 patches (EAX from Decode4 is also 32-bit, so
any following movsx must be fixed in the same pass).


======================================================================
PATCH CATEGORY 2: FAKETEAR (Raw 4-Byte Storage)
======================================================================
25 sites where HP/MP values are written to ZtlSecure slots.
PatchCall replaces ZtlSecureTear_short (or _long) with HPMP_FakeTear.

  GW_CharacterStat::Decode
  -------------------------
  0x004E2BA4  FakeTear   HP
  0x004E2BB8  FakeTear   MaxHP
  0x004E2BCC  FakeTear   MP
  0x004E2BE0  FakeTear   MaxMP

  CStage::OnSetField
  --------------------
  0x00776224  FakeTear   HP

  OnStatChanged / sub_4E2FBA
  ----------------------------
  0x004E30E4  FakeTear   HP
  0x004E30FE  FakeTear   MaxHP
  0x004E3118  FakeTear   MP
  0x004E3132  FakeTear   MaxMP

  CSkillInfo::CheckConsumeForActiveSkill (Skill HP/MP write-back)
  ----------------------------------------------------------------
  0x007646F2  FakeTear   HP
  0x0076470F  FakeTear   MP
  Without these, skill cost deduction writes truncated values back
  via original Tear, corrupting the raw 4-byte slot on next Fuse read.

  CUserLocal::DoActiveSkill (HP/MP restore on skill failure)
  -----------------------------------------------------------
  0x00967B94  FakeTear   HP
  0x00967BA2  FakeTear   MP

  Stat Recalculation / sub_78D46C (MaxHP/MaxMP capping)
  ------------------------------------------------------
  0x0078D914  FakeTear   MaxHP
  0x0078D961  FakeTear   MaxMP

  Equipment Stat Calculation / sub_77EC9F (ZtlSecureTear_long writes)
  --------------------------------------------------------------------
  CUIStatusBar reads MaxHP/MaxMP via ZtlSecureFuse_long from these
  slots. FakeTear all Tear_long writes so our Fuse_long hook returns
  the correct 32-bit value (sentinel checksum=0 -> raw *(int*)pTear).

  MaxHP Tear_long writes:
  0x0077ED9D  FakeTear   MaxHP  (Init from GW_CharacterStat)
  0x0077EF78  FakeTear   MaxHP  (Loop 1 - equipment bonuses)
  0x0077F0EF  FakeTear   MaxHP  (Loop 2 - equipment bonuses)
  0x0077F164  FakeTear   MaxHP  (Percentage multiplier)
  0x0077F1AF  FakeTear   MaxHP  (Cap path)

  MaxMP Tear_long writes:
  0x0077EDB9  FakeTear   MaxMP  (Init from GW_CharacterStat)
  0x0077EFA7  FakeTear   MaxMP  (Loop 1 - equipment bonuses)
  0x0077F11E  FakeTear   MaxMP  (Loop 2 - equipment bonuses)
  0x0077F18E  FakeTear   MaxMP  (Percentage multiplier)
  0x0077F1CE  FakeTear   MaxMP  (Cap path)


======================================================================
PATCH CATEGORY 3: GLOBAL FUSE HOOKS (Detours)
======================================================================
Two Detours hooks on both Fuse variants.

ZtlSecureFuse_short (0x4746DD):
  if (checksum == 0) return *(int*)pTear;   // raw 4-byte (HP/MP)
  else return original_Fuse(pTear, checksum); // encrypted short

ZtlSecureFuse_long (0x416563):
  if (checksum == 0) return *(int*)pTear;   // raw MaxHP/MaxMP from sub_77EC9F
  else return original_Fuse(pTear, checksum); // normal long stat

These handle ALL read sites globally without individual patches.
The checksum==0 sentinel is safe - legitimate ZtlSecure values
have non-zero checksums by design.


======================================================================
PATCH CATEGORY 4: MOVSX TRUNCATION FIXES
======================================================================
54 total sites (50 in section 4 + 4 inline with Decode4 patches above)
where movsx sign-extends EAX from 16-bit, truncating values > 32767.

  FixMovsx handles two forms:
  - Register: movsx eax, ax (0F BF C0) -> NOP NOP NOP
              movsx reg, ax (0F BF xx) -> mov reg, eax + NOP
  - Memory:   movsx reg, word ptr [mem] -> mov reg, dword ptr [mem] + NOP

  [4 inline Decode4 sites listed in Category 1 above]

  CUIStatusBar::Draw
  --------------------
  0x008D822D  movsx  MP
  0x008D8237  movsx  HP

  CUIStat::Draw (Character Stats Window)
  ----------------------------------------
  0x008C5DC4  movsx  HP
  0x008C5EB9  movsx  MP

  CUIPartyHP::Draw (Party HP Bar - Local Player)
  ------------------------------------------------
  0x009203F7  movsx  HP
  Local player's own HP is read via Fuse_short here for the party bar.
  Without this, the party HP bar shows negative when HP > 32767.

  CUIStatDetail::Draw (Detailed Stats)
  --------------------------------------
  0x008C4139  movsx ebx, ax   HP
  0x008C4141  movsx eax, ax   HP
  0x008C418E  movsx eax, ax   HP
  0x008C419F  movsx eax, ax   MP

  CStatWnd::UpdateAbilityButtons / sub_8CBDDB
  --------------------------------------------
  Controls whether AP +/- buttons are enabled in Stat Window.
  0x008CBF94  movsx  MaxHP
  0x008CC015  movsx  MaxMP
  0x008CC1CF  movsx  MaxHP
  0x008CC24F  movsx  MaxMP

  CStatWnd::DrawStatValues / sub_8CC8DE
  ----------------------------------------
  Renders stat text values in the Stat Window.
  0x008CC9DC  movsx  MaxMP
  0x008CCA8A  movsx  MaxHP

  CStatWnd::DrawStatDetails / sub_8CD8A0
  ----------------------------------------
  Renders stat detail/preview values (AP preview, hover tooltips).
  0x008CD9CE  movsx  MaxMP
  0x008CDAE5  movsx  MaxHP
  0x008CDF25  movsx  MaxMP
  0x008CDFC0  movsx  MaxHP

  CWvsContext::TryRecovery
  --------------------------
  0x00A02F88  movsx  HP
  0x00A03180  movsx  MaxMP

  CWvsContext::CheckDarkForce (HP-% skill condition)
  ---------------------------------------------------
  0x00A2938D  movsx  HP (site 1)
  0x00A29412  movsx  HP (site 2)
  Dark Force activates based on HP percentage. Without these fixes,
  HP > 32767 sign-extends to negative, triggering the condition
  incorrectly (or never).

  CWvsContext::CheckDragonFury (HP-% skill condition)
  ----------------------------------------------------
  0x00A29535  movsx  MP (site 1)
  0x00A29586  movsx  MP (site 2)
  Same issue as CheckDarkForce for Dragon Fury.

  CWvsContext::SendAbilityMassUpRequest
  ---------------------------------------
  0x00A23C4A  movsx  AP truncation

  CUserLocal::Update
  --------------------
  0x0094B096  movsx  HP
  0x0094B230  movsx  HP
  0x0094EA4C  movsx  MP (sub_94E88C inner loop)
  0x0094BB78  movsx  MP (skill check loop)

  CUserLocal::TryConsumePetHP / TryConsumePetMP
  ------------------------------------------------
  0x0095BA30  movsx  HP  (Pet auto-HP item trigger)
  0x0095BC7B  movsx  MP  (Pet auto-MP item trigger)
  Without these, pets trigger HP/MP potions at wrong thresholds
  when HP/MP > 32767 (sign-extended negative always < threshold).

  CUserLocal::SetDamaged
  ------------------------
  0x009584B6  movsx  HP
  0x0095960C  movsx  MaxMP

  CUserLocal::DoActiveSkill  *** ROOT CAUSE OF SKILL HAYWIRE ***
  ----------------------------------------------------------------
  0x00967733  movsx eax, ax   HP (truncates before saving to local)
  0x0096774F  movsx eax, ax   MP (truncates before saving to local)

  These were the primary cause of HP/MP going haywire when using
  skills. The flow:
    1. Fuse returns full 32-bit HP (e.g., 50000)
    2. movsx eax, ax truncates to -15536 (sign-extended 0xC350)
    3. Truncated value saved to [ebp+var_54] / [ebp+var_58]
    4. On skill failure, restored via Tear -> corrupts actual HP/MP

  Equipment Stat Calculation / sub_77EC9F
  -----------------------------------------
  0x0077ED97  movsx  MaxHP
  0x0077EDB3  movsx  MaxMP

  Stat Recalculation / sub_78D46C
  ---------------------------------
  0x0078D8FA  movsx  MaxHP
  0x0078D947  movsx  MaxMP

  CSkillInfo::CheckConsumeForActiveSkill
  ----------------------------------------
  0x00764401  movsx  HP
  0x007644E4  movsx  HP
  0x00764507  movsx  MP

  CField_Dojang::Update
  -----------------------
  0x00554AFC  movsx  HP
  0x00554B31  movsx  HP

  CSummoned::TryDoingHeal
  -------------------------
  0x007A5B42  movsx  HP

  Stat Formatting / sub_4E3238 (GW_CharacterStat::GetString)
  -----------------------------------------------------------
  Reads HP/MP back from stack as word ptr after full-stat decode.
  0x004E3371  movsx ecx, word ptr [ebp-24h]  MaxMP
  0x004E3376  movsx ecx, word ptr [ebp-28h]  MaxHP
  0x004E337B  movsx ecx, word ptr [ebp-2Ch]  MP
  0x004E3380  movsx ecx, word ptr [ebp-30h]  HP


======================================================================
PATCH CATEGORY 5: 16-BIT COMPARISON FIXES
======================================================================
2 sites where cmp ax, si (16-bit) must become cmp eax, esi (32-bit).

  Stat Recalculation / sub_78D46C
  ---------------------------------
  0x0078D8E7  66 3B C6 -> 3B C6 90   MaxHP vs limit
  0x0078D934  66 3B C6 -> 3B C6 90   MaxMP vs limit

  NOTE: Instruction starts at 0x78D8E7 (66 prefix), not 0x78D8E9.
  The 66 operand-size prefix is overwritten with 3B (cmp opcode).

  Original: cmp ax, si (operand-size prefix limits to 16-bit)
  Patched:  cmp eax, esi; nop (full 32-bit comparison)


======================================================================
PATCH CATEGORY 6: CLAMPING LIMITS (99,999)
======================================================================
4 sites where hardcoded limits are raised from 30,000 to 99,999.

  0x0078D8D2  Patch4  99999   Level Up MaxHP/MaxMP cap (sub_78D46C)
  0x0077F1A0  Patch4  99999   Equipment Sum cap (sub_77EC9F)
  0x008CD657  Patch4  99999   AP -> HP cap (CStatWnd)
  0x008CD6EB  Patch4  99999   AP -> MP cap (CStatWnd)


======================================================================
SERVER-SIDE CHANGES REQUIRED
======================================================================

PacketCreator.java:
  - addCharStats: writeInt for HP/MaxHP/MP/MaxMP
  - updatePlayerStats: writeInt for bitmasks 0x400/0x800/0x1000/0x2000
  - getWarpToMap: writeInt for HP

AbstractCharacterObject.java:
  - changeStatPool: Accepts Integer params for HP/MP instead of
    packed Long (which clamped to short range)
  - assignHP/assignMP: Caps raised from 30000 -> 99999
  - setMaxHp/setMaxMp: clientmaxhp/clientmaxmp capped at 99999

Key Server Change:
  Old: calcStatPoolNode clamps to [-32767, 32767] and packs into
       16-bit Long slots -> inherently limits to short range
  New: changeStatPool accepts direct Integer parameters, bypassing
       the Long-packing entirely. Uses null checks instead of
       Short.MIN_VALUE sentinels.


======================================================================
PATCH SUMMARY
======================================================================

  Category               Count   Method
  -------------------    -----   -------------------------
  Decode2 -> Decode4        13   PatchCall
  FakeTear                  25   PatchCall
  Fuse Hooks                 2   ATTACH_HOOK (Detours)
  FixMovsx                  54   Runtime byte patching
  16-bit cmp fix             2   Patch1 (remove 66 prefix)
  Clamping limits            4   Patch4
  -------------------    -----
  TOTAL                    100   patches


======================================================================
DEBUGGING TIPS
======================================================================

1. If values display as negative millions:
   -> Missing movsx patch. Check log for "[FixMovsx] WARNING" messages.
   -> Check if there's a new Fuse call site not yet patched.

2. If HP/MP corrupts only when using skills:
   -> CUserLocal::DoActiveSkill movsx (0x967733/0x96774F)
   -> DoActiveSkill Tear restore (0x967B94/0x967BA2)
   -> CSkillInfo::CheckConsumeForActiveSkill FakeTear (0x7646F2/0x76470F)

3. If MaxHP/MaxMP doesn't update past 30000:
   -> Check clamping limits (Patch4 sites)
   -> Check 16-bit comparisons in sub_78D46C

4. If game crashes on login/map change:
   -> Server not sending 4 bytes (writeInt) for HP/MP stats
   -> Packet desync: bytes shift if server sends 2 but client reads 4

5. If pet auto-HP/MP triggers at wrong thresholds:
   -> TryConsumePetHP/MP movsx (0x95BA30 / 0x95BC7B)

6. If party HP bar shows negative for local player:
   -> CUIPartyHP::Draw movsx (0x9203F7)

7. If dark force / dragon fury triggers incorrectly:
   -> CWvsContext::CheckDarkForce (0xA2938D / 0xA29412)
   -> CWvsContext::CheckDragonFury (0xA29535 / 0xA29586)

8. If other stats (STR, DEX, etc.) break:
   -> Fuse hook checksum==0 sentinel may conflict if another stat
      has natural checksum of 0. Extremely unlikely but possible.

9. Finding new truncation sites:
   -> Use IDA: xrefs_to ZtlSecureFuse_short (0x4746DD)
   -> Check for movsx within 15 bytes after each call
   -> Filter by HP/MP stat offsets:
      +0x61/+0x65 (HP), +0x69/+0x6D (MaxHP)
      +0x71/+0x75 (MP), +0x79/+0x7D (MaxMP)


======================================================================
FAILED APPROACHES (DO NOT RETRY)
======================================================================

1. Global PatchJmp Tear_short -> Tear_long:
   CRASH. Struct layout incompatibility. Tear_long stores encrypted
   data at [ptr+4], caller overwrites with checksum -> Fuse_long
   reads garbage -> ZException.

2. Decode2 -> Decode4 only (no Tear/Fuse changes):
   PARTIAL. Values up to 32767 display correctly, but Tear_short
   internally truncates to 16-bit (loops only 2 times). Current HP
   caps at 32768.

3. Global FakeTear + FakeFuse (early attempt):
   CRASH. FakeFuse applied globally breaks all non-HP/MP stats that
   expect encrypted format.

4. Shadow Global Buffer (alternative method):
   VIABLE but requires server protocol cooperation. Instead of
   patching ZtlSecure slots, hook GW_CharacterStat::Decode to store
   raw 32-bit HP/MP in parallel globals (g_nMaxHP, g_nMaxMP, etc.),
   then redirect all read sites to those globals. Advantage: zero
   ZtlSecure slot risk. Disadvantage: same number of read-site
   patches needed, plus server must encode high-half in a protocol
   field. Best suited for greenfield server design, not drop-in mod.


======================================================================
POTENTIAL FUTURE PATCHES (NOT YET NEEDED)
======================================================================

  CUserLocal::CalculateMapDamage / sub_642710
  ----------------------------------------------
  0x0064286B  movsx esi, ax   HP
  Triggered on maps with environmental damage (lava, poison gas).
  Symptom: Wrong damage taken from map hazards with HP > 32767.

  CUserLocal::GetMortalBlowThreshold / sub_7656AA
  --------------------------------------------------
  0x007656FE  movsx eax, ax   MP
  Checks MP threshold for Mortal Blow skill (Archer job).
  Symptom: Mortal Blow activates at wrong MP thresholds.

  Recovery Packet Encode / sub_A1E997
  --------------------------------------
  Opcode: 0x59 (CP_Recovery or similar)
  0x00A1E9D3  call Encode2   HP recovery amount -> Encode4
  0x00A1E9DE  call Encode2   MP recovery amount -> Encode4
  ALSO requires server-side handler to read 4 bytes instead of 2.
  Only needed if single-tick recovery amounts can exceed 32767.
  Fixed potions (+3000 HP) do NOT need this.

  Equipment HP/MP Bonuses
  -------------------------
  PacketCreator.java uses writeShort for equip.getHp()/getMp().
  These are equipment bonus stats (e.g., +20 HP from a ring).
  Only needed if individual equipment bonuses exceed 32767.

  Copy-paste block:
  // CUserLocal::CalculateMapDamage
  FixMovsx(0x0064286B); // HP

  // CUserLocal::GetMortalBlowThreshold
  FixMovsx(0x007656FE); // MP

  // Recovery Encode (REQUIRES SERVER CHANGE)
  // PatchCall(0x00A1E9D3, COutPacket__Encode4); // HP
  // PatchCall(0x00A1E9DE, COutPacket__Encode4); // MP


======================================================================
RELATED FUNCTIONS (IDA)
======================================================================

  Address      Name / Purpose
  ----------   -----------------------------------------
  0x004E2A84   GW_CharacterStat::Decode
  0x004E2FBA   OnStatChanged handler (sub_4E2FBA)
  0x004E3238   GW_CharacterStat::GetString (sub_4E3238)
  0x00554AFC   CField_Dojang::Update
  0x00642710   CUserLocal::CalculateMapDamage (sub_642710)
  0x007643FC   CSkillInfo::CheckConsumeForActiveSkill
  0x007656AA   CUserLocal::GetMortalBlowThreshold (sub_7656AA)
  0x0077EC9F   Equipment stat calc (sub_77EC9F)
  0x0078D46C   Stat recalculation (sub_78D46C)
  0x007A5B42   CSummoned::TryDoingHeal
  0x008C4100   CUIStatDetail::Draw (approx)
  0x008C5D00   CUIStat::Draw (approx)
  0x008CBDDB   CStatWnd::UpdateAbilityButtons (sub_8CBDDB)
  0x008CC8DE   CStatWnd::DrawStatValues (sub_8CC8DE)
  0x008CD8A0   CStatWnd::DrawStatDetails (sub_8CD8A0)
  0x008D8200   CUIStatusBar::Draw (approx)
  0x0091F001   CUIPartyHP constructor
  0x0091FADA   CUIPartyHP::Draw
  0x0094B000   CUserLocal::Update (approx)
  0x0095BA14   CUserLocal::TryConsumePetHP
  0x0095BC60   CUserLocal::TryConsumePetMP
  0x009584B0   CUserLocal::SetDamaged (approx)
  0x009804D0   CUserRemote::OnAttack
  0x009834E0   CUserRemote::OnHit
  0x00967700   CUserLocal::DoActiveSkill (approx)
  0x00776020   CStage::OnSetField
  0x00A02F17   CWvsContext::TryRecovery
  0x00A1E997   Recovery packet encode (sub_A1E997)
  0x00A23C45   CWvsContext::SendAbilityMassUpRequest
  0x00A29300   CWvsContext::CheckDarkForce
  0x00A2949F   CWvsContext::CheckDragonFury
  0x00A3E6FD   CWvsContext::OnPartyResult
  0x00A3F836   CWvsContext::GetPartyMemberData


VERIFIED: 2026-02-26 | Implementation: src/maxHPmaxMP.cpp
