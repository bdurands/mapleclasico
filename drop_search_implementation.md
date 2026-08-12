# Drop Search (`@whodrops` / `@whatdropsfrom`) — Implementation Guide

How the in-game drop browser is built, and what you need to know to change it or build another
NPC-dialog browser like it.

Two commands, one feature:

| Command | Alias | Flow |
|---|---|---|
| `@whodrops <item name>` | `@wd` | item search → item list → the monsters that drop it |
| `@whatdropsfrom <mob name>` | `@wdf` | monster search → monster list → that monster's drop table |

Both open NPC **2040052 (Wiz the Librarian)** and share a single main menu, so a player who started
with `@wd` can jump to a monster search without retyping a command.

**Files**

| Path | Role |
|---|---|
| `src/main/java/server/DropSearchService.java` | All rendering, search, rates, WZ/DB lookups |
| `scripts/npc/whodrops.js` | Conversation state machine, opens in item mode |
| `scripts/npc/whatdropsfrom.js` | Same state machine, opens in monster mode |
| `src/main/java/client/command/commands/gm1/WhoDropsCommand.java` | Launches the script |
| `src/main/java/client/command/commands/gm1/WhatDropsFromCommand.java` | Launches the script, owns the NPC id constant |
| `src/main/java/client/command/CommandsExecutor.java` | Command + alias registration |

---

## 1. Architecture

```
@wd / @wdf              → Command: validate, dispose any open conversation, start the NPC script
scripts/npc/*.js        → State machine: which view, which page, which mode. No formatting.
server.DropSearchService → Search, rate math, and the finished page text. No conversation state.
```

The split matters. The script cannot format anything (JS in this engine is slow and awkward for string
building at this scale), and the service cannot hold conversation state (it is a static singleton shared
by every player). Keep that boundary: **the service returns page text, the script remembers indices.**

### Command layer

Register the aliases with the array overload — rank is preserved:

```java
addCommand(new String[]{"whatdropsfrom", "wdf"}, WhatDropsFromCommand.class);
addCommand(new String[]{"whodrops", "wd"}, WhoDropsCommand.class);
```

The command body is deliberately not `openNpc()`:

```java
String query = player.getLastCommandMessage();
if (params.length < 1 || query == null || query.isBlank()) { /* usage message */ return; }

c.removeClickedNPC();                                   // bypass the 500ms NPC click cooldown
NPCScriptManager.getInstance().dispose(c);              // drop any conversation already open
NPCScriptManager.getInstance().start(c, DROP_SEARCH_NPC, "whatdropsfrom", player);
```

`AbstractPlayerInteraction.openNpc()` returns early when `c.getCM() != null`, which makes the command look
dead if the player re-runs it while a dialog is still open. The three lines above are `openNpc()` without
that guard.

The search term travels in `lastCommandMessage`, which `CommandsExecutor` sets before `execute()`. The
script reads it in `start()`.

### Script state machine

Both scripts are the same file except for the entry mode (`var mobMode = true|false`). States:

| State | Screen | Sent with |
|---|---|---|
| `main` | main menu | `cm.sendSimple` |
| `input` | search text box | `cm.sendGetText` |
| `list` | paginated search results | `cm.sendSimple` |
| `detail` | drop table / dropper list | `cm.sendSimple` |

The result list is rendered **one page at a time** (`mobListPage(query, ids, page)`), because each row
needs that monster's Mob.wz img for its level — building all ten pages up front pays that ten times. The
detail views are built as a whole `String[]` up front, since they cost one DB read regardless.

`sendGetText` replies arrive as `action(mode=1, type=2, selection=-1)`; read `cm.getText()`. A cancelled
text box disposes the conversation for you, so no handling is needed. Check the `input` state **first** in
`action()`, before any selection comparison.

---

## 2. Dialog markup: what actually works

This is the part that is easy to get wrong. See also `docs/npcmarkups.txt` for the full code list.

### 2.1 Item tooltips

To make a row show the real inventory-style item info window on hover:

```java
"#L" + itemId + "##i" + itemId + ":# #b#z" + itemId + "##k" + trailer + "#l"
```

The mechanism, from the client's markup code (MushMS.exe, imagebase 0x400000): every parsed element is
0x44 bytes, and **`elem+0x04` holds a tooltip item id**. The hover hit-test at `0x009ABEBD` branches on
that field — non-zero goes to `CItemInfo::MakeItemSlotFromID` (0x005D5D95) → `CUIToolTip::SetToolTip_Item`
(0x008F5B20), the same call a real inventory slot makes; zero falls through to `SetToolTip_String`
(0x008E6E7D), a plain name-only box that reads as "no tooltip".

