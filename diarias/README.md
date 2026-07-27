# Daily Check-In — 28-day login-streak reward window

A custom **Daily Check-In** window for a **v83 MapleStory** client (Kaentake-style injector DLL)
plus a **Cosmic / HeavenMS-style** (Java/Netty) server. Players claim one reward per real day from a
28-day grid; missing a day resets the streak.

The bundled reward table is a **mock** that grants **1 meso per day** so it runs out of the box.
Replace it with your own rewards in `DailyCheckinRewards.java`.

---

## 1. What it does

- **Auto-opens on login** when a reward is ready (the server pushes the window).
- ****`@daily`** chat command opens it any time (and reports the cooldown when not ready).
- **28-day grid (7×4).** Each slot shows the reward's item icon. Click an unlocked, unclaimed day to
  claim it → a native confirmation dialog pops and the reward is granted.
- **One claim per 24 hours** (real epoch-millis gate). Claiming consecutive days advances the streak;
  letting >48h pass since the last claim resets you to Day 1; finishing Day 28 starts a new cycle.
- **Hover tooltip** per day showing the full reward breakdown (item names + mesos + slot bonuses).
- **States:** locked = dim, claimable = blinking gold border, claimed = green tint + check.
- **Level-gated:** only characters at or above `DailyCheckinRewards.MIN_LEVEL` (default **10**) can see
  or use it — lower-level characters get no auto-open and a "unlocks at Level N" message on `@daily`.

Everything rides a single opcode pair, **`0x11A`** (client→server) and **`0x17C`** (server→client).

---

## 2. What's in this package

```
DailyCheckin/
├── README.md                         ← you are here
├── wz/
│   └── UI.wz                         ← ready-made UIWindow.img/DailyCheckin/backgrnd (GMS); copy into your UI.wz (§6)
├── client/
│   └── src/dailycheckin.cpp          ← THE window (drop-in; depends on your client framework headers)
└── server/
    ├── src/main/java/
    │   ├── net/server/channel/handlers/DailyCheckinHandler.java   (copy)
    │   ├── server/DailyCheckinRewards.java                        (copy — the editable reward table)
    │   └── client/command/commands/gm0/CheckinCommand.java        (copy)
    └── sql/daily_checkin.sql          ← adds 3 columns to `characters`
```

