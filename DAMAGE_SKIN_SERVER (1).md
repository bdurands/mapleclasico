# Damage Skin — Server Integration Tutorial

For the Cosmic v83 server. Follow this top to bottom —
each step says the exact file, whether to **CREATE** or **EDIT**, and
shows the full code / snippet to paste.

---

## STEP 1 — CREATE `src/main/resources/db/tables/026-damage-skin.sql`

```sql
-- Damage Skin feature: adds per-character active skin, owned catalog, and
-- the shop price list. Skin ID 0 is the "default/no skin" and is implicitly
-- owned by every character (no inventory row needed); the client renders
-- the stock digits when active = 0.

-- Per-character active skin id (0 = default).
ALTER TABLE characters
    ADD COLUMN activeDamageSkin INT NOT NULL DEFAULT '0';

-- Shop catalog: skinId -> price in mesos.
CREATE TABLE damageskin_catalog
(
    skinId     INT         NOT NULL,
    priceMesos BIGINT      NOT NULL DEFAULT '10000000',
    PRIMARY KEY (skinId)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

-- Per-character owned skins.
CREATE TABLE damageskin_inventory
(
    id          INT       NOT NULL AUTO_INCREMENT,
    characterId INT       NOT NULL,
    skinId      INT       NOT NULL,
    acquiredAt  TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uq_char_skin (characterId, skinId),
    KEY idx_char (characterId),
    CONSTRAINT fk_damageskin_char
        FOREIGN KEY (characterId) REFERENCES characters (id)
        ON DELETE CASCADE
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;
```

---

## STEP 2 — Apply the schema (pick ONE of the two options below)

### Option A — automatic, via Liquibase (recommended)

EDIT `src/main/resources/db/changelog-tables.xml`. Find the last
`<changeSet>` (probably `id="24"`) and add this one just before the
closing `</databaseChangeLog>`:

```xml
    <changeSet id="26" author="Kaentake">
        <sqlFile path="db/tables/026-damage-skin.sql"/>
    </changeSet>

</databaseChangeLog>
```

Liquibase will apply the migration the next time the server starts.
**Nothing else to do** — skip to STEP 3.

### Option B — run the SQL manually

If you'd rather not touch Liquibase (or you want to control exactly when
the schema hits the DB), you can **skip STEP 1 and STEP 2 entirely** and
just run this SQL against your `cosmic` database once. Paste it into
MySQL Workbench, HeidiSQL, phpMyAdmin, or the CLI:

```sql
-- === Damage Skin one-shot schema === --
ALTER TABLE characters
    ADD COLUMN activeDamageSkin INT NOT NULL DEFAULT '0';

CREATE TABLE damageskin_catalog
(
    skinId     INT    NOT NULL,
    priceMesos BIGINT NOT NULL DEFAULT '10000000',
    PRIMARY KEY (skinId)
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

CREATE TABLE damageskin_inventory
(
    id          INT       NOT NULL AUTO_INCREMENT,
    characterId INT       NOT NULL,
    skinId      INT       NOT NULL,
    acquiredAt  TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (id),
    UNIQUE KEY uq_char_skin (characterId, skinId),
    KEY idx_char (characterId),
    CONSTRAINT fk_damageskin_char
        FOREIGN KEY (characterId) REFERENCES characters (id)
        ON DELETE CASCADE
) ENGINE = InnoDB DEFAULT CHARSET = utf8mb4;

-- CLI form, if you prefer:
-- mysql -u root -p cosmic < damage-skin.sql
```

Verify it ran:
```sql
SHOW TABLES LIKE 'damageskin%';                  -- expect two rows
SHOW COLUMNS FROM characters LIKE 'activeDamageSkin'; -- expect one row
```

After that, continue with STEP 3.

---

## STEP 3 — CREATE `src/main/java/client/DamageSkinInventory.java`

