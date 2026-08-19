# Integrating the Coloring Prism

Work step-by step in the following order. The database and WZ steps are independent, but the server will not
compile until step 3 is finished, and the window will not open until step 4 is.

What it dyes: any equip, a worn item's effect art, cash effect
items in the `5010000..5019999` group, and the character's own hair, eyes and skin.

Opcodes used: **`0x372E`** client to server, **`0x372F`** server to client. Item:
**`5782000`**. All three are quoted verbatim below; change them only if they collide
with something you already use, and change them in all of the places listed.

---

## 1. Database

Copy `server/db/coloring-prism.xml` into `src/main/resources/db/extensions/`. That is the
whole step.

> **Do not also add an `<include>` for it.** Stock Cosmic's `db/changelog-root.xml` already
> ends with `<includeAll path="extensions" relativeToChangelogFile="true"/>`, which is
> upstream's own mechanism for exactly this: "create an extensions directory and put them
> in there". Adding an explicit `<include>` on top registers every changeset in the file a
> second time under the same identifier, and Liquibase fails validation and aborts the
> server at boot rather than running either copy. If your fork removed that `includeAll`,
> then add the one line instead: `<include file="db/extensions/coloring-prism.xml"/>`.

That adds eighteen columns and nothing else: six on `inventoryequipment`, nine on
`characters`, three on `inventoryitems`. The last changeset only puts the item in shop
`1337` (GM shop) so you can buy one for testing; retarget the shopid or delete that changeset if
you don't want it added.

Liquibase runs off the classpath, so rebuild the jar before restarting.

**Check it took:**

```sql
SHOW COLUMNS FROM inventoryequipment LIKE 'tint%';
SHOW COLUMNS FROM characters LIKE '%tint%';
SHOW COLUMNS FROM inventoryitems LIKE 'efftint%';
```

Six rows, then nine, then three.

The third set is called `efftint*` rather than `tint*` on purpose. `ItemFactory` joins
`inventoryitems` and `inventoryequipment` with `SELECT * ... USING(inventoryitemid)`, and
one site wraps that in a second `SELECT *`. Two columns of the same name in one result
set is a hard `Duplicate column name` from MySQL, thrown on the **login** path. Do not
rename these to match the equip columns.

---

## 2. WZ data

Three fragments under `wz/client/`. Each is a normal `.img` whose **root children are
the nodes to merge**, so open both in a WZ editor and copy the children across.

| fragment | merge into | what it is |
|---|---|---|
| `wz/client/UI/ColorPrism.img` | `UI.wz/UIWindow.img`, as a new `ColorPrism` node | the window art |
| `wz/client/String/Cash.img` | `String.wz/Cash.img` | the item's name and description |
| `wz/client/Item/Cash/0578.img` | `Item.wz/Cash/0578.img` | the item's icons and `cash` flag |

`ColorPrism.img` contains `backgrnd` (the 301x406 backdrop), the three 176x8 slider
gradients `trackTone` / `trackChroma` / `trackBright`, and `backgrndLook`, an optional
second backdrop without the drop well that the Hair, Eyes and Skin tabs use if it is
present.

The backdrop **bakes the window title and the three `HUE` / `CHROMA` / `VALUE` row
labels**. Everything else the window shows is drawn at runtime: the banner copy, the tab
labels, the item icon, the gradients, the thumbs, the numbers and the buttons. If you
redraw or localize the backdrop, those two pieces of text are yours to carry over.

Everything else the window draws is stock v83 and needs no import: `Basic.img/BtOK2`,
`BtCancel2`, `Tab2`, `Slider/thumb*`, and `UIWindow.img/Bag/BtClose`.

**The server needs its own copy of the item data.** If your server reads WZ from XML,
merge `wz/server/String.wz/Cash.img.xml` and `wz/server/Item.wz/Cash/0578.img.xml` into
the matching files. The server reads the name and the `cash` flag from its own copy, so if
`05782000` is missing there the item does not exist as far as the server is concerned: it
cannot be put in a shop or spawned by a command, and it will not land in the **Cash** tab,
which is the inventory the handler looks in for the prism it is about to consume.

---

## 3. Server

Two new files, and edits to ten existing ones. **`Character.java` is touched twice** and
that catches people out: it gets the nine look-tint fields AND the `syncWeaponTint()`
helper in 3.6, then one call to that helper in 3.8.

| file | what goes in | step |
|---|---|---|
| `server/colorprism/ColorPrismPackets.java` | new file, drop in as-is | 3.1 |
| `net/server/channel/handlers/WeaponTintHandler.java` | new file, drop in as-is | 3.1 |
| `constants/id/ItemId.java` | one constant | 3.2 |
| `net/opcodes/RecvOpcode.java` | one enum entry | 3.3 |
| `net/opcodes/SendOpcode.java` | one enum entry | 3.3 |
| `net/PacketProcessor.java` | one `registerHandler` line | 3.4 |
| `client/inventory/Equip.java` | six fields, ten methods, six lines in `copy()` | 3.5 |
| `client/Character.java` | nine fields, accessors, DB load and save, `syncWeaponTint()` | 3.6 |
| `client/inventory/ItemFactory.java` | six columns on the equip INSERT and read, three on the item INSERT and read | 3.7 |
| `client/inventory/Item.java` | three fields, accessors, three lines in `copy()` | 3.9 |
| `net/server/channel/handlers/PlayerLoggedinHandler.java` | one call | 3.8 |
| `server/maps/MapleMap.java` | one call | 3.8 |
| `client/Character.java` (again) | one call, in `equipChanged()` | 3.8 |

### 3.1 Drop in the two files

```
server/server/colorprism/ColorPrismPackets.java  ->  src/main/java/server/colorprism/
server/net/.../WeaponTintHandler.java            ->  src/main/java/net/server/channel/handlers/
```

They import only stock Cosmic types: `Character`, `Client`, `Equip`, `Inventory`,
`InventoryType`, `Item`, `InventoryManipulator`, `ItemId`, `AbstractPacketHandler`,
`InPacket`, `MapleMap`, `OutPacket`, `Packet`, `SendOpcode`.

