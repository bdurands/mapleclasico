# Server-side implementation — World-Map "Players" tooltip

Everything you need to add the server half of the world-map *Players* section to a
**Cosmic / HeavenMS-family** server (Java, Netty packet stack, `RecvOpcode` /
`SendOpcode` enums, `PacketProcessor`, `AbstractPacketHandler`). The client half ships
in the Kaentake DLL (see `client/`); the server is the source of truth for who-is-where.

There are exactly **four** changes:

| # | File                                                        | Change                                  |
|---|-------------------------------------------------------------|-----------------------------------------|
| 1 | `net/opcodes/RecvOpcode.java`                               | add `WORLD_MAP_PLAYERS(0x115)`          |
| 2 | `net/opcodes/SendOpcode.java`                               | add `WORLD_MAP_PLAYERS(0x178)`          |
| 3 | `net/server/channel/handlers/WorldMapPlayersHandler.java`  | **new file** (provided)                 |
| 4 | `net/PacketProcessor.java`                                  | register the handler for the recv opcode|


---

## 1. The protocol

Two **distinct** opcode values, one per direction. (They don't have to differ, but
recv and send opcode spaces are separate enums, and `0x115`/`0x178` are what this
client DLL uses.)

```
RecvOpcode.WORLD_MAP_PLAYERS = 0x115    client - server   (the request)
SendOpcode.WORLD_MAP_PLAYERS = 0x178    server - client   (the reply)
```

### Request body (client → server), after the 2-byte opcode

| Field   | Type | Notes                                   |
|---------|------|-----------------------------------------|
| `mapId` | int  | the hovered map id (e.g. `100000000`)   |

### Reply body (server → client), after the 2-byte opcode — **field order is exact**

The client decodes positionally, so the order and widths below are not negotiable.

| Field     | Type             | Notes                                                      |
|-----------|------------------|------------------------------------------------------------|
| `mapId`   | int              | echo back; the client matches the hovered map by this      |
| `count`   | short            | number of players that follow (server caps at 40)          |
| — repeat `count` times: —                                                              |
| `name`    | string           | `writeString`: 2-byte LE length prefix, then the raw bytes |
| `level`   | short            | character level                                            |
| `channel` | byte             | 1-based channel number                                    |

**Endianness / encoding (Cosmic `ByteBufOutPacket`):** `writeInt`/`writeShort` are
**little-endian** (`writeIntLE`/`writeShortLE`); `writeString` writes a little-endian
`short` byte-length then the bytes in the server's configured charset (defaults to
`US-ASCII` — single byte per char). Player names are ASCII, so this is one byte per
character, which is exactly what the client reads back. You don't have to think about
any of this if you use `OutPacket`/`writeString` as shown — it's the house format.

The client's matching decoder is `players::HandleResponse` in
[`client/worldmapinfo.cpp`](client/worldmapinfo.cpp) (search for `LP_WORLD_MAP_PLAYERS`).

---

## 2. Edit `RecvOpcode.java`

Add one enum constant. Concretely, in this codebase it sits among the other custom
Kaentake opcodes:

```java
    BAG_WINDOW(0x113),             // Ore/Scroll bag windows
    CRIT_RECOVERY(0x114),
    WORLD_MAP_PLAYERS(0x115),      // ← ADD: World-map tooltip "Players" request
    MONSTER_BOOK_DROP(0x116),
```

If your enum's custom block ends with a semicolon-terminated last entry, use the usual
trick — change the trailing `;` to `,` and append the new entry with the `;`:

```java
        ...
        BAG_WINDOW(0x113),
        WORLD_MAP_PLAYERS(0x115);   // 0x114 here is taken by CRIT_RECOVERY; pick any FREE value
```

**Choosing the value:** `0x115` is what this client DLL sends. Only change it if you
also rebuild the client (§6). If `0x115` already collides with something in *your*
`RecvOpcode`, you must rebuild the client to a free value — you cannot just remap the
server.

## 3. Edit `SendOpcode.java`

Symmetric — add the reply opcode:

```java
    BAG_WINDOW(0x176),             // Ore/Scroll bag windows (reply)
    USER_INFO_EX(0x177),
    WORLD_MAP_PLAYERS(0x178),      // ← ADD: World-map tooltip "Players" reply
    MONSTER_BOOK_DROP(0x179),
```

`0x178` is what the client's inbound router (`PacketDispatcher.cpp`,
`kWorldMapPlayersReply`) listens for. Same rule: change it only if you rebuild the
client, and keep it free in your `SendOpcode` space.

## 4. Add the handler