```java
/*
    Damage Skin Inventory — per-character set of owned skin IDs.
*/
package client;

import tools.DatabaseConnection;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.Collections;
import java.util.HashSet;
import java.util.Set;
import java.util.TreeSet;
import java.util.concurrent.locks.Lock;
import java.util.concurrent.locks.ReentrantLock;

public final class DamageSkinInventory {

    public static final int DEFAULT_SKIN_ID = 0;

    private final Set<Integer> owned = new HashSet<>();
    private final Lock lock = new ReentrantLock();

    public boolean ownsSkin(int skinId) {
        if (skinId == DEFAULT_SKIN_ID) return true;
        lock.lock();
        try {
            return owned.contains(skinId);
        } finally {
            lock.unlock();
        }
    }

    public Set<Integer> getOwnedIds() {
        lock.lock();
        try {
            TreeSet<Integer> snapshot = new TreeSet<>();
            snapshot.add(DEFAULT_SKIN_ID);
            snapshot.addAll(owned);
            return Collections.unmodifiableSet(snapshot);
        } finally {
            lock.unlock();
        }
    }

    public boolean addSkin(int characterId, int skinId) throws SQLException {
        if (skinId == DEFAULT_SKIN_ID) return false;

        lock.lock();
        try {
            if (!owned.add(skinId)) return false;
        } finally {
            lock.unlock();
        }

        try (Connection con = DatabaseConnection.getConnection();
             PreparedStatement ps = con.prepareStatement(
                     "INSERT IGNORE INTO damageskin_inventory (characterId, skinId) VALUES (?, ?)")) {
            ps.setInt(1, characterId);
            ps.setInt(2, skinId);
            ps.executeUpdate();
        }
        return true;
    }

    public void loadSkins(int characterId) throws SQLException {
        lock.lock();
        try {
            owned.clear();
            try (Connection con = DatabaseConnection.getConnection();
                 PreparedStatement ps = con.prepareStatement(
                         "SELECT skinId FROM damageskin_inventory WHERE characterId = ?")) {
                ps.setInt(1, characterId);
                try (ResultSet rs = ps.executeQuery()) {
                    while (rs.next()) {
                        owned.add(rs.getInt("skinId"));
                    }
                }
            }
        } finally {
            lock.unlock();
        }
    }

    public void saveSkins(Connection con, int characterId) throws SQLException {
        // no-op: purchases are written eagerly in addSkin
    }
}
```

---

## STEP 4 — CREATE `src/main/java/client/DamageSkinCatalog.java`

```java
/*
    Damage Skin Catalog — scans server's WZ at boot and seeds the catalog.
*/
package client;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import provider.Data;
import provider.DataProvider;
import provider.DataProviderFactory;
import provider.wz.WZFiles;
import tools.DatabaseConnection;

import java.sql.Connection;
import java.sql.PreparedStatement;
import java.sql.ResultSet;
import java.sql.SQLException;
import java.util.Collections;
import java.util.Map;
import java.util.TreeMap;

public final class DamageSkinCatalog {
    private static final Logger log = LoggerFactory.getLogger(DamageSkinCatalog.class);

    public static final long DEFAULT_PRICE_MESOS = 10_000_000L;

    private static final String[] REQUIRED_CHILDREN = {
            "NoRed0", "NoRed1", "NoCri0", "NoCri1"
    };

    private static final TreeMap<Integer, Long> prices = new TreeMap<>();
    private static boolean loaded = false;

    public static synchronized void loadOrSeed() {
        if (loaded) return;
        try {
            int imported = importFromWz();
            loadFromDb();
            loaded = true;
            log.info("DamageSkinCatalog: {} new rows from WZ, {} total in catalog",
                     imported, prices.size());
        } catch (SQLException e) {
            log.error("DamageSkinCatalog load failed", e);
        } catch (Exception e) {
            log.error("DamageSkinCatalog WZ scan failed", e);
        }
    }

    private static int importFromWz() throws SQLException {
        DataProvider dp = DataProviderFactory.getDataProvider(WZFiles.EFFECT);
        if (dp == null) return 0;
        Data root = dp.getData("BasicEff.img");
        if (root == null) return 0;
        Data node = root.getChildByPath("damageSkin");
        if (node == null) return 0;

        int inserted = 0;
        try (Connection con = DatabaseConnection.getConnection();
             PreparedStatement ps = con.prepareStatement(
                     "INSERT IGNORE INTO damageskin_catalog (skinId, priceMesos) VALUES (?, ?)")) {
            for (Data skin : node.getChildren()) {
                int id;
                try {
                    id = Integer.parseInt(skin.getName());
                } catch (NumberFormatException nfe) { continue; }
                if (id <= 0) continue;

                boolean complete = true;
                for (String child : REQUIRED_CHILDREN) {
                    if (skin.getChildByPath(child) == null) {
                        complete = false;
                        break;
                    }
                }
                if (!complete) continue;

                ps.setInt(1, id);
                ps.setLong(2, DEFAULT_PRICE_MESOS);
                int rows = ps.executeUpdate();
                if (rows > 0) inserted++;
            }
        }
        return inserted;
    }

    private static void loadFromDb() throws SQLException {
        prices.clear();
        try (Connection con = DatabaseConnection.getConnection();
             PreparedStatement ps = con.prepareStatement(
                     "SELECT skinId, priceMesos FROM damageskin_catalog ORDER BY skinId");
             ResultSet rs = ps.executeQuery()) {
            while (rs.next()) {
                int id = rs.getInt("skinId");
                long price = rs.getLong("priceMesos");
                if (id != DamageSkinInventory.DEFAULT_SKIN_ID) {
                    prices.put(id, price);
                }
            }
        }
    }

    public static long getPrice(int skinId) {
        if (skinId == DamageSkinInventory.DEFAULT_SKIN_ID) return -1L;
        Long p = prices.get(skinId);
        return p == null ? -1L : p;
    }

    public static boolean contains(int skinId) {
        return skinId != DamageSkinInventory.DEFAULT_SKIN_ID && prices.containsKey(skinId);
    }

    public static Map<Integer, Long> getAll() {
        return Collections.unmodifiableMap(prices);
    }
}
```