Consequences, in order of how likely they are to bite you:

- **`#z<id>#` is what arms it.** Its handler (0x009A1BB8) writes `elem+0x04` unconditionally. The item name
  in a row must be the client-side `#z` token, **never** a server-side Java string — a plain string is just
  text and produces nothing on hover.
- **`#i` and `#v` are the same handler** (0x0099E530, jump-table entries [7] and [8] point at one address).
  Do not assume `#v` is "the tooltip one".
- That handler arms `elem+0x04` **only when the token contains a `':'`** (strchr at 0x0099E559). So
  `#i<id>#` gives a picture with no tooltip; **`#i<id>:#`** gives a picture that is hoverable. `atoi` stops
  at the colon so the id still parses. Shipped precedent: `#v1142107:#` in `scripts/quest/29900-29903.js`.
- The tooltip is anchored to **each element's own rect**. Without the colon only the name is hot, which is
  why a row can look correct and still do nothing when the player hovers the icon.
- Being inside `#L…#l` is **not** what makes the tooltip work — the only link-ish gate in the hit-test
  (`elem+0x28` vs `m_6D0`) is the typewriter reveal cursor, inert once the text has finished appearing.
  Wrap rows in `#L` for selectability; a no-op redraw is a fine click handler for an informational row.

Format rules that go with the template: `#b` goes **before** `#z` (the colour wraps the token, it is not
part of it), the trailer starts with a **plain space** — never a colour code, since the name already closes
with `#k` — and no `#e`/`#n` inside the row.

### 2.2 Never print a raw WZ name

Six `Etc.img.xml` names contain a literal `#`: 4031095/96/97 "Box of Parts #1/#2/#3" and 4031141/42/43
"Parts #1/#2/#3". Printing one raw feeds the client a bare `#1` mid-line and corrupts every token after it
on that row. `#z<id>#` is immune by construction; anything printed directly (mob names, the no-icon
fallback) goes through a `#`-stripping helper first.

### 2.3 Monster icons

Monsters are drawn as their **monster-book card artwork**:

```java
"#f" + "Item/Consume/0238.img/" + paddedTo8(cardId) + "/info/iconRaw#"
```

Two traps:

- **`#i<cardid>#` does not work.** Every monster card shares one generic 32×32 `info/icon` — verified by
  pulling the bitmaps out of the client: cards 2384011 and 2384031 return byte-identical base64. A whole
  list comes out wearing the same picture. The per-monster art is the 27×38 **`info/iconRaw`**, which is
  what the Monster Book itself draws, and it is reachable only through `#f`.
- **Do not draw the monster's own Mob.wz sprite.** It was tried and removed: Von Leon's `stand/0` frame is
  554×506, far wider than the dialog's text column, and three of them on one page closed the client. Card
  art at 27×38 is nowhere near that limit. If you ever need a mob sprite anyway, the provider does not
  expose canvas width/height — read the frame's `origin` vector instead (`origin.y` ≈ height,
  `2*origin.x` ≈ width) and cap it well under 120px.

Monsters with no card, or whose card id has no WZ entry, fall back to the generic card back
(`…/02380000/info/icon`). A `monstercarddata` row alone is not enough for the client to draw anything —
see §6.

### 2.4 Selection values

Three tiers that can never collide:

| Range | Meaning |
|---|---|
| `0 … pageCount-1` | page links (`#L<index>#Page <index+1>#l`) |
| item / mob ids (≥ 100000) | content rows |
| `10000001 … 10000007` | `SEL_PREV`, `SEL_NEXT`, `SEL_SEARCH`, `SEL_MAIN`, `SEL_LIST`, `SEL_SEARCH_MOB`, `SEL_SEARCH_ITEM` |

The client returns **the number written in `#L<n>#`**, not the row ordinal, so sentinels are safe. The
script disambiguates with `selection < 1000` for page links.

### 2.5 Layout gotchas

- A "simple" (type 4) selection dialog with **zero** `#L` entries gives the client nothing to draw. Every
  screen must emit at least one link — the `[Back to main menu]` footer covers this.
- A row of `#L` links renders **taller** than a plain text row, and the client draws the following text
  line *over* it instead of below it. Put a blank line between a link row and any text line that follows —
  this is exactly what makes the page strip and the counter rule collide if you forget.
- Page strip layout: numbered links `PAGE_LINKS_PER_ROW` (5) to a row, current page in `#r`, then a blank
  line, then the `n / total` rule, then Prev/Next.