Copy the provided file verbatim to:

```
src/main/java/net/server/channel/handlers/WorldMapPlayersHandler.java
```

It's self-contained and reproduced/annotated in §7 below. In short: read `mapId`, walk
every channel of the requester's world, collect characters on that map (skipping hidden
GMs for non-GM requesters), reply with `(name, level, channel)` capped at 40, with a
short result cache to absorb hover bursts. Everything is wrapped so a malformed request
can never crash the channel thread.

## 5. Register the handler

In `net/PacketProcessor.java`, next to the other custom registrations:

```java
        registerHandler(RecvOpcode.WORLD_MAP_PLAYERS,
                new net.server.channel.handlers.WorldMapPlayersHandler());
```

That is the entire wiring. The reply opcode (`SendOpcode.WORLD_MAP_PLAYERS`) needs no
registration — only inbound (recv) opcodes are dispatched through `PacketProcessor`;
the handler builds and sends the outbound packet itself.

---

## 6. ⚠️ Opcode values are baked into the client DLL

The client carries the two values as compile-time constants:

* `worldmapinfo.cpp` — `CP_WORLD_MAP_PLAYERS = 0x115`, `LP_WORLD_MAP_PLAYERS = 0x178`
* `PacketDispatcher.cpp` — `kWorldMapPlayersReply = 0x178`

So:

* **Using the prebuilt DLL as-is** → your server **must** use `0x115` (recv) and
  `0x178` (send). Non-negotiable.
* **Rebuilding the client** → you may pick any free values, but change them in **both**
  client files above *and* the two server enums, consistently. Recv value goes in the
  client `CP_…` constant + server `RecvOpcode`; send value goes in the client `LP_…`
  constant **and** `kWorldMapPlayersReply` + server `SendOpcode`.

Mismatch symptoms: wrong **recv** value → the request never reaches the handler, tooltip
stuck on `Players (loading...)`. Wrong **send** value → the client never recognizes the
reply, same stuck `loading...`.

---

## 7. The handler, explained

Full source is `server/.../WorldMapPlayersHandler.java`. The logic:

```java
public void handlePacket(InPacket p, Client c) {
    int mapId = p.readInt();                       // the only request field
    Character self = c.getPlayer();
    if (self == null || mapId <= 0) return;        // ignore junk / not-in-game

    World world = c.getWorldServer();
    if (world == null) return;

    boolean requesterIsGM = self.isGM();           // gates hidden-GM visibility

    // (optional) short result cache, keyed world+map+GM-visibility — see note below
    ...
    List<Found> found = new ArrayList<>();
    for (Channel ch : world.getChannels()) {       // every channel of THIS world
        int channelId = ch.getId();                // 1-based; from the Channel, never null
        for (Character chr : ch.getPlayerStorage().getAllCharacters()) {
            if (chr == null || chr.getMapId() != mapId) continue;
            if (chr.isHidden() && !requesterIsGM)   continue;   // hide hidden GMs from players
            found.add(new Found(chr.getName(), chr.getLevel(), channelId));
            if (found.size() >= MAX_PLAYERS) break; // cap at 40 (must match the client cap)
        }
    }

    OutPacket out = OutPacket.create(SendOpcode.WORLD_MAP_PLAYERS);
    out.writeInt(mapId);                            // echo
    out.writeShort(found.size());
    for (Found f : found) {
        out.writeString(f.name());                 // 2-byte LE len + bytes
        out.writeShort(f.level());
        out.writeByte(f.channel());
    }
    c.sendPacket(out);
}
```

Design choices worth keeping:

* **Walk per channel, read the channel id from the `Channel` object.** Don't derive the
  channel from `chr.getClient().getChannel()` — a character whose client is momentarily
  null (channel change, cash shop, etc.) would NPE or report a stale channel. The outer
  loop already knows the channel.
* **`MAX_PLAYERS = 40` must equal the client cap** (`players::MAX_PLAYERS` in
  `worldmapinfo.cpp`). The client also clamps, but matching avoids sending bytes the
  client will throw away.
* **Hidden GMs are filtered for non-GM requesters** (`isHidden() && !isGM()`). This is
  the only privacy rule; the reply carries no character preview, just the line list.
* **Catch-all around the scan.** A lookup error replies with whatever was collected so
  far rather than letting an exception reach the Netty channel thread.
* **The short result cache (≈750 ms, keyed `worldId:mapId:isGM`) is optional but
  recommended.** Hovering fires the request repeatedly; without the cache each hover is
  a full multi-channel `O(online)` scan on the packet thread. The tooltip is purely
  informational, so a sub-second-stale roster is fine. The provided file includes it;
  if you'd rather keep the handler minimal, delete the cache block and scan every time —
  the wire behavior is identical.