---

## STEP 5 — CREATE `src/main/java/net/server/channel/handlers/DamageSkinApplyHandler.java`

```java
package net.server.channel.handlers;

import client.Character;
import client.Client;
import client.DamageSkinInventory;
import net.AbstractPacketHandler;
import net.packet.InPacket;
import tools.PacketCreator;

public final class DamageSkinApplyHandler extends AbstractPacketHandler {

    private static final int OP_APPLY = 1;

    @Override
    public void handlePacket(InPacket p, Client c) {
        int skinId = p.readInt();
        Character chr = c.getPlayer();
        if (chr == null) return;

        DamageSkinInventory inv = chr.getDamageSkinInventory();
        if (!inv.ownsSkin(skinId)) {
            c.sendPacket(PacketCreator.damageSkinResult(OP_APPLY, false, skinId, chr.getMeso()));
            return;
        }

        chr.setActiveDamageSkin(skinId);
        c.sendPacket(PacketCreator.damageSkinResult(OP_APPLY, true, skinId, chr.getMeso()));

        if (chr.getMap() != null) {
            chr.getMap().broadcastMessage(PacketCreator.damageSkinBroadcast(chr.getId(), skinId));
        }
    }
}
```

---

## STEP 6 — CREATE `src/main/java/net/server/channel/handlers/DamageSkinPurchaseHandler.java`

```java
package net.server.channel.handlers;

import client.Character;
import client.Client;
import client.DamageSkinCatalog;
import client.DamageSkinInventory;
import net.AbstractPacketHandler;
import net.packet.InPacket;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import tools.PacketCreator;

public final class DamageSkinPurchaseHandler extends AbstractPacketHandler {
    private static final Logger log = LoggerFactory.getLogger(DamageSkinPurchaseHandler.class);

    private static final int OP_PURCHASE = 2;

    @Override
    public void handlePacket(InPacket p, Client c) {
        int skinId = p.readInt();
        Character chr = c.getPlayer();
        if (chr == null) return;

        final int curMesos = chr.getMeso();

        if (skinId == DamageSkinInventory.DEFAULT_SKIN_ID) {
            c.sendPacket(PacketCreator.damageSkinResult(OP_PURCHASE, false, skinId, curMesos));
            return;
        }
        long priceL = DamageSkinCatalog.getPrice(skinId);
        if (priceL < 0) {
            log.info("chr {} tried to buy unknown damage skin {}", chr.getId(), skinId);
            c.sendPacket(PacketCreator.damageSkinResult(OP_PURCHASE, false, skinId, curMesos));
            return;
        }
        DamageSkinInventory inv = chr.getDamageSkinInventory();
        if (inv.ownsSkin(skinId)) {
            c.sendPacket(PacketCreator.damageSkinResult(OP_PURCHASE, false, skinId, curMesos));
            return;
        }
        int priceI = priceL > Integer.MAX_VALUE ? Integer.MAX_VALUE : (int) priceL;
        if (curMesos < priceI) {
            c.sendPacket(PacketCreator.damageSkinResult(OP_PURCHASE, false, skinId, curMesos));
            return;
        }

        try {
            boolean inserted = inv.addSkin(chr.getId(), skinId);
            if (!inserted) {
                c.sendPacket(PacketCreator.damageSkinResult(OP_PURCHASE, false, skinId, curMesos));
                return;
            }
        } catch (Exception e) {
            log.error("damage skin insert failed for chr {} skin {}", chr.getId(), skinId, e);
            c.sendPacket(PacketCreator.damageSkinResult(OP_PURCHASE, false, skinId, curMesos));
            return;
        }

        chr.gainMeso(-priceI, true);
        c.sendPacket(PacketCreator.damageSkinResult(OP_PURCHASE, true, skinId, chr.getMeso()));
    }
}
```

