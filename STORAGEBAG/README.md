# Storage Bag (per-Character) — Full Feature + Repair

A tabbed **Storage Bag** for MapleStory **v83** (HeavenMS / BBrStory-derived cores).
One scrollable window, opened by a **BAG** button on the inventory, with four tabs:

| Tab | Holds | Item id rule |
|-----|-------|--------------|
| **Ore** | Maker materials / ores (ETC) | `ItemConstants.makerItemIds` set |
| **Scroll** | Upgrade scrolls (USE) | `itemId/10000 == 204` **or** White Scroll `2340000` |
| **Chair** | Chairs (SETUP) | `itemId/10000 == 301` |
| **Mount** | Mounts / taming mobs (EQUIP) | `itemId/1000 == 1902` or `1912` |

Each bag holds up to **200 slots** and behaves exactly like the normal inventory:
per-character persistence, drag in/out, stack merge, an **Auto**-collect toggle
(auto-stores matching items on pickup), a live **name search**, and an
**Organize** (merge + compact) button.

> This is the **per-CHARACTER** edition: bag items are keyed by `characterid`
> (not by account). There is **no** `World.java` change and **no** per-account
> cache — a character simply loads its own four bags at login.

---

## 0. What's in the box

```
StorageBag_Repair/
├─ README.md                         ← this guide
├─ server/
│  ├─ db/
│  │  └─ storagebag_percharacter.sql ← schema migration (RUN ONCE)
│  └─ src/main/java/…                ← 10 Java files (full, ready to diff/merge)
├─ client/
│  ├─ storagebag.cpp                 ← the isolated bag module (drop-in)
│  ├─ slotlock.cpp                   ← slot-lock file w/ right-click-deposit hook
│  └─ wiring/                        ← 3 tiny snippets: hook.h / CMakeLists / PacketDispatcher
└─ ui/
   ├─ StorageBag.img                 ← the custom /Bag node, extracted (v83, import-ready)
   └─ art_reference/                 ← reference PNGs + node-tree doc
```

## Requirements

* A v83 HeavenMS-derived server (Java 11+, Maven) with the standard
  `inventoryitems` / `inventoryequipment` tables.
* A v83 client DLL project (Microsoft Detours) — this feature is a hook module.
* MySQL/MariaDB access to run one migration script.

---

# 1. SERVER install

### Step 1.1 — Run the SQL (once)

Run **`server/db/storagebag_percharacter.sql`** on your game database
**as a single script** (it uses `DELIMITER` for a migration procedure — do not
paste it line by line).

It is **idempotent / production-safe** and does three things:

1. Adds four per-character auto-collect toggle columns to `characters`:
   `autoOreStorage`, `autoScrollStorage`, `autoChairStorage`, `autoMountStorage`
   (renaming an old `autoCashStorage` if you ran an earlier per-account build).
2. Deletes only *legacy per-account* bag rows (`inventoryitems` with
   `type IN (10,11,12,13)` **and** `characterid IS NULL`). On a clean DB this
   removes 0 rows and never touches per-character bag items.
3. Drops the unused old `orestorages` metadata table if present.

> **Mandatory.** The Java reads/writes those four columns on character
> load/save. Without them, character load throws and **players cannot log in**.

### Step 1.2 — Add / merge the Java files

Bag items live in the existing `inventoryitems` table (plus the
`inventoryequipment` JOIN for mount equips), keyed by `characterid` + a `type`
value that separates each bag: **10=ore, 11=scroll, 12=chair, 13=mount**.
No new tables.

**NEW files — drop straight in:**

| File | Notes |
|------|-------|
| `server/OreStorage.java` | Backs all 4 bags (kind 0-3). Fixed-200-slot model. |
| `net/server/channel/handlers/BagWindowHandler.java` | Server side of the protocol. |