### Building the reply inline vs. via `PacketCreator`

The provided handler builds the packet inline with `OutPacket`. If your codebase
convention is to put wire-building in `tools.PacketCreator` (as some servers do for
every packet), move these exact writes into a `PacketCreator.worldMapPlayers(...)`
method and have the handler call it — the bytes are identical. Either is fine.

---

## 8. Server-API assumptions to verify

These are standard Cosmic/HeavenMS APIs. If your fork renamed anything, adjust:

| Call                                              | Returns                  | Used for                          |
|---------------------------------------------------|--------------------------|-----------------------------------|
| `Client.getWorldServer()`                         | `World`                  | the requester's world             |
| `World.getChannels()`                             | `List<Channel>`          | iterate channels                  |
| `Channel.getId()`                                 | `int` (1-based)          | the channel number in the reply   |
| `Channel.getPlayerStorage().getAllCharacters()`   | `Collection<Character>`  | online characters on that channel |
| `Character.getMapId()`                            | `int`                    | filter to the hovered map         |
| `Character.getName()` / `getLevel()`              | `String` / `int`         | reply fields                      |
| `Character.isGM()` / `isHidden()`                 | `boolean`                | hidden-GM privacy filter          |
| `OutPacket.create(SendOpcode)` + `writeInt/Short/String/Byte` | —            | build the reply                   |
| `Client.sendPacket(OutPacket)`                    | —                        | send the reply                    |

**Alternative if your `World` exposes a single world-wide storage** instead of
per-channel storages:

```java
for (Character chr : world.getPlayerStorage().getAllCharacters()) {
    if (chr.getMapId() != mapId) continue;
    int channelId = chr.getClient().getChannel();   // beware: client may be null mid-transition
    ...
}
```

The per-channel walk in the provided handler is preferred precisely because the channel
id then comes from the `Channel` and is never null.

---

## 9. Build, deploy, test

1. Drop the four changes in (3 edits + 1 new file).
2. Rebuild the server. *(This repo: `cd server && docker compose build maplestory`,
   then `docker compose up -d maplestory`. Generic Maven source: `mvn -q -DskipTests
   package` and restart your launcher.)*
3. Confirm a clean boot — look for the normal "now online" / world-and-channels-online
   line in the server log. The new handler logs nothing of its own.
4. In game, with the matching client DLL: open the **World Map**, hover a map that has
   players on **any** channel. The tooltip grows a purple **`Players (N):`** section
   listing `[Lv.x] Name (Ch.y)`.

### Client timing to expect while testing (so you don't think it's broken)

* The client only requests when the hovered map **changes**, and re-requests a given map
  at most every **2.5 s**; it caches a reply for **6 s**. So the roster refreshes a few
  seconds after players move maps/channels — by design, to keep hovering off the packet
  thread.
* The tooltip redraws on the next **mouse-move** over the spot. Nudge the cursor to pull
  freshly-arrived data onto the screen.
* First hover of a never-before-seen map shows `Players (loading...)` for a beat, then
  fills in on the next mouse-move once the reply lands.

---

## 10. Troubleshooting

| Symptom                                             | Likely cause                                                                 |
|-----------------------------------------------------|------------------------------------------------------------------------------|
| Tooltip stuck on `Players (loading...)` forever     | Handler not registered, or **recv** opcode mismatch (server isn't getting `0x115`). Verify §2/§5. |
| Request arrives (log shows it) but nothing renders  | **Send** opcode mismatch — client's `kWorldMapPlayersReply`/`LP_…` ≠ server `SendOpcode`. See §6. |
| No `Players` section at all, even `(loading...)`    | Client side not installed/old DLL. This package's server half can't add it alone. |
| Players listed on wrong channel, or NPE in logs     | You derived channel from `chr.getClient().getChannel()` instead of the `Channel` loop. Use the provided per-channel walk (§8). |
| Hidden GM shows up for normal players               | Missing the `isHidden() && !isGM()` filter (§7).                              |
| Garbled names / truncated list on the client        | Wire order or widths changed. Reply must be exactly int, short, then `{string, short, byte}×count` (§1). |
| Hovering causes brief server lag spikes             | High population + cache removed. Keep the ≈750 ms result cache (§7).          |

Because the feature is additive and read-only, a missing or broken server half never
crashes anything — the worst case is the `Players` line never filling in. You can add,
remove, or re-deploy the server side at any time without touching the client.