---

## STEP 7 — EDIT `src/main/java/provider/wz/WZFiles.java`

Add `EFFECT("Effect")` to the enum. Example final list:

```java
public enum WZFiles {
    QUEST("Quest"),
    ETC("Etc"),
    EFFECT("Effect"),   // ← ADD THIS LINE
    ITEM("Item"),
    // ... rest unchanged
```

---

## STEP 8 — EDIT `src/main/java/client/Character.java`

### 8a. Add the field next to `monsterbook`

FIND:
```java
    private MonsterBook monsterbook;
    private CashShop cashshop;
```

REPLACE with:
```java
    private MonsterBook monsterbook;
    private DamageSkinInventory damageSkinInv = new DamageSkinInventory();
    private int activeDamageSkin = 0;
    private CashShop cashshop;
```

### 8b. Add accessors next to `getMonsterBook`

FIND:
```java
    public MonsterBook getMonsterBook() {
        return monsterbook;
    }

    public int getMonsterBookCover() {
        return bookCover;
    }
```

REPLACE with:
```java
    public MonsterBook getMonsterBook() {
        return monsterbook;
    }

    public DamageSkinInventory getDamageSkinInventory() {
        return damageSkinInv;
    }

    public int getActiveDamageSkin() {
        return activeDamageSkin;
    }

    public void setActiveDamageSkin(int skinId) {
        this.activeDamageSkin = skinId;
    }

    public int getMonsterBookCover() {
        return bookCover;
    }
```

### 8c. Load at login (inside `loadCharFromDB`)

FIND:
```java
                    ret.monsterbook = new MonsterBook();
                    ret.monsterbook.loadCards(charid);
```

REPLACE with:
```java
                    ret.monsterbook = new MonsterBook();
                    ret.monsterbook.loadCards(charid);
                    ret.activeDamageSkin = rs.getInt("activeDamageSkin");
                    ret.damageSkinInv.loadSkins(charid);
```

### 8d. Persist the column (inside `saveCharToDB`)

FIND (the huge UPDATE SQL — search for `partySearch = ? WHERE id = ?`):
```java
                try (PreparedStatement ps = con.prepareStatement("UPDATE characters SET level = ?, fame = ?, ... partySearch = ? WHERE id = ?", Statement.RETURN_GENERATED_KEYS)) {
```

REPLACE `partySearch = ? WHERE id = ?` with `partySearch = ?, activeDamageSkin = ? WHERE id = ?`.

Then find:
```java
                    ps.setBoolean(55, canRecvPartySearchInvite);
                    ps.setInt(56, id);
```

REPLACE with:
```java
                    ps.setBoolean(55, canRecvPartySearchInvite);
                    ps.setInt(56, activeDamageSkin);
                    ps.setInt(57, id);
```

---

## STEP 9 — EDIT `src/main/java/net/server/Server.java`

FIND:
```java
        futures.add(initExecutor.submit(SkillbookInformationProvider::loadAllSkillbookInformation));
        initExecutor.shutdown();
```

REPLACE with:
```java
        futures.add(initExecutor.submit(SkillbookInformationProvider::loadAllSkillbookInformation));
        futures.add(initExecutor.submit(client.DamageSkinCatalog::loadOrSeed));
        initExecutor.shutdown();
```

---

## STEP 10 — EDIT `src/main/java/net/opcodes/RecvOpcode.java`

FIND:
```java
    USE_MAPLELIFE(0x100),
    USE_HAMMER(0x104);
```

REPLACE with:
```java
    USE_MAPLELIFE(0x100),
    USE_HAMMER(0x104),
    DAMAGE_SKIN_APPLY(0x110),
    DAMAGE_SKIN_PURCHASE(0x111);
```

---