> **The handler does not gate on `isCash`.** Ordinary equips dye exactly like Cash ones,
> and the client's drop gate is open to match. The two must move together: if only one
> side is tightened, a drop is accepted on the well and then refused a round trip later,
> which reads as the window being broken rather than as a rule. If you want the original
> Cash-only behaviour, add the check back in `resolve()` **and** narrow
> `IsDyeableEquip` in `coloringprism.cpp`.

### 3.2 The item constant

In `constants/id/ItemId.java`:

```java
public static final int COLORING_PRISM = 5782000;
```

### 3.3 Opcodes

In `net/opcodes/RecvOpcode.java`:

```java
WEAPON_TINT_ACTION(0x372E),
```

In `net/opcodes/SendOpcode.java`:

```java
WEAPON_TINT_SYNC(0x372F),
```

The one opcode carries seven actions, and the handler switches on the first byte:

| action | payload |
|---|---|
| 0 request | resend every tint this character owns |
| 1 apply | invType(1) invPos(2) itemId(4) hue(2) chroma(1) bright(1) prismPos(2) layer(1) |
| 2 restore | invType(1) invPos(2) itemId(4) prismPos(2) layer(1) |
| 3 applyLook | kind(1) hue(2) chroma(1) bright(1) prismPos(2) |
| 4 restoreLook | kind(1) prismPos(2) |
| 5 applySkill | skillId(4) hue(2) chroma(1) bright(1) prismPos(2) |
| 6 restoreSkill | skillId(4) prismPos(2) |

Actions 5 and 6 name their target by SKILL ID rather than by inventory address, because a skill
has none. The handler verifies the character actually knows the skill instead, which is the only
validation available for that target: every other action re-reads the item at the position the
client named, but a skill id is four bytes with nothing behind it.

### 3.4 Register the handler

In `net/PacketProcessor.java`, beside the other `registerHandler` calls:

```java
registerHandler(RecvOpcode.WEAPON_TINT_ACTION, new WeaponTintHandler());
```

### 3.5 `client/inventory/Equip.java`

Six fields:

```java
private short tintHue = 0;
private byte tintChroma = 0;
private byte tintBright = 0;
private short tintFxHue = 0;
private byte tintFxChroma = 0;
private byte tintFxBright = 0;
```

Ten methods. All zero is the identity transform and therefore also means "never dyed",
which is why there is no separate flag:

```java
public short getTintHue()    { return tintHue; }
public byte  getTintChroma() { return tintChroma; }
public byte  getTintBright() { return tintBright; }
public boolean isTinted()    { return tintHue != 0 || tintChroma != 0 || tintBright != 0; }

public void setTint(int hue, int chroma, int bright) {
    this.tintHue    = (short) normalizeTintHue(hue);
    this.tintChroma = (byte) Math.max(-100, Math.min(100, chroma));
    this.tintBright = (byte) Math.max(-100, Math.min(100, bright));
}

public void clearTint() { tintHue = 0; tintChroma = 0; tintBright = 0; }

public short getTintFxHue()    { return tintFxHue; }
public byte  getTintFxChroma() { return tintFxChroma; }
public byte  getTintFxBright() { return tintFxBright; }
public boolean isFxTinted()    { return tintFxHue != 0 || tintFxChroma != 0 || tintFxBright != 0; }

public void setFxTint(int hue, int chroma, int bright) {
    this.tintFxHue    = (short) normalizeTintHue(hue);
    this.tintFxChroma = (byte) Math.max(-100, Math.min(100, chroma));
    this.tintFxBright = (byte) Math.max(-100, Math.min(100, bright));
}

public void clearFxTint() { tintFxHue = 0; tintFxChroma = 0; tintFxBright = 0; }
```

The clamp is not decoration: it is what stops a hand-made packet storing a rotation the
client would then render as an arbitrary colour.

Each of these three classes needs the same small helper beside its setters:

```java
/**
 * The sign of a tint hue carries meaning: positive is a ROTATION of 1..359 degrees,
 * negative is an ABSOLUTE target encoded as -(degrees + 1), and zero means leave the hue
 * alone. A plain floorMod(hue, 360) would turn -1 into 359, silently converting
 * "absolute red" into "rotate by 359", so only the positive half wraps.
 */
protected static int normalizeTintHue(int hue) {
    if (hue < 0) {
        return hue < -360 ? -360 : hue;
    }
    return Math.floorMod(hue, 360);
}
```

**Copy all six in `copy()`.** Cosmic's `Equip.copy()` builds a new Equip field by field,
and anything you forget there is silently lost the first time the item is moved:

```java
ret.tintHue = tintHue;
ret.tintChroma = tintChroma;
ret.tintBright = tintBright;
ret.tintFxHue = tintFxHue;
ret.tintFxChroma = tintFxChroma;
ret.tintFxBright = tintFxBright;
```

### 3.6 `client/Character.java`

Nine fields, three per look tab:

```java
private short hairTintHue = 0;
private byte hairTintChroma = 0;
private byte hairTintBright = 0;
private short faceTintHue = 0;
private byte faceTintChroma = 0;
private byte faceTintBright = 0;
private short skinTintHue = 0;
private byte skinTintChroma = 0;
private byte skinTintBright = 0;
```

`skinTint*` is **not** `skincolor`. That column still chooses which of the client's fixed
skins the character wears; these three rotate the colour of whichever one is worn, so the
two coexist.

Accessors, clamped the same way:

```java
public short getHairTintHue()    { return hairTintHue; }
public byte  getHairTintChroma() { return hairTintChroma; }
public byte  getHairTintBright() { return hairTintBright; }
public short getFaceTintHue()    { return faceTintHue; }
public byte  getFaceTintChroma() { return faceTintChroma; }
public byte  getFaceTintBright() { return faceTintBright; }

public boolean isHairTinted() { return hairTintHue != 0 || hairTintChroma != 0 || hairTintBright != 0; }
public boolean isFaceTinted() { return faceTintHue != 0 || faceTintChroma != 0 || faceTintBright != 0; }

public void setHairTint(int hue, int chroma, int bright) {
    this.hairTintHue    = (short) normalizeTintHue(hue);
    this.hairTintChroma = (byte) Math.max(-100, Math.min(100, chroma));
    this.hairTintBright = (byte) Math.max(-100, Math.min(100, bright));
}

public void setFaceTint(int hue, int chroma, int bright) {
    this.faceTintHue    = (short) normalizeTintHue(hue);
    this.faceTintChroma = (byte) Math.max(-100, Math.min(100, chroma));
    this.faceTintBright = (byte) Math.max(-100, Math.min(100, bright));
}

public void clearHairTint() { hairTintHue = 0; hairTintChroma = 0; hairTintBright = 0; }
public void clearFaceTint() { faceTintHue = 0; faceTintChroma = 0; faceTintBright = 0; }

public short getSkinTintHue()    { return skinTintHue; }
public byte  getSkinTintChroma() { return skinTintChroma; }
public byte  getSkinTintBright() { return skinTintBright; }
public boolean isSkinTinted() { return skinTintHue != 0 || skinTintChroma != 0 || skinTintBright != 0; }

public void setSkinTint(int hue, int chroma, int bright) {
    this.skinTintHue    = (short) normalizeTintHue(hue);
    this.skinTintChroma = (byte) Math.max(-100, Math.min(100, chroma));
    this.skinTintBright = (byte) Math.max(-100, Math.min(100, bright));
}

public void clearSkinTint() { skinTintHue = 0; skinTintChroma = 0; skinTintBright = 0; }
```

**Load them** in `loadCharFromDB`, wherever the other `characters` columns are read:

```java
ret.hairTintHue    = (short) rs.getInt("hairtinthue");
ret.hairTintChroma = (byte) rs.getInt("hairtintchroma");
ret.hairTintBright = (byte) rs.getInt("hairtintbright");
ret.faceTintHue    = (short) rs.getInt("facetinthue");
ret.faceTintChroma = (byte) rs.getInt("facetintchroma");
ret.faceTintBright = (byte) rs.getInt("facetintbright");
ret.skinTintHue    = (short) rs.getInt("skintinthue");
ret.skinTintChroma = (byte) rs.getInt("skintintchroma");
ret.skinTintBright = (byte) rs.getInt("skintintbright");
```

**Save them** in `saveCharToDB`. A short separate UPDATE is the least invasive way in,
and it keeps you out of the big positional statement:

```java
try (PreparedStatement psTint = con.prepareStatement(
        "UPDATE characters SET hairtinthue = ?, hairtintchroma = ?, hairtintbright = ?, "
        + "facetinthue = ?, facetintchroma = ?, facetintbright = ?, "
        + "skintinthue = ?, skintintchroma = ?, skintintbright = ? WHERE id = ?")) {
    psTint.setInt(1, hairTintHue);
    psTint.setInt(2, hairTintChroma);
    psTint.setInt(3, hairTintBright);
    psTint.setInt(4, faceTintHue);
    psTint.setInt(5, faceTintChroma);
    psTint.setInt(6, faceTintBright);
    psTint.setInt(7, skinTintHue);
    psTint.setInt(8, skinTintChroma);
    psTint.setInt(9, skinTintBright);
    psTint.setInt(10, id);
    psTint.executeUpdate();
}
```

Put it **inside** the existing transaction, before the commit. `saveCharToDB` is one
transaction; a statement outside it can commit on its own while the rest rolls back.

**Add the sync helper**, which pushes tints to the client DLL:

```java
public void syncWeaponTint() {
    if (client != null) {
        sendPacket(server.colorprism.ColorPrismPackets.snapshot(this));
        server.colorprism.ColorPrismPackets.broadcastMapTable(getMap());
    }
}
```

The `client != null` guard matters: this gets called from equip paths that also run
during login and in the cash shop, where there is no channel client to send to.

**Skill tints need a map, not columns.** Hair, eyes and skin are three columns each because a
character has exactly one of each; an equip and a cash effect carry their colours on their own
inventory rows. A skill has no inventory row at all and a character may dye any number of them,
so this one lives in the `skilltints` table the changelog creates.

```java
private final Map<Integer, int[]> skillTints = new LinkedHashMap<>();
```

**Load it beside the skills themselves**, in the same method that reads the `skills` table:

```java
try (PreparedStatement ps = con.prepareStatement(
        "SELECT skillid, tinthue, tintchroma, tintbright FROM skilltints WHERE characterid = ?")) {
    ps.setInt(1, charid);
    try (ResultSet rs = ps.executeQuery()) {
        while (rs.next()) {
            ret.skillTints.put(rs.getInt("skillid"), new int[]{
                    rs.getInt("tinthue"), rs.getInt("tintchroma"), rs.getInt("tintbright")});
        }
    }
}
```

**Save it in the same transaction** as the look tints above. Delete and reinsert rather than
upsert: the map is the whole truth for this character, so replacing the set is simpler than
reconciling it, and a cleared tint is an absent key rather than a row of zeroes.

```java
try (PreparedStatement psDel = con.prepareStatement(
        "DELETE FROM skilltints WHERE characterid = ?")) {
    psDel.setInt(1, id);
    psDel.executeUpdate();
}
if (!skillTints.isEmpty()) {
    try (PreparedStatement ps = con.prepareStatement(
            "INSERT INTO skilltints (characterid, skillid, tinthue, tintchroma, tintbright) "
            + "VALUES (?, ?, ?, ?, ?)")) {
        for (Entry<Integer, int[]> e : skillTints.entrySet()) {
            int[] t = e.getValue();
            ps.setInt(1, id);
            ps.setInt(2, e.getKey());
            ps.setInt(3, t[0]);
            ps.setInt(4, t[1]);
            ps.setInt(5, t[2]);
            ps.addBatch();
        }
        ps.executeBatch();
    }
}
```

**Four accessors.** An identity tint REMOVES the entry rather than storing three zeroes, so
"never dyed" and "dyed back" are one state in the table as well as in the renderer:

```java
public Map<Integer, int[]> getSkillTints() { return Collections.unmodifiableMap(skillTints); }

public boolean isSkillTinted(int skillId) {
    int[] t = skillTints.get(skillId);
    return t != null && (t[0] != 0 || t[1] != 0 || t[2] != 0);
}

public void setSkillTint(int skillId, int hue, int chroma, int bright) {
    int h = normalizeTintHue(hue);
    int c = Math.max(-100, Math.min(100, chroma));
    int b = Math.max(-100, Math.min(100, bright));
    if (h == 0 && c == 0 && b == 0) skillTints.remove(skillId);
    else skillTints.put(skillId, new int[]{h, c, b});
}

public void clearSkillTint(int skillId) { skillTints.remove(skillId); }
```

`normalizeTintHue` is the same helper the look tints use; see the hue section of the README.

### 3.7 `client/inventory/ItemFactory.java`

Six columns on the equipment insert and the equipment read.

Find `INSERT_EQUIPMENT_SQL` and add the six names to the column list, plus six `?` to
the placeholder list:

```
`tinthue`, `tintchroma`, `tintbright`, `tintfxhue`, `tintfxchroma`, `tintfxbright`
```

Then bind them, continuing whatever index the list ended on:

```java
ps.setInt(N + 1, equip.getTintHue());
ps.setInt(N + 2, equip.getTintChroma());
ps.setInt(N + 3, equip.getTintBright());
ps.setInt(N + 4, equip.getTintFxHue());
ps.setInt(N + 5, equip.getTintFxChroma());
ps.setInt(N + 6, equip.getTintFxBright());
```

> **Cosmic prepares `INSERT_EQUIPMENT_SQL` in two places**, `saveItemsCommon` and
> `saveItemsMerchant`, and both bind it. Do both. Editing only the first is easy to miss
> because ordinary play never exercises the second, and the symptom when it finally fires
> is not a dye problem at all: the merchant save throws `Column count doesn't match value
> count` on its own connection, so the character save around it succeeds and the items in
> the merchant are the ones that quietly go missing. If your fork has already factored the
> binding into one shared helper, there is only one site and you are done.

> If your `INSERT_EQUIPMENT_SQL` still uses positional `VALUES` with no column list,
> convert it to a named list first. The column names, the placeholders and the `setInt`
> calls have to move together, and a positional statement makes that a trap every time
> anyone adds a column.

And in the method that builds an `Equip` from a `ResultSet`:

```java
equip.setTint(rs.getInt("tinthue"), rs.getInt("tintchroma"), rs.getInt("tintbright"));
equip.setFxTint(rs.getInt("tintfxhue"), rs.getInt("tintfxchroma"), rs.getInt("tintfxbright"));
```

Miss this one and dyeing appears to work perfectly until the player relogs.

**`INSERT_ITEM_SQL` needs the same treatment**, for cash effect items. Add three column
names and three placeholders:

```
`efftinthue`, `efftintchroma`, `efftintbright`
```

then bind them at **both** call sites (Cosmic writes this statement in two places), and
read them back wherever a plain `Item` is built from a `ResultSet`, also two places:

```java
item.setEffTint(rs.getInt("efftinthue"), rs.getInt("efftintchroma"),
        rs.getInt("efftintbright"));
```

Count the columns against the placeholders after editing. A mismatch there does not fail
at startup: it fails on the next inventory save, for every character, with
`Column count doesn't match value count`.

### 3.8 Three send sites

The DLL has nothing to recolour with until a snapshot arrives, so send one. Three
places, each given below by **method** and by a **stock line to search for**, since your
line numbers will not match anyone else's.

`syncWeaponTint()` guards itself with `if (client != null)` internally, so you can call
it bare at all three sites. That guard is not optional: two of these paths also run
during login and in the cash shop, where there is no channel client to send to.

**1. On login.** File `net/server/channel/handlers/PlayerLoggedinHandler.java`, method
`handlePacket(InPacket p, Client c)`. Search for:

```java
c.sendPacket(PacketCreator.enableReport());
```

and add the call directly after it:

```java
player.syncWeaponTint();
```

Anywhere after the character is on the map works; `enableReport()` is just a stable
landmark near the end of the login burst. Without this, a dyed weapon renders in its
original colour until the player's first equip change.

**2. On map entry.** File `server/maps/MapleMap.java`, method
`addPlayer(final Character chr)`. Search for:

```java
chr.receivePartyMemberHP();
announcePlayerDiseases(chr.getClient());
```

and add after them:

```java
server.colorprism.ColorPrismPackets.broadcastMapTable(this);
```

Note this one is `this`, the map, not the character, and it is a **broadcast** rather
than a send. One call covers both directions: the arriving player needs everyone else's
tints, and everyone already standing there needs the arrival's. Skip it and dyes are
invisible to other players until something else forces a resend.

**3. On equipment change.** File `client/Character.java`, method `equipChanged()`. In
stock Cosmic that method is short:

```java
public void equipChanged() {
    getMap().broadcastUpdateCharLookMessage(this, this);
    equipchanged = true;
    updateLocalStats();
    if (getMessenger() != null) {
        getWorldServer().updateMessenger(getMessenger(), getName(), getWorld(), client.getChannel());
    }
    syncWeaponTint();          // <- add this, at the end
}
```

Put it at the end, after `updateLocalStats()`. This is the one that makes swapping
between two dyed Cash weapons repaint immediately instead of on the next map change.

### 3.9 `client/inventory/Item.java`

These three fields are what **cash effect items** dye with, the `5010000..5019999` group
that sits in the Cash tab and plays an effect around the character.

> **Not optional.** Both files from 3.1 call these accessors unconditionally, so a tree
> without them does not compile: `ColorPrismPackets` reads them to build the snapshot and
> `WeaponTintHandler` writes them on apply. If you genuinely do not want cash effect items
> dyeable, cut the feature at the two `resolveCashEffect` branches in the handler rather
> than by skipping this section.

Three fields, not six. An equip carries two tints because its body art and its effect art
are separate and dye separately; a cash effect **is** the effect, so there is no body half
to keep apart:

```java
private short effTintHue = 0;
private byte effTintChroma = 0;
private byte effTintBright = 0;
```

Accessors, clamped exactly as `Equip.setTint` clamps:

```java
public short getEffTintHue()    { return effTintHue; }
public byte  getEffTintChroma() { return effTintChroma; }
public byte  getEffTintBright() { return effTintBright; }
public boolean isEffTinted() {
    return effTintHue != 0 || effTintChroma != 0 || effTintBright != 0;
}

public void setEffTint(int hue, int chroma, int bright) {
    this.effTintHue    = (short) normalizeTintHue(hue);
    this.effTintChroma = (byte) Math.max(-100, Math.min(100, chroma));
    this.effTintBright = (byte) Math.max(-100, Math.min(100, bright));
}

public void clearEffTint() { effTintHue = 0; effTintChroma = 0; effTintBright = 0; }
```

**And copy all three in `Item.copy()`**, the same trap as `Equip.copy()`:

```java
ret.effTintHue = effTintHue;
ret.effTintChroma = effTintChroma;
ret.effTintBright = effTintBright;
```

The bundled `WeaponTintHandler` and `ColorPrismPackets` already handle these: the handler
resolves a plain `Item` out of the CASH tab instead of an `Equip`, and the snapshot sends
the colour under the item's EFFECT key (`itemId + 10000000`), which is the same key an
equip's glow uses. The client's tint table needs no new concept for them.

---

## 4. Client DLL

### 4.1 Add the files

Add `client/coloringprism.cpp` and `client/weapontint.cpp` to your build, and
`client/coloringprism.h` and `client/weapontint.h` to your include path.

The two headers are what your own files include to reach this feature. Between them they
declare every function the steps below tell you to call, so include the matching header at
each of those call sites rather than hand-writing an `extern`: a hand-written declaration
that disagrees with the definition is a link error at best and a silent stack mismatch at
worst.

### 4.2 What your DLL must provide

Both files include `pch.h`, `hook.h`, `debug.h`, and the WZ and ZTL wrapper headers
(`wvs/avatar.h`, `wvs/iteminfo.h`, `wvs/packet.h`, `wvs/util.h`, `wvs/wnd.h`,
`wvs/wndman.h`, `ztl/ztl.h`).

**This section is verified, not guessed.** Both files were compiled against a clean
checkout of iw2d/kaentake **v3.4.1**, the errors collected, the three fixes below
applied, and both then compile clean. If you are on that base, this is the complete
list; nothing else is missing.

**What v3.4.1 already has**, so you can stop looking for it: `get_rm()`,
`get_unknown()`, `get_screen_width()`, `get_screen_height()`, `PcCreateObject<>`,
`ATTACH_HOOK`, `CWnd`, `ZRefCounted`, `CWndMan`, `CItemInfo`, `IWzCanvas`, `IWzProperty`,
`IWzUOL`, `Ztl_bstr_t`, `ZXString<>`, and `COutPacket` with `Encode1` / `Encode2` /
`Encode4`. `IWzUOL` comes in through `ztl/ztl.h` -> `ztl/zcom.h`, which includes
`WzLib/IWzUOL.h` in stock; if your `WzLib` submodule is unpopulated nothing here compiles
anyway.

Four things are missing. Every raw client address in the two files is byte-verified
against the v83 client with image base `0x400000`; on a different client build they are
all wrong.

#### 4.2a `debug.h`: the logging macros

v3.4.1 has `DebugMessage` and `ErrorMessage` but not these. Nine `LOG_ONCE`, two
`LOG_ONCE_PER_ID` and one `LogMessage` call site depend on them:

```cpp
void LogMessage(const char* sFormat, ...);   // append a line to a log file

#define LOG_ONCE(FORMAT, ...)                       \
    do {                                            \
        static bool bLogged__ = false;              \
        if (!bLogged__) {                           \
            bLogged__ = true;                       \
            LogMessage(FORMAT, __VA_ARGS__);        \
        }                                           \
    } while (0)

constexpr int kLogKeyCap = 64;
#define LOG_ONCE_PER_ID(KEY, FORMAT, ...)                                       \
    do {                                                                        \
        static int  aKeys__[kLogKeyCap] = {};                                   \
        static int  nKeys__ = 0;                                                \
        static bool bCapped__ = false;                                          \
        const int   nKey__ = (KEY);                                             \
        bool bSeen__ = false;                                                   \
        for (int i__ = 0; i__ < nKeys__; ++i__) {                               \
            if (aKeys__[i__] == nKey__) { bSeen__ = true; break; }              \
        }                                                                       \
        if (!bSeen__) {                                                         \
            if (nKeys__ < kLogKeyCap) {                                         \
                aKeys__[nKeys__++] = nKey__;                                    \
                LogMessage(FORMAT, __VA_ARGS__);                                \
            } else if (!bCapped__) {                                            \
                bCapped__ = true;                                               \
                LogMessage("    (%d distinct ids logged here; rest suppressed)",\
                           kLogKeyCap);                                         \
            }                                                                   \
        }                                                                       \
    } while (0)
```

`LOG_ONCE` fires once per call site, `LOG_ONCE_PER_ID` once per distinct id per site,
which is what tells you the whole SET of offending items rather than just the first.

**None of it is load-bearing.** Every call is a diagnostic on a path that already
handles the failure, and every argument is a plain value with no side effects, so if you
would rather not add a logger these compile away safely:

```cpp
inline void LogMessage(const char*, ...) {}
#define LOG_ONCE(FORMAT, ...)             ((void)0)
#define LOG_ONCE_PER_ID(KEY, FORMAT, ...) ((void)0)
```

You lose the diagnostics in section 6, which is where most of the value is when
something does not work.

#### 4.2b `wvs/packet.h`: five public `CInPacket` members

v3.4.1's `CInPacket` declares the fields but exposes nothing. These five need to be
public; none of them needs a client address, because they only read fields the class
already has. Add them to the class, after the existing `protected:` block:

```cpp
public:
    size_t GetOffset() const { return m_uOffset; }
    void   SetOffset(size_t offset) { m_uOffset = offset; }

    bool CanRead(size_t n) const {
        return m_uOffset + n <= static_cast<size_t>(m_aRecvBuff.GetCount());
    }

    // Raw read at the current offset, then advance. Guard every call with CanRead.
    template <typename T>
    T Decode() {
        static_assert(sizeof(T) <= 8, "Decode<T> only supports up to 8 bytes");
        if (!CanRead(sizeof(T))) return T{};
        T value = *reinterpret_cast<const T*>(&m_aRecvBuff[m_uOffset]);
        m_uOffset += sizeof(T);
        return value;
    }

    // Pointer to the current offset, for the tint table's name strings.
    // UNCHECKED: call CanRead first.
    const uint8_t* CurrentPublic() const { return &m_aRecvBuff[m_uOffset]; }
```