**EDIT files — merge the bag additions into YOUR copy** (these are core files;
your versions differ, so diff — don't blindly overwrite). Provided full for
reference:

| File | What to add |
|------|-------------|
| `net/opcodes/RecvOpcode.java` | `BAG_WINDOW(0x3724),` |
| `net/opcodes/SendOpcode.java` | `BAG_WINDOW(0x3725),` |
| `net/PacketProcessor.java` | `registerHandler(RecvOpcode.BAG_WINDOW, new net.server.channel.handlers.BagWindowHandler());` |
| `client/inventory/ItemFactory.java` | 4 enum entries: `OREBAG(10,false), SCROLLBAG(11,false), CHAIRBAG(12,false), MOUNTBAG(13,false)` |
| `constants/inventory/ItemConstants.java` | `makerItemIds` set + `isOreBagAllowed / isScrollBagAllowed / isChairBagAllowed / isMountBagAllowed` |
| `tools/PacketCreator.java` | `bagWindowSnapshot(int bagKind, OreStorage storage, boolean auto)` |
| `client/Character.java` | see anchors below |
| `server/StorageInventory.java` | **stock file** — only the Repair (see §4). If yours is unmodified stock you can drop it in. |

**`Character.java` anchors** (search these in the provided file to copy the exact code):

* 4 storage fields — `private server.OreStorage orestorage/scrollstorage/chairstorage/mountstorage`
* Auto/used flags — `autoOreStorage…` + `usedOreStorage…` (+ their getters/setters)
* Load at login — `ret.orestorage = server.OreStorage.loadOreStorage(ret.id);` (×4)
* Save on save-transaction — `if (orestorage != null && usedOreStorage) { orestorage.saveToDB(con); … }` (×4)
* Auto-collect on pickup — the `gainItem`/pickup path branch that calls
  `isOreBagAllowed(...)` etc. and stores into the matching bag
* Accessors — `getOreStorage()… ` and the static filter `bagAccepts(int kind, int itemId)`

> Tip: in the provided source, most additions carry a `Storage Bag` marker
> comment — grepping `Storage Bag` / `orestorage` / `bagAccepts` / `autoOreStorage`
> across the provided files surfaces every insertion point.

### Step 1.3 — Recompile & restart

```
mvn -q clean package
# deploy the new jar, then:
systemctl restart <your-service>     # or however you launch the server
```

---

# 2. CLIENT install

### Step 2.1 — Add the module

Copy **`client/storagebag.cpp`** into your DLL `src/`. It is self-contained
(the whole bag window: rendering, tabs, scroll, drag in/out, search, Auto,
Organize, snapshot parser, and the inventory BAG button hook).

### Step 2.2 — Wire it (3 tiny edits)

Apply the three snippets in **`client/wiring/`**:

1. `hook.h.txt` — declare + call `AttachBagWindowMod();`
2. `CMakeLists.txt.txt` — add `"storagebag.cpp"` (and `"slotlock.cpp"` if not already built)
3. `PacketDispatcher.cpp.txt` — route incoming opcode **0x3725** to
   `BagWindow_HandleSnapshotPacket(iPacket)`

### Step 2.3 — slot-lock integration (optional but recommended)

**`client/slotlock.cpp`** carries the inventory right-click behavior this
feature expects:

* **Ctrl + right-click** on an inventory slot → toggle the slot lock (the old
  plain right-click lock is now gated behind Ctrl).
* **Plain right-click** on an inventory item **while a bag is open** → deposit
  that item into its matching bag. If no bag is open, plain right-click does
  nothing special.

If your project already has a `slotlock.cpp`, merge those two branches (the
file forward-declares `bool BagWindow_DepositFromInventory(int invType, int slot);`
which `storagebag.cpp` exports). If you don't use slot-lock at all, the bag
still works by drag-and-drop — this step only adds the right-click shortcut.

### Step 2.4 — Rebuild the DLL

Rebuild the injector target; confirm `storagebag.cpp` compiled and the DLL
exports/loads as before.

---

# 3. UI / WZ art

The client loads all bag art from **`UI/UIWindow.img/Bag/*`**. That single
`Bag` node ships here as **`ui/StorageBag.img`** — a v83 image containing
*only* the `Bag` node (extracted from the source `UIWindow.img`, so none of
the rest of that window comes with it).

**How to install it** — with a WZ editor (HaRepacker / HaCreator / WzComparerR):

1. Open **`ui/StorageBag.img`** and your client's **`UI.wz/UIWindow.img`**.
2. Copy the **`Bag`** SubProperty from `StorageBag.img` and paste it under the
   root of your `UIWindow.img` (so it becomes `UIWindow.img/Bag`).
3. Save `UI.wz`. Done — the client resolves `UI/UIWindow.img/Bag/*` at runtime.

> The `Bag` node must sit at `UIWindow.img/Bag` exactly — `storagebag.cpp`
> looks it up by that path and its geometry is measured against `backgrnd`
> (207×248). Server-side (BBrData) does **not** need this art; it is purely
> client rendering.

For reference (and if you'd rather rebuild by hand), the exact tree — with the
PNGs in `ui/art_reference/`:

```
UI/UIWindow.img/Bag/
├─ backgrnd        (Canvas 207×248)   window background + 5×5 grid
├─ tabOre          (Canvas 39×12)     tab label, selected
├─ tabOreOff       (Canvas 39×12)     tab label, unselected
├─ tabScroll       (38×12)  / tabScrollOff
├─ tabChair        (38×12)  / tabChairOff
├─ tabCash         (42×12)  / tabCashOff   ← reused as the 4th (MOUNT) tab
├─ BtClose         (SubProperty: normal/mouseOver/pressed/disabled)
├─ BtOreBag        (SubProperty: 4 states) ← the BAG button drawn on the inventory
└─ BtAuto          (SubProperty: 4 states) ← the Auto-collect toggle
```

> Cosmetic note: the 4th tab reuses `tabCash` art. If you want it to read
> "Mount"/"Montaria", repaint `tabCash`/`tabCashOff` in your WZ — no code change.

---

# 4. THE REPAIR — Organize button wiped bags with 128+ items

This is the data-loss fix that motivated this package. If you **already run**
the feature, this is all you need to patch.

### Symptom
A player fills a bag, clicks **Organize**, and the whole bag vanishes
(permanently, after the next character save). Only happens with **more than
127 items** in the bag — so heavy **chair** collectors (chairs don't stack:
N chairs = N slots) and **scroll** hoarders hit it; small bags never do.

### Root cause — `server/StorageInventory.java`
`Organize` calls `OreStorage.mergeStacks()` → `new StorageInventory(c, items)`,
whose constructor did:

```java
this.slotLimit = (byte) toSort.size();   // 128 → -128, 200 → -56
```

With **>127** items the `byte` cast goes **negative**, so `getNextFreeSlot()` /
`isFull()` reject **every** `addItem` in the constructor → `mergeItems()` runs
on an empty map → `sortItems()` returns an **empty list** → `mergeStacks()`
sets `items = []`. The bag is wiped in memory and the wipe is persisted on the
next save. (Native account storage caps at 48 slots, so stock never triggered
this — the 200-slot bags exposed the latent bug.)

### The fix (2 files, no DB / no client / no WZ)

**A) `server/StorageInventory.java`** — widen the counter from `byte` to `int`
(3 spots):

```java
// field
private final int slotLimit;                 // was: private final byte slotLimit;

// constructor
this.slotLimit = toSort.size();              // was: (byte) toSort.size();

// getter
private int getSlotLimit() { … }             // was: private byte getSlotLimit()
```

**B) `server/OreStorage.java`** — a safety net in `mergeStacks()` so a
consolidation can never persist an empty bag (belt-and-suspenders against any
future regression):