## STEP 11 — EDIT `src/main/java/net/opcodes/SendOpcode.java`

FIND:
```java
    VICIOUS_HAMMER(0x162),
    VEGA_SCROLL(0x166);
```

REPLACE with:
```java
    VICIOUS_HAMMER(0x162),
    VEGA_SCROLL(0x166),
    DAMAGE_SKIN_CATALOG(0x170),
    DAMAGE_SKIN_INVENTORY(0x171),
    DAMAGE_SKIN_RESULT(0x172),
    DAMAGE_SKIN_BROADCAST(0x173);
```

---

## STEP 12 — EDIT `src/main/java/tools/PacketCreator.java`

### 12a. Add imports

FIND:
```java
import client.Client;
import client.Disease;
```

REPLACE with:
```java
import client.Client;
import client.DamageSkinCatalog;
import client.DamageSkinInventory;
import client.Disease;
```

### 12b. Add packet builders — paste at the bottom of the class, just before the final `}`:

```java
    // ------------------------------------------------------------------
    // Damage Skin packets
    // ------------------------------------------------------------------
    public static Packet damageSkinCatalog() {
        OutPacket p = OutPacket.create(SendOpcode.DAMAGE_SKIN_CATALOG);
        var all = DamageSkinCatalog.getAll();
        p.writeShort(all.size());
        for (var e : all.entrySet()) {
            p.writeInt(e.getKey());
            p.writeLong(e.getValue());
        }
        return p;
    }

    public static Packet damageSkinInventory(Character chr) {
        OutPacket p = OutPacket.create(SendOpcode.DAMAGE_SKIN_INVENTORY);
        p.writeInt(chr.getActiveDamageSkin());
        DamageSkinInventory inv = chr.getDamageSkinInventory();
        var owned = inv.getOwnedIds();
        int count = 0;
        for (int id : owned) if (id != DamageSkinInventory.DEFAULT_SKIN_ID) count++;
        p.writeShort(count);
        for (int id : owned) {
            if (id == DamageSkinInventory.DEFAULT_SKIN_ID) continue;
            p.writeInt(id);
        }
        return p;
    }

    /** op=1 apply, op=2 purchase. */
    public static Packet damageSkinResult(int op, boolean ok, int skinId, int newMesos) {
        OutPacket p = OutPacket.create(SendOpcode.DAMAGE_SKIN_RESULT);
        p.writeByte(op);
        p.writeByte(ok ? 1 : 0);
        p.writeInt(skinId);
        p.writeInt(newMesos);
        return p;
    }

    public static Packet damageSkinBroadcast(int charId, int skinId) {
        OutPacket p = OutPacket.create(SendOpcode.DAMAGE_SKIN_BROADCAST);
        p.writeInt(charId);
        p.writeInt(skinId);
        return p;
    }
```

---

## STEP 13 — EDIT `src/main/java/net/server/channel/handlers/UseCashItemHandler.java`

FIND (near the Fusion Anvil branch — search for `itemType == 561`):
```java
            remove(c, position, itemId); // consume the anvil
            c.sendPacket(PacketCreator.enableActions());
        } else if (itemType == 561) { //VEGA'S SPELL
```

REPLACE with:
```java
            remove(c, position, itemId); // consume the anvil
            c.sendPacket(PacketCreator.enableActions());
        } else if (itemType == 591) { // Damage Skin picker opener (item 5910000)
            // Do NOT consume — the picker opener is reusable.
            c.sendPacket(PacketCreator.damageSkinCatalog());
            c.sendPacket(PacketCreator.damageSkinInventory(player));
            c.sendPacket(PacketCreator.enableActions());
        } else if (itemType == 561) { //VEGA'S SPELL
```

---

## STEP 14 — EDIT `src/main/java/net/PacketProcessor.java`

### 14a. Add imports

FIND:
```java
import net.server.channel.handlers.UseCashItemHandler;
import net.server.channel.handlers.UseCatchItemHandler;
```

REPLACE with:
```java
import net.server.channel.handlers.DamageSkinApplyHandler;
import net.server.channel.handlers.DamageSkinPurchaseHandler;
import net.server.channel.handlers.UseCashItemHandler;
import net.server.channel.handlers.UseCatchItemHandler;
```

### 14b. Register the handlers

FIND:
```java
        registerHandler(RecvOpcode.USE_HAMMER, new UseHammerHandler());
        registerHandler(RecvOpcode.SCRIPTED_ITEM, new ScriptedItemHandler());
```