The field layout is what makes them work, and v3.4.1 already has it right:
`m_aRecvBuff` at `+0x08`, `m_uLength` at `+0x0C`, `m_uOffset` at `+0x14`, with
`static_assert(sizeof(CInPacket) == 0x18)`. Keep that assert.

Call counts, so you can check you got them all: `CanRead` 7, `Decode<uint8_t>` 7,
`Decode<uint16_t>` 2, `Decode<uint32_t>` 1, `GetOffset` 2, `SetOffset` 2,
`CurrentPublic` 1.

> **A caveat on `CanRead`.** It bounds against `GetCount()`, the buffer's allocated
> size, not `m_uLength`, the packet's declared length. The allocation is the looser of
> the two, so on a truncated packet `CanRead` can pass and `Decode` hands back stale
> bytes from earlier traffic rather than failing. It will not read outside the
> allocation and it will not crash. For the stricter behaviour use
> `m_uOffset + n <= m_uLength`.

#### 4.2c `wvs/avatar.h`: the equip arrays are `[52]`, not `[60]`

**Read this one before you make it compile.** v3.4.1 declares:

```cpp
int anHairEquip[60];
int anUnseenEquip[60];
static_assert(sizeof(AvatarLook) == 0x205);
```

Those numbers are wrong, and the `static_assert` passes anyway because the wrong count
and the wrong size are consistent with each other. A passing size assert proves internal
consistency, not correctness.

The real layout, byte-verified: `AvatarLook::operator=` at `0x00451541` copies the tail
as three explicit loops, `push 0x34` (= 52) with `lea edx,[eax+0x19]`, then `push 0x34`
with `lea edx,[eax+0xe9]`, then `push 3` with `lea edx,[eax+0x1b9]`. That lands the
struct end at `0x1C5`, corroborated at `0x004515D5` by `push 0x1c5`, the memcmp length
between the two AvatarLooks at `CAvatar+4` and `CAvatar+0x1C9`.

```cpp
#define AVATAR_EQUIP_SLOTS 52

#pragma pack(push, 1)
struct AvatarLook : public ZRefCounted {
    unsigned char nGender;                      // +0x0C
    int nSkin;                                  // +0x0D
    int nFace;                                  // +0x11
    int nWeaponStickerID;                       // +0x15
    int anHairEquip[AVATAR_EQUIP_SLOTS];        // +0x19
    int anUnseenEquip[AVATAR_EQUIP_SLOTS];      // +0xE9
    int anPetID[3];                             // +0x1B9
};
#pragma pack(pop)
static_assert(sizeof(AvatarLook) == 0x1C5);
```

Change `CAvatar::CustomData::aItemEffectLayer[60]` to `[AVATAR_EQUIP_SLOTS]` in the same
file so the two cannot drift apart again.

> **Do not just `#define AVATAR_EQUIP_SLOTS 60` to clear the error.** It compiles, and
> then every loop over `anHairEquip` runs eight iterations into `anUnseenEquip[0..7]`,
> so equips hidden underneath cash items get treated as worn. Nothing crashes and
> nothing reads out of bounds, because the overrun lands inside the same struct, so it
> presents as a cosmetic oddity rather than as a bug. If your DLL already renders item
> effects, it has this bug today.

Everything at or before `anHairEquip` is at the correct offset under either declaration,
so the window's `kAL_*` constants and `nWeaponStickerID` are unaffected either way.

#### 4.2d `wvs/wndman.h`: a missing include

Stock `wndman.h` uses `CONSTANTS_CENTER_STATUSBAR` without including the header that defines it.
Nothing in stock includes `wndman.h`, so the bug is latent there; `coloringprism.cpp` does
include it, which is why it surfaces here and not upstream. Add at the top of that file:

```cpp
#include "constants.h"
```

A one-line fix to a file you already have, not a change to anything this bundle ships.

### 4.3 Attach at startup

Wherever you install your hooks:

```cpp
AttachWeaponTintMod();      // weapontint.cpp: four Detours, listed below
AttachColoringPrismMod();   // coloringprism.cpp: no hooks at all
```

`AttachWeaponTintMod()` claims **four** addresses, and the one-owner rule in 4.6 and 4.7
applies to every one of them. If your DLL already Detours any of these, dispatch from
whatever hooked it first instead of adding a second Detour:

| address | what it is | what it dyes |
|---|---|---|
| `0x00453AD1` | `CAvatar::PrepareActionLayer` | the body: equips, hair, skin, effect art |
| `0x00453696` | `CAvatar`'s face layer builder | the Eyes tab (see 4.3a) |
| `0x0093BEB9` | cash effect show | cash effect items (see 4.8) |
| `0x0093C218` | cash effect apply | cash effect items (see 4.8) |

### 4.3a The face builder, `0x00453696`

**The face is not built inside `PrepareActionLayer`.** It has its own builder, reached
from `CAvatar::SetEmotion` (`0x00451D82`), which is why the Eyes tab needs a hook of its
own: by the time anything reads a face canvas, the swap window around `PrepareActionLayer`
has already closed, so the tinted clones were built and thrown away. It is a plain
`__thiscall void(int)` ending `ret 4` at `0x00453A26`, with the `CAvatar` in `ecx`.

Hooking it also buys the refresh for free. The builder runs on every emotion change and
every blink, so a face tint appears within a second or two even with no explicit repaint.

Nothing to do here unless your DLL already hooks that address, which any blink, emotion or
eye-colour feature plausibly does. A collision is silent: no crash, no log line, and the
only symptom is that the Eyes tab does nothing while every other tab works.

### 4.4 Once per frame

From your per-frame update hook (a `CWvsApp::CallUpdate` Detour or equivalent):

```cpp
WeaponTint_Tick();
```