```java
StorageInventory msi = new StorageInventory(c, items);
msi.mergeItems();
List<Item> merged = msi.sortItems();
if (merged.isEmpty() && !items.isEmpty()) {   // never empty a non-empty bag
    log.error("[OreStorage] mergeStacks emptied a non-empty {} bag (characterId {}, {} items) - aborting reorg to avoid item loss",
            KIND_NAME[kind], id, items.size());
    return;
}
items = merged;
// … existing compaction to positions 0..N-1 …
```

### Applying just the Repair
Replace `StorageInventory.java` + `OreStorage.java` with the ones in this
package (or hand-apply the changes above), then `mvn clean package` and
restart. **No SQL, no client rebuild, no WZ change.**

> Honest note: items already lost **before** deploying the fix do **not** come
> back — that wipe was already committed to the DB. The fix stops it from the
> recompile onward.

### Why it's safe (zero side effects)
* Native account storage is capped at 48 slots, so its count is always ≤127 →
  `(byte)n == (int)n` → behavior is **byte-for-byte identical** there.
* Bags are the only path that exceeds 127 → the only behavior change is
  "organizes correctly" instead of "wipes".
* `slotLimit` is never serialized; loops use `short` counters (max 200 fits) →
  no protocol change, no overflow, no infinite loop.

---

## Protocol reference

```
client → server  0x3724  CP_BagWindow
   byte action   0=OPEN 1=WITHDRAW 2=DEPOSIT 3=MERGE 4=SET_AUTO 5=MOVE
   byte bagKind  0=ore 1=scroll 2=chair 3=mount
   (per-action payload: see BagWindowHandler.java header)

server → client  0x3725  LP_BagWindow  (snapshot after every action)
   byte 1 (RESP_SNAPSHOT) · byte bagKind · short count
   count×[ short slot · itemInfo ] · count×[ short qty ] · byte autoFlag
```

## Credits
Storage Bag feature + per-character migration + organize-wipe repair, built on
a HeavenMS/BBrStory v83 core. Share freely.