REPLACE with:
```java
        registerHandler(RecvOpcode.USE_HAMMER, new UseHammerHandler());
        registerHandler(RecvOpcode.DAMAGE_SKIN_APPLY, new DamageSkinApplyHandler());
        registerHandler(RecvOpcode.DAMAGE_SKIN_PURCHASE, new DamageSkinPurchaseHandler());
        registerHandler(RecvOpcode.SCRIPTED_ITEM, new ScriptedItemHandler());
```

---

## STEP 15 — EDIT `src/main/java/net/server/channel/handlers/PlayerLoggedinHandler.java`

FIND:
```java
            c.sendPacket(PacketCreator.updateGender(player));
            player.checkMessenger();
            c.sendPacket(PacketCreator.enableReport());
```

REPLACE with:
```java
            c.sendPacket(PacketCreator.updateGender(player));
            player.checkMessenger();
            c.sendPacket(PacketCreator.enableReport());

            // Damage skin: push catalog + owned list + active id at login.
            c.sendPacket(PacketCreator.damageSkinCatalog());
            c.sendPacket(PacketCreator.damageSkinInventory(player));
```

---

## STEP 16 — EDIT `src/main/java/server/maps/MapleMap.java`

FIND (end of `addPlayer(final Character chr)`):
```java
        chr.receivePartyMemberHP();
        announcePlayerDiseases(chr.getClient());
    }
```

REPLACE with:
```java
        chr.receivePartyMemberHP();
        announcePlayerDiseases(chr.getClient());

        // Damage skin: send new arrival every resident's active skin,
        // then broadcast their own to everyone else.
        for (Character other : getAllPlayers()) {
            if (other == chr) continue;
            int sid = other.getActiveDamageSkin();
            if (sid != 0) {
                chr.sendPacket(PacketCreator.damageSkinBroadcast(other.getId(), sid));
            }
        }
        if (chr.getActiveDamageSkin() != 0) {
            broadcastMessage(chr, PacketCreator.damageSkinBroadcast(
                    chr.getId(), chr.getActiveDamageSkin()), false);
        }
    }
```

---

## STEP 17 — Build

From the server root:

```
./mvnw compile
```

If `BUILD SUCCESS`, start the server. You should see a log line like:

```
DamageSkinCatalog: 641 new rows from WZ, 641 total in catalog
```

The number is however many valid skin folders sit in
`server/wz/Effect.wz/BasicEff.img/damageSkin/`.

---

## ⚠️ GOTCHAS — Read before deploying

These aren't bugs in the tutorial — they're real-world pitfalls that
happen when the baseline source differs from vanilla Cosmic, or the
server is being deployed on Linux, etc. Skim through before blaming the
guide if something goes sideways.

### 1. STEP 8d relies on a clean `characters` UPDATE

STEP 8d shifts the parameter index from `ps.setInt(56, id)` to
`ps.setInt(57, id)`. This is correct **only if** your `saveCharToDB`'s
big UPDATE matches the vanilla Cosmic one (55 existing `?` placeholders
before `id`). If other mods have already added columns to that UPDATE,
my `56` and `57` will land on the wrong slots and characters will save
garbage (wrong meso, wrong level, etc.).

**How to check:** count the `?` in the UPDATE before the `WHERE id = ?`.
`activeDamageSkin` must be one slot before `id`, and `id` must be the
last slot. Adjust the `setInt` indices to match your actual count.

### 2. STEP 12a — import order

STEP 12a shows adding the two new imports "between `Client` and
`Disease`". If your `PacketCreator.java` imports are sorted differently
(alphabetical mod, auto-formatted, etc.) the anchor won't match
literally. **It doesn't matter where in the import block they live** —
just make sure these two lines exist somewhere in the imports:

```java
import client.DamageSkinCatalog;
import client.DamageSkinInventory;
```

### 3. Linux is case-sensitive — Windows is forgiving

If the server runs on Linux (production) while you developed on Windows,
every filename MUST match the public class name exactly, including case.
On Windows `damageskincatalog.java` often "works"; on Linux the compiler
will refuse to find it at all. Double-check every new file:

```
✅ DamageSkinCatalog.java
✅ DamageSkinInventory.java
✅ DamageSkinApplyHandler.java
✅ DamageSkinPurchaseHandler.java
❌ damageskincatalog.java           (will fail on Linux)
❌ DamageSkinCatalog.java.txt       (Notepad's default — hidden extension)
```