This is what makes a server-driven colour change visible. Confirm lands on the receive
thread, which can only raise a flag; without the tick the weapon keeps its old colour
until something else happens to rebuild its layers.

### 4.5 Route the inbound opcode

In your packet dispatcher, for opcode `0x372F`:

```cpp
WeaponTint_HandleSync(iPacket);
```

The handler reads the opcode from the packet's **current** offset, not from zero, so
pass the packet without rewinding it.

### 4.6 Dispatch the double-click

Three addresses, and **they are not interchangeable.** Two are send paths that you
intercept and swallow; the third is a gate that has to *answer*, and it is the one people
miss. Hook all three, in one file, and **do not add a second Detour on any of them** -- if
something in your DLL already owns them, dispatch from there instead.

**The gate: `0x004863D5`**, `int __cdecl get_consume_cash_item_type(int nItemID)`, a bare
`C3` epilogue. The client asks this what kind of cash item an id is, and its stock answer
only covers groups `500..561`. **Group 578 is outside that window**, so a stock client
drops a prism double-click before any of the send paths run and no packet ever exists.
Return nonzero for the prism and the click routes normally:

```cpp
int32_t __cdecl get_consume_cash_item_type_hook(int32_t nItemID) {
    if (ColorPrism_IsPrismItem(nItemID)) return 1;
    return get_consume_cash_item_type_orig(nItemID);
}
```

> This one answers on **every** call, not just on double-click: the address has twelve
> call sites in the client, covering tooltips, icons and drop/move legality. That is the
> accepted cost of getting group 578 through the gate, and it is why the test is an exact
> id match rather than a range.

**The two send paths**, which some v83 items route through one of and some the other, so
both are needed: `0x00A0A63F` `SendConsumeCashItemUseRequest`,
`__thiscall(void*, int nPOS, int nItemID, int, ZXString<char>)`, and `0x00A1DC5B`
`SendEtcCashItemUseRequest`, `__thiscall(void*, int nPOS, int nItemID)`, `ret 8`. In each,
before calling the original:

```cpp
if (ColorPrism_IsPrismItem(nItemID)) {
    ColorPrism_OnUse(nPOS, nItemID);
    return;          // swallow it: the item is consumed by the server, not here
}
```

If the window never opens on double-click, the gate is the first thing to check: with the
two send hooks in and the gate missing, nothing happens at all and nothing logs.

### 4.7 Dispatch the drop

The window picks its target from a drag and drop onto the well. From your
`CDraggableItem::OnDropped` hook (`0x004EF140`), same no-second-Detour rule.

**Two of the three arguments are not parameters of that hook.** Its shape is

```cpp
int __thiscall CDraggableItem::OnDropped(void* pThis, void* pFrom, void* pTo, int rx, int ry)
```

so `pTo` you have, but the inventory type and position describe **where the dragged item
came from**, and those live on the `CDraggableItem` itself. Read them off `pThis`:

```cpp
// The drag source tagged on a CDraggableItem: the originating handler at +0x24, and the
// (inventory type, position) pair at +0x18 / +0x1C.
void* srcHandler = *(void**)((char*)pThis + 0x24);
int   invType    = *(int*)  ((char*)pThis + 0x18);
int   invPos     = *(int*)  ((char*)pThis + 0x1C);

if (ColorPrism_HandleItemDrop(pTo, invType, invPos)) {
    return 1;        // consumed either way: the item must not also be moved
}
```

Do those three reads inside a `__try` block of their own (a leaf function, since `__try`
may not share a function with anything that unwinds) and fall through to the original hook
if it faults. `pThis` is not guaranteed non-null on every path into this hook.

> **`srcHandler` matters as much as the pair.** Any window that implements its own drag
> puts its own meaning in those same two fields -- a custom bag puts `(bagKind, bagSlot)`
> there -- so if you have such a window, compare `srcHandler` against it and skip the
> prism when they match. Reading `+0x18` / `+0x1C` without checking who wrote them
> silently reinterprets, say, a bag slot as an inventory position, and the prism then
> dyes whatever item happens to be at that position in the real inventory.

`pTo` **may legitimately be null** and must still be passed through. The window accepts a
drop addressed to it (`pTo == window` or `window + 4`, since `CWnd` inherits
`IUIMsgHandler` at +4) *or*, when the engine latched onto nothing, a drop whose cursor is
over its 32x32 well. Filtering out null `pTo` before the call removes that fallback and
makes the well feel intermittent.

### 4.8 Cash effect items: two more Detours

The last two of the four addresses in 4.3, both for the `5010000..5019999` group:

```cpp
0x0093BEB9   __thiscall(CUser*, int nItemID)
0x0093C218   __thiscall(CUser*, int nItemID)
```

Nothing to do unless your DLL already Detours either address, in which case the same
one-owner rule applies as everywhere else here: dispatch from whatever hooked it first
rather than adding a second Detour.

Both were found by scanning for the `__SEH_prolog4` entry (`mov eax, <scopetable>;
call 0x00A60B98`) that this region of the client uses instead of `push ebp; mov ebp, esp`,
which is why an ordinary prologue scan does not find them at all. Both end `ret 4` and take
the item id as their only argument, which is what the swap keys on.

A third function, `0x0093B6A7`, also reaches this art, and is deliberately **not** hooked:
it calls both of the above and additionally serves the `Effect/ItemEff.img` path, so
hooking it too would swap the same subtree twice.

### 4.9 Optional: item-effect layers

If your DLL renders cape auras or similar from `Effect/ItemEff.img` as their own
`IWzGr2DLayer`, that art is outside `CAvatar::PrepareActionLayer` and the main hook
cannot see it. Wrap your layer build to dye it with the same Effects tab:

```cpp
WeaponTint_BeginItemEffSwap(pAvatar, nItemID);
const bool ok = pUser->LoadLayer(sUOL, bFlip, layer, nullptr);
WeaponTint_EndItemEffSwap();
```

Always call `End` if you called `Begin`, even when it returned false. The pair is not
reentrant.

One catch if you do this: such layers usually cache on item, action and facing, and a
recolour changes none of the three. Invalidate that cache when a tint changes, or the
new colour will not appear until the player turns around.