---

## 3. Rendering: page structure

Every screen is built as:

```
header          (what you searched / which monster or item)
rows            (one #L…#l link per row)
pager           (blank line, page strip, counter rule, Prev/Next)
footer          ([Back to the results] when applicable, [Back to main menu])
```

Page sizes live in one place at the top of the service:

```java
public static final int MAX_LIST_PAGES = 10;   // SEARCH RESULT lists only
private static final int MOBS_PER_PAGE = 8;
private static final int ITEMS_PER_PAGE = 10;
private static final int DROPS_PER_PAGE = 12;
private static final int SOURCES_PER_PAGE = 8;
private static final int PAGE_LINKS_PER_ROW = 5;
```

`MAX_LIST_PAGES` caps the **search result lists only** — a 300-hit search is one that wants narrowing, and
the footer says so (`showing the first N of M — narrow your search`). Once a monster or item is picked, its
drop table is never truncated.

The drop table is grouped into fixed sections, in this order: **Equipments, USE, ETC, Quest Items, Card
drop rate**. Classification:

```java
if (ItemId.isMonsterCard(itemId))                            → Card drop rate
else if (questid > 0 || isQuestItem(itemId))                 → Quest Items
else switch (itemId / 1000000) { 1 → Equipments; 2 → USE; default → ETC }
```

Empty sections are skipped. A section header is re-emitted at the top of the next page when a section
carries over. Quest Items carries a note that those drops may need the matching quest active — that is
`drop_data.questid` gating through `Character.needQuestItem`.

---

## 4. Rate math

Chances are shown at the rates **the asking player** actually rolls at, mirroring
`MapleMap.dropItemsFromMonsterOnMap` exactly:

```java
int    dropRate = mobIsBoss ? chr.getBossDropRate() : chr.getDropRate();
double eff      = (double) entry.chance * dropRate * chr.getCardRate(entry.itemId);
if (ItemId.isMonsterCard(entry.itemId)) {
    eff *= chr.getWorldServer().getCardDropRate();
}
// eff is out of 1,000,000
```

Displayed as `1/X (Y%)`:

```java
long oneIn = Math.max(1, Math.round(1_000_000.0 / eff));
double pct = eff / 10_000.0;          // clamp to "1/1 (100%)" once eff >= 1,000,000
```

Percent precision scales with magnitude (1 decimal ≥10%, 2 ≥1%, 3 ≥0.01%, else 4) so a 1/10,000 drop does
not render as `0.0%`.

Header values:

- **EXP** — `mobExp * chr.getExpRate()` (that method already folds in coupons and `EXP_RATE_BY_LEVEL`).
- **Mesos** — the `drop_data` row with `itemid = 0`; `Minimum`/`Maximum` × `chr.getMesoRate()`, plus that
  row's own drop chance.
- The applied rates are printed under them so the numbers are auditable.

---

## 5. Crash guards

Two guards exist because both failure modes were hit in production. Do not remove them.

### Item info window

`#z` makes the client actually run `MakeItemSlotFromID`, which dies on equips it cannot type:

```java
private static boolean hasSafeIcon(int itemId) {
    if (itemId / 1000000 != 1) return true;                     // non-equips are always fine
    if (!equipIdsInWz().contains(itemId)) return false;         // no Character.wz img → no icon to draw
    int prefix = itemId / 10000;
    if (prefix >= 121 && prefix <= 169) {                       // weapon directory range, excluding cash
        return getWeaponType(itemId) != WeaponType.NOT_A_WEAPON;
    }
    return true;
}
```

Weapon category is `(itemId / 10000) % 100`; anything outside 30–49, or in the 34/35/36/39 holes, resolves
to `NOT_A_WEAPON`. Post-BB imports land there — Timeless 121-125/134/136/152-155, Lionheart 1232014 /
1542015 / 1582015 — and it is the same class of data that disconnects players on equip.

Two deliberate exceptions:

- **Cash weapons at 170xxxx stay allowed.** Players carry them daily, so the client clearly handles them.
- **Post-BB armour prefixes 113/114/115** (belt, medal, shoulder/charm) **stay allowed** — players own
  those too.

A rejected item gets **neither `#i` nor `#z`**, just an escaped plain name: no tooltip on that row, but the
client stays open. Do not "fix" that branch by giving it `#z`.

### Oversized canvases

Covered in §2.3 — `#f` is safe, oversized canvases are not.

---

## 6. Data prerequisites