Notepad often saves a hidden `.txt` extension. Turn on "File name
extensions" in Explorer to see the real name.

### 4. WZ folder casing — `Effect.wz` vs `effect.wz`

STEP 7 adds `EFFECT("Effect")` to `WZFiles.java`. Cosmic then looks for
a file literally named `Effect.wz`. If your server's `wz/` folder has
`effect.wz` lowercase (or any other casing) and the OS is case-sensitive
(Linux), the catalog loader won't find anything.

Symptom in the server log:
```
DamageSkinCatalog: EFFECT data provider unavailable
DamageSkinCatalog: 0 new rows from WZ, 0 total in catalog
```

Fix: rename the file to `Effect.wz` so it matches the enum string, e.g.:
```
mv wz/effect.wz wz/Effect.wz
```

The server runs fine either way — it's just the damage-skin catalog
that stays empty until the casing matches.

---

## DATABASE — Managing Prices

The catalog table contains ONE row per real skin in your WZ. You can edit
prices with plain SQL — **restart the server afterwards** so the in-memory
cache re-reads from DB.

```sql
-- See all prices
SELECT skinId, priceMesos FROM damageskin_catalog ORDER BY skinId;

-- Change one skin
UPDATE damageskin_catalog SET priceMesos = 50000000 WHERE skinId = 305;

-- Bulk change a range
UPDATE damageskin_catalog SET priceMesos = 25000000 WHERE skinId BETWEEN 100 AND 200;

-- Reset all to the default
UPDATE damageskin_catalog SET priceMesos = 10000000;

-- Wipe and let the server re-seed from WZ on next boot
TRUNCATE damageskin_catalog;
```

### Give / revoke skins for a specific character

```sql
-- Grant a skin for free
INSERT IGNORE INTO damageskin_inventory (characterId, skinId) VALUES (12345, 99);

-- Revoke a skin
DELETE FROM damageskin_inventory WHERE characterId = 12345 AND skinId = 99;

-- Force which skin they have applied (they must re-log)
UPDATE characters SET activeDamageSkin = 0 WHERE id = 12345;
```

### Adding NEW skins to your server

1. Drop the folder into `wz/Effect.wz/BasicEff.img/damageSkin/<newId>/` (must contain `NoRed0`, `NoRed1`, `NoCri0`, `NoCri1`).
2. Restart the server. The boot scan auto-adds a row at 10,000,000.
3. Adjust its price with `UPDATE damageskin_catalog ...` if needed.

---

## Tables — Schema Reference

**`damageskin_catalog`**
| Column | Type | Notes |
|---|---|---|
| `skinId` | `INT PK` | Matches `Effect.wz/BasicEff.img/damageSkin/<id>`. |
| `priceMesos` | `BIGINT` | Default 10,000,000. |

**`damageskin_inventory`**
| Column | Type | Notes |
|---|---|---|
| `id` | `INT PK AUTO_INCREMENT` | |
| `characterId` | `INT` | FK → `characters.id`, `ON DELETE CASCADE`. |
| `skinId` | `INT` | Unique per `(characterId, skinId)`. |
| `acquiredAt` | `TIMESTAMP` | Auto-set. |

**`characters.activeDamageSkin`** — `INT NOT NULL DEFAULT 0`. `0` = no skin.

---

## Opcodes — for debugging

**Client → Server**
| Name | Opcode | Payload |
|---|---|---|
| `DAMAGE_SKIN_APPLY` | `0x110` | `[int skinId]` |
| `DAMAGE_SKIN_PURCHASE` | `0x111` | `[int skinId]` |

**Server → Client**
| Name | Opcode | Payload |
|---|---|---|
| `DAMAGE_SKIN_CATALOG` | `0x170` | `[short n] { [int id][long price] } × n` |
| `DAMAGE_SKIN_INVENTORY` | `0x171` | `[int activeId][short n] { [int id] } × n` |
| `DAMAGE_SKIN_RESULT` | `0x172` | `[byte op=1|2][byte ok][int id][int newMesos]` |
| `DAMAGE_SKIN_BROADCAST` | `0x173` | `[int charId][int skinId]` |

---

The cash-shop item the client picker opens on is **item id 5910000**
(itemType 591). It is never consumed — the picker is reusable.


Lynx