---

## 5. Smoke test

1. Buy a Coloring Prism. It should have a name and an icon, and it should land in the
   **Cash** tab. If the name is "null" or blank, step 2 did not reach `String.wz`; if it
   lands in Etc, the `cash` flag from step 2 is missing.
2. Double-click it. The window opens with your character walking in the preview; arrow
   keys move it, jump and attack work off your own key bindings. **If nothing happens at
   all, the gate in 4.6 is missing** -- that is the single most common failure.
3. Drag any equipped item onto the well, cash or not. Its icon appears there.
4. Drag the HUE slider. The preview recolours as you drag.
5. Press OK. The prism is consumed and the item keeps its colour in the field.
6. Relog. The colour is still there. If it is not, step 3.7 is incomplete.
7. Have a second character stand next to you. They should see the colour too.

Then walk the tabs that use a different code path, because each one fails on its own:

8. **Hair**, then **Skin**. Both need no drop. If these do nothing while Equip works,
   check the nine `characters` columns in 3.6.
9. **Eyes**. This one is built by a separate hook; if it alone does nothing, see 4.3a.
10. **Effects**, on a cash weapon with a visible glow. Only about 4.45% of equips have
    effect art, so pick the target deliberately.
11. A **cash effect item** (`5010000..5019999`), equipped, dropped on the well. This uses
    the `Item.java` columns from 3.9 and the two Detours from 4.8, and it is the only test
    that covers either.

---

## 6. When it silently does nothing

Every failure mode here is quiet. There is no crash and usually no log line.

**The window opens but the weapon never recolours.** The tint is applied by swapping
canvases during one call to `CAvatar::PrepareActionLayer`. If a second Detour on that
address exists anywhere in your DLL, whichever installed last wins and the swap never
runs. `weapontint.cpp` must own that address outright.

**Dyeing works until you relog.** The `ItemFactory` read in step 3.7 is missing. The
write succeeds, so the columns fill up correctly in the database and are then ignored.

**One body part keeps an old colour while the rest updates.** A walk restored the wrong
thing over a WZ alias. A UOL child stands in for a node stored elsewhere and resolves
transparently, so a walk that restores the *resolved canvas* replaces the indirection with a
hard reference; and if the target was already swapped this pass, the alias resolves to the
module's own clone, which then gets tinted twice and recorded as the "original". Both walks
in `weapontint.cpp` record the **raw slot contents** (`RawChildObject`) and refuse to swap a
canvas the module cloned (`IsOurClone`). Any art path you add has to keep both rules.

**A single pose, or a few frames of one animation, stay vanilla.** The opposite mistake:
something *skipped* aliases instead of restoring them properly. A walk is scoped to the
action being built, and a lot of art is reachable only through an alias pointing at a
different action, which the walk never visits: `prone/0/hair` into `proneStab/0/hair` on
1988 of 17483 hair styles, `prone/0/body` into `proneStab/0/body` on all 46 skin imgs,
`heal/0/weapon` into `alert/1/weapon` and `swingO2` on two-handed weapons. Skip those and
the frame renders vanilla while the rest of the pose recolours. Hair has a third case that
no alias handling covers at all: 2806 styles store a separate artist-drawn lying-down canvas
inline under `prone/0`, which is why the hair walk visits the current action node and not
just `default` / `backDefault`.

**Dyeing works until the item is moved.** `Equip.copy()` is not copying the six fields.

**Every dye is refused with "You don't have a Coloring Prism."** The handler looks for the
prism in the **Cash** inventory. If the server-side `Item.wz` entry from step 2 is missing
its `cash` flag, the item lands in Etc or Use instead and the search never finds it.

**Hair, Eyes and Skin tabs do nothing, Equip and Effects are fine.** The nine `characters`
columns are not being loaded or saved. Check step 3.6.

**The Eyes tab alone does nothing.** The face has its own builder, `0x00453696`, hooked
separately from `PrepareActionLayer`. Either that hook is missing or something else in
your DLL Detoured the same address and won last. See 4.3a.

**Nothing happens on double-click at all.** The gate in 4.6, `0x004863D5`, is not hooked
or is not returning nonzero for 5782000. Group 578 is outside the `500..561` range the
client's own answer covers, so the click is dropped before either send path runs and no
packet is ever built. Hooking only the two send functions is the usual way to end up here.

**The flame chip appears to do nothing on most items.** It probably is doing nothing,
correctly: only about 4.45% of equips have effect art at all, and 870 of those 923 are
cash weapons. The chip greys itself on the rest. Test it on a cash weapon with a visible glow.

**A multi-projectile skill only dyes its first shots.** This one the bundle cannot fix for you.

A skill's art is swapped into the WZ tree for a window around the cast, because projectiles do
not resolve their sprites during `CUser::ShowSkillEffect` -- `CAnimationDisplayer::CreateBullet`
(`0x00435DF3`) only QUEUES a request, and each bullet resolves its art on a later 30ms tick, at
its own launch. A volley therefore resolves over time, and any arrow that launches after the
window closes takes the original art.

`weapontint.cpp` exports the fix but cannot call it:

```cpp
void WeaponTint_NoteBulletFlight(void* psBallUol, int nBulletItemId, int tArriveFrameTime);
```

Detour `CreateBullet` and call it before the original runs, passing arg9 (`psBallUol`), arg11
(`nBulletItemId`) and arg2 (`tArrive`). It extends the hold to that bullet's arrival, which is
absolute on the client frame clock at `0x00987257` and in milliseconds. Call it for every bullet;
it gates itself on the UOL being present and on a swap actually being held.

It is not wired up here because a Detour has exactly one owner, and whichever of your features
owns `0x00435DF3` should make the call rather than have this bundle fight it for the address.

**Everything but the first two arrows stays vanilla even with that wired up.** Then the swap is
being evicted rather than expiring. Held swaps are keyed per SKILL for exactly this reason: a bow
procs Final Attack (3100001) within half a second of a Strafe volley, and a single shared swap
list means installing Final Attack's tint tears out Strafe's while its arrows are still
resolving. If you have flattened `g_heldSkillSwaps` back to one list, that is the cause.