| Source | Used for | Failure mode |
|---|---|---|
| `drop_data` | drop tables, dropper lists | — |
| `monstercarddata` | mob id → card id | no row → blank card back |
| `Item.wz/Consume/0238.img` | card artwork (`info/iconRaw`) | id missing → blank card back |
| `String.wz/Mob.img` | monster name index | — |
| `Character.wz` | equip existence check | — |

`monstercarddata` and the WZ can drift: a row may name a card the WZ does not ship. The service counts
those once at first use and logs

```
N of M monstercarddata rows point at a card id with no info/iconRaw in Item.wz/Consume/0238.img
```

so the gap is visible without anyone hunting for it. **The fix is a data sync, not a code change.**

---

## 7. Performance

Three traps, all of which made a single lookup take seconds before they were fixed:

1. **`DataProviderFactory.getDataProvider(...)` builds a brand new provider on every call** and rewalks
   that WZ's whole directory tree — Character.wz is ~34k files. Never call it in a loop. String/Item/
   Character providers already exist as `protected` fields on `ItemInformationProvider` (same package),
   so borrow those; hold anything else in a static field.
2. **`MonsterInformationProvider.getMobNameFromId` reparses the 247KB `String.wz/Mob.img` on every cache
   miss.** Resolving 60 new monster names means 60 full XML parses. Build your own id → name index instead.
3. **Name normalisation.** The search compares an accent/case/punctuation-free form. Doing that with
   regexes (`replaceAll` × 4 plus an unconditional `Normalizer`) over the ~34k item names dominated the
   first search. A single-pass char loop, decomposing only strings that actually carry non-ASCII, is ~5×
   faster and was verified byte-identical against the regex version over all 49,789 real WZ names.

Everything else is cached behind a `volatile` field with a `clearCaches()` escape hatch: the mob name
index, the item name index, mob level/exp/HP/boss, `monstercarddata`, the Character.wz equip id set, and
the card artwork id set.

Search ranking is exact match → prefix match → substring, then by id, capped at `SEARCH_HARD_CAP` (2000)
so the counter can still report the true match count.

---

## 8. Service API

```java
// search
int[]    findMobs(String query)
int[]    findItems(String query)

// screens
String   mainMenu(String notice)                                  // notice may be null
String   searchPrompt(boolean mobMode)                            // caption for cm.sendGetText

int      mobListPageCount(int totalMobs)
String   mobListPage(String query, int[] mobIds, int page)        // rendered per page
String[] mobDropPages(Character chr, int mobId, boolean withList)

int      itemListPageCount(int totalItems)
String   itemListPage(String query, int[] itemIds, int page)      // rendered per page
String[] itemDropperPages(Character chr, int itemId, boolean withList)

// misc
String   escapeUserText(String text)                              // strips '#'
void     clearCaches()
```

`withList` is `false` when a search resolved to exactly one hit and the list screen was skipped — it
suppresses the `[Back to the results]` link.

---

## 9. Testing checklist

Tests cannot run in-agent (no live DB or client), so verify against a running server:

- [ ] `@wd`, `@wdf`, `@wd <name>`, `@wdf <name>`, and both aliases
- [ ] Hover an item **name** — the full info window appears
- [ ] Hover an item **icon** — same window (this is what the `:` in `#i<id>:#` buys)
- [ ] A search with 1 hit skips the list; `[Back to main menu]` still present
- [ ] A search with 0 hits returns to the main menu with the notice
- [ ] Page strip and the counter rule do not overlap
- [ ] Pages past 10 are unreachable on a result list, reachable on a drop table
- [ ] The in-list search line reopens the text box and re-searches in place
- [ ] Main menu offers both searches regardless of which command opened it
- [ ] A monster with no card shows the blank card back, not a missing image
- [ ] Search a post-BB weapon (e.g. `lionheart`) — rows render, client stays open
- [ ] Search an item whose name contains `#` (e.g. `Box of Parts`) — the row renders intact
- [ ] Boss drop tables use the boss drop rate, normal monsters the normal rate

## 10. Extending it

- **Cross-navigation** (click an item in a drop table → who else drops it; click a monster in a dropper
  list → its full table): both renderers already exist in the service and the script already has a mode
  flag. Currently a click on a detail row is a no-op redraw.
- **More sections**: add to `SECTIONS[]` and the `section(...)` classifier together — the index into that
  array is the sort key.
- **Another browser entirely**: the shape here — command starts a script, script holds state, service
  returns pages — transfers directly. §2 is the part worth re-reading first.