The edits to **existing** server/client files are given as snippets in
[Section 4](#4--server-install-java) and [Section 5](#5--client-install-c).

---

## 3. Protocol (opcode `0x11A` / `0x17C`)

**Client → Server** (`RecvOpcode.DAILY_CHECKIN = 0x11A`): first byte is the action.

| Action | Byte | Extra |
|---|---|---|
| Open / request | `0` | — |
| Claim | `1` | `day(1)` (1..28) |

**Server → Client** (`SendOpcode.DAILY_CHECKIN = 0x17C`): `PacketCreator.dailyCheckinSnapshot`.

| Field | Type | Meaning |
|---|---|---|
| respType | `byte` = 1 | snapshot |
| currentDay | `byte` | highest day unlocked this cycle (0..28); the claimable day, or last-claimed during cooldown |
| claimedMask | `int` | 28-bit mask of days already claimed this cycle |
| justClaimed | `byte` | 0 = plain open; else the day just claimed → client pops the confirmation |
| count | `byte` = 28 | reward-table length |
| iconItemId × 28 | `int` each | the item icon drawn in each day's slot |
| tooltip × 28 | `string` each | per-day reward-breakdown text (newline-separated) |

> ⚠️ **String format:** the DLL reads each tooltip string as `uint16 length` + that many raw bytes
> (little-endian length). Cosmic/HeavenMS `OutPacket.writeString` already does exactly this
> (`writeShort(len)` + bytes). If your fork's `writeString` differs, match the DLL's `decStr()` or
> change one side.

> ⚠️ **Opcode collision:** `0x11A` and `0x17C` must be **free** in your `RecvOpcode`/`SendOpcode`.
> If not, pick free values and change them in **both** the Java enums **and** `dailycheckin.cpp`
> (`kOpcode_Send` / `kOpcode_Recv`).

---

## 4. Server install (Java)

### 4A. Copy the 3 new files (paths shown relative to `src/main/java/`)
- `net/server/channel/handlers/DailyCheckinHandler.java`
- `server/DailyCheckinRewards.java`
- `client/command/commands/gm0/CheckinCommand.java`

### 4B. Edit existing files (snippets)

**`net/opcodes/RecvOpcode.java`** — add an enum entry:
```java
DAILY_CHECKIN(0x11A),
```

**`net/opcodes/SendOpcode.java`** — add an enum entry:
```java
DAILY_CHECKIN(0x17C),
```

**`net/PacketProcessor.java`** — register the handler (next to the others):
```java
registerHandler(RecvOpcode.DAILY_CHECKIN, new net.server.channel.handlers.DailyCheckinHandler());
```

**`tools/PacketCreator.java`** — add the snapshot builder:
```java
public static Packet dailyCheckinSnapshot(int currentDay, int claimedMask, int justClaimed) {
    OutPacket p = OutPacket.create(SendOpcode.DAILY_CHECKIN);
    p.writeByte(1);                 // RESP_SNAPSHOT
    p.writeByte(currentDay);
    p.writeInt(claimedMask);
    p.writeByte(justClaimed);
    int n = server.DailyCheckinRewards.CYCLE_DAYS;
    p.writeByte(n);
    for (int d = 1; d <= n; d++) {
        p.writeInt(server.DailyCheckinRewards.iconItemId(d));
    }
    for (int d = 1; d <= n; d++) {
        p.writeString(server.DailyCheckinRewards.tooltip(d));
    }
    return p;
}
```

**`client/Character.java`** — add the streak state + logic.

Fields (next to the other character fields):
```java
private int  checkinDay = 0;        // last day claimed this cycle (0..28)
private int  checkinClaimed = 0;    // 28-bit mask of days claimed this cycle
private long checkinLastClaim = 0;  // epoch millis of the last claim (0 = never)
private static final long CHECKIN_PERIOD_MS = 86_400_000L;   // 24h
```

Methods (anywhere in the class):
```java
public int  getCheckinDay()       { return checkinDay; }
public int  getCheckinClaimed()   { return checkinClaimed; }
public long getCheckinLastClaim() { return checkinLastClaim; }

/**
 * Normalize the streak for the current time; return the claimable day (1..28), or 0 if still in the
 * 24h cooldown. Resets the cycle (clears day + mask) when the streak lapsed (>48h) or day 28 was
 * already claimed. Mutates in-memory state only (persisted on a claim).
 */
public int refreshCheckin() {
    final int cycle = server.DailyCheckinRewards.CYCLE_DAYS;
    if (checkinLastClaim <= 0) {           // never claimed -> fresh cycle, day 1 ready
        checkinDay = 0; checkinClaimed = 0;
        return 1;
    }
    long elapsed = System.currentTimeMillis() - checkinLastClaim;
    if (elapsed < CHECKIN_PERIOD_MS)        return 0;                       // cooldown
    if (elapsed >= 2L * CHECKIN_PERIOD_MS) { checkinDay = 0; checkinClaimed = 0; return 1; } // lapsed -> reset
    if (checkinDay >= cycle)               { checkinDay = 0; checkinClaimed = 0; return 1; } // cycle done -> new cycle
    return checkinDay + 1;                  // consecutive day available
}

/** Record a successful claim of day d: mark it, advance the streak, stamp the clock. */
public void applyCheckinClaim(int d) {
    if (d < 1 || d > server.DailyCheckinRewards.CYCLE_DAYS) return;
    checkinDay = d;
    checkinClaimed |= (1 << (d - 1));
    checkinLastClaim = System.currentTimeMillis();
}

/** Seconds remaining on the 24h cooldown, or 0 if a claim is available now. */
public long getCheckinCooldownSeconds() {
    if (checkinLastClaim <= 0) return 0;
    long remain = (checkinLastClaim + CHECKIN_PERIOD_MS) - System.currentTimeMillis();
    return remain > 0 ? (remain + 999) / 1000 : 0;
}
```

Load — in `loadCharacterData` where the row is read into `ret` (ResultSet `rs`):
```java
ret.checkinDay       = rs.getInt("checkinDay");
ret.checkinClaimed   = rs.getInt("checkinClaimed");
ret.checkinLastClaim = rs.getLong("checkinLastClaim");
```

Save — in `saveCharToDB` (inside the existing transaction). Either fold these three columns into your
existing `UPDATE characters SET ... WHERE id = ?`, or add a small dedicated statement:
```java
try (PreparedStatement psCheckin = con.prepareStatement(
        "UPDATE characters SET checkinDay = ?, checkinClaimed = ?, checkinLastClaim = ? WHERE id = ?")) {
    psCheckin.setInt(1, checkinDay);
    psCheckin.setInt(2, checkinClaimed);
    psCheckin.setLong(3, checkinLastClaim);
    psCheckin.setInt(4, id);
    psCheckin.executeUpdate();
}
```

**`net/server/channel/handlers/PlayerLoggedinHandler.java`** — auto-open on login (level-gated).
Near the END of `handlePacket` (after the character is fully loaded and in the field):
```java
if (c.getPlayer().getLevel() >= DailyCheckinRewards.MIN_LEVEL) {
    int checkinClaimable = c.getPlayer().refreshCheckin();
    if (checkinClaimable >= 1) {
        c.sendPacket(PacketCreator.dailyCheckinSnapshot(
                checkinClaimable, c.getPlayer().getCheckinClaimed(), 0));
    }
}
```
> Add `import server.DailyCheckinRewards;` to this file. (Cosmic's `PlayerLoggedinHandler` has a
> local variable named `server`, so the fully-qualified `server.DailyCheckinRewards` won't resolve —
> import it and use the simple name as shown.)

**`client/command/CommandsExecutor.java`** — register the command (gm level 0 = all players):
```java
addCommand(new String[]{"checkin", "daily"}, CheckinCommand.class);
```
(Match your project's `addCommand` signature; the important part is **level 0**.)

---

## 5. Client install (C++)

1. Copy `client/src/dailycheckin.cpp` into your injector's source folder.
2. Add it to your build (CMake/vcxproj — alongside your other feature `.cpp`s).
3. In your hook installer, declare and call the install entry:
   ```cpp
   void AttachDailyCheckinMod();   // forward declare
   ...
   AttachDailyCheckinMod();        // call inside AttachClientHooks() / your install routine
   ```
4. Build.

The file expects the same base framework headers your other features use: `pch.h`, `hook.h`,
`wvs/packet.h`, `wvs/wnd.h`, `wvs/iteminfo.h`, `wvs/util.h`, `wvs/wvsapp.h`, `wvs/wndman.h`,
`ztl/ztl.h` (providing `CWnd`, `COutPacket`/`CInPacket`, `get_rm()`, `CItemInfo`, `ZXString`,
`ATTACH_HOOK`, `ZALLOC_GLOBAL`, etc.). It hooks **`CClientSocket::ProcessPacket`** (chained — it
peeks `0x17C` and forwards everything else, so it coexists with other ProcessPacket hooks).

### Client addresses (v83, image base 0x400000)
If your client is a different build, re-find these and update the `kAddr_*` constants at the top of
`dailycheckin.cpp` (and `CWnd::CreateWnd` / `CItemInfo` in your wvs headers):

| Symbol | Address |
|---|---|
| `play_ui_sound` | `0x00989588` |
| `ProcessBasicUIKey` | `0x00A07431` |
| `CWvsContext` instance | `0x00BE7918` |
| `CClientSocket` instance | `0x00BE7914` |
| `CClientSocket::SendPacket` | `0x0049637B` |
| `CClientSocket::ProcessPacket` | `0x004965F1` |
| `CUtilDlg_Notice` (1-button dialog) | `0x009929DD` |
| `get_basic_font` | `0x0098A707` |
| `IWzFont::SetFont` | `0x0046341A` |
| `CItemInfo` singleton | `0x00BE78D8` |
| `CItemInfo::DrawItemIconForSlot` | `0x005D6458` |
| `CWnd::CreateWnd` | `0x009DE4D2` |

---

## 6. UI / WZ assets (included)

A ready-made **`wz/UI.wz`** ships with this package — a minimal **GMS-encrypted** `UI.wz` containing
exactly the one node the DLL needs:
```
UI.wz / UIWindow.img / DailyCheckin / backgrnd      ← Canvas, the window art (513 × 346)
```

### Copy the node into your own WZ
Open both `wz/UI.wz` (from this package) and your client's `UI.wz` in a WZ editor (HaRepacker /
HaCreator / any MapleLib-based tool), then **copy the `DailyCheckin` sub-property** from the bundled
`UIWindow.img` into **your** `UIWindow.img`, and save/repack:

- **Packed `UI.wz`:** copy `UIWindow.img → DailyCheckin` (the whole sub-property — it holds
  `backgrnd`) into your `UI.wz`'s `UIWindow.img`, then repack.
- **Extracted / loose `UIWindow.img`:** copy the same `DailyCheckin/backgrnd` canvas node across.
- The exact path the DLL loads at runtime is `UI/UIWindow.img/DailyCheckin/backgrnd`.

> ⚠️ The bundled `UI.wz` is **GMS-encrypted** (standard v83). If your client uses a different WZ key
> (BMS / EMS / custom), re-save the node with your client's encryption in your WZ editor.

> Using your own art? Just replace the `backgrnd` canvas with your own **513 × 346** PNG (keep the
> grid layout below, or adjust the constants in `dailycheckin.cpp`).

### Close button (reused, usually already present)
The DLL also loads the vanilla bag close button:
```
UI.wz / UIWindow.img / Bag / BtClose / normal / 0
UI.wz / UIWindow.img / Bag / BtClose / mouseOver / 0
```
These ship in standard v83 UI.wz. If your client doesn't have `Bag/BtClose`, change the two
`LoadSprite(L"UI/UIWindow.img/Bag/BtClose/...")` paths in `dailycheckin.cpp` to any close-button
canvas you have (and the `kBtClose*` rect to match its position/size).

### Art layout (so your `backgrnd` lines up with the grid)
The DLL's geometry constants assume a **513 × 346** window with a **7×4** grid: each cell has a
"DAY N" label strip on top and a content box below where the reward icon is drawn. Defaults:

| Constant | Value | Meaning |
|---|---|---|
| window | 513 × 346 | `kWndW` × `kWndH` |
| title bar | y 0–18 | draggable strip (`kTitleH`) |
| columns | left **6**, pitch **72**, 7 cols | `kCol0`, `kColPitch` |
| rows | label-top **56**, pitch **73**, 4 rows | `kRowLabel0`, `kRowPitch` |
| cell | 65 wide, 71 tall | `kCellW`, `kCellH` (label + box) |
| content box | x = cell-left, width 66; y = label-top **+21**, height **50** | where the icon + overlays sit |
| icon | 32px, centred at cell-left+16, bottom anchor cell-top+62 | `kIconDX`, `kIconBY` |
| close button | x 490, y 6, 12×12 (in the header band) | `kBtCloseX/Y/W/H` |

If your art uses different positions, edit those constants in `dailycheckin.cpp`. A simple matching
art = a 513×346 panel with a title bar, an optional banner, and 28 boxes (7 across, 4 down) each with
a "DAY 1".."DAY 28" label.

### Sounds
Uses standard UI sounds `MenuUp`, `BtMouseClick`, `BtMouseOver` (present in stock `Sound.wz/UI.img`).

---

## 7. Customizing the rewards

Everything is in **`server/DailyCheckinRewards.java`** → the `DAYS` table. The shipped table is a
mock (1 meso/day). Each day is a `Reward(iconItemId, mesos, Grant[], slotType, slotCount)`:

- `iconItemId` — the item whose icon shows in the slot (must be a real item id with an icon).
- `mesos` — mesos granted (0 = none).
- `Grant(itemId, qty[, expireDays])` — items granted; `expireDays > 0` for timed cash items.
- `slotType` / `slotCount` — optionally expand an inventory tab on claim
  (1=Equip 2=Use 3=Set-up 4=Etc 5=Cash) by `slotCount` slots.

Example:
```java
DAYS[6]  = new Reward(5220000, 0, new Grant[]{ new Grant(5220000, 1) });           // Day 7: a Gachapon Ticket
DAYS[13] = new Reward(2340000, 0, new Grant[]{ new Grant(2340000, 10) });          // Day 14: 10 White Scrolls
DAYS[20] = new Reward(2000005, 1_000_000, new Grant[]{ new Grant(2000005, 100) }); // Day 21: 100 Power Elixir + 1M mesos
DAYS[27] = new Reward(2000000, 0, NONE, 2, 8);                                      // Day 28: +8 Use slots
```
The hover tooltip is generated from this table automatically (item names via
`ItemInformationProvider.getName`). `CYCLE_DAYS` is 28; if you change it, also size your art's grid
to match (and the DLL `kDays` / grid constants).

---

## 8. Database

Run `java/sql/daily_checkin.sql` once against your game schema. It adds `checkinDay`,
`checkinClaimed`, `checkinLastClaim` to `characters` (idempotent). If you use Flyway/Liquibase,
register it as a migration instead.

---

## 9. How it works (flow)

```
login ─► PlayerLoggedinHandler: refreshCheckin(); if a day is claimable, push snapshot ─► [0x17C]
                                                                                              │
client receives 0x17C ─► CUIDailyCheckin opens, draws the 28-day grid from the snapshot
                                                                                              │
click a claimable day ─► [0x11A][1][day] ─► DailyCheckinHandler: refreshCheckin() validates the day
                                            is the claimable one ─► grantDay() ─► applyCheckinClaim()
                                            (stamps checkinLastClaim = now) ─► save ─► reply [0x17C]
                                                                                              │
reply has justClaimed=day ─► client grants notice + the day flips to "claimed"; next day stays
                             locked until 24h pass (server-enforced).

@checkin ─► CheckinCommand: refreshCheckin(); push snapshot (and report cooldown if not ready).
```

The **server is authoritative** — it enforces the 24h gate and which day is claimable; the client
only renders and requests.
