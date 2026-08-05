# Integration

The copied files are not enough on their own. These are the edits inside files you already have.
Each one **fails silently** if you skip it, so they are worth doing carefully.

Line numbers are from the source repo and are a hint, not a location.

---

## 1. Client

### 1a. Route the reply — otherwise nothing the server sends ever arrives

Wherever your DLL already intercepts `CClientSocket::ProcessPacket`, add the route:

```cpp
#include "cashshopwnd.h"

// ... inside the packet-dispatch hook, before the default handling:
if (nType == kCashShopSyncOpcode) {
    CashShopWnd_HandleSync(iPacket);
    return;
}
```

Two things the source repo learned the hard way:

- **Do not add a second Detour on `0x004965F1`.** In a DLL that already hooks it several times
  over, a fresh hook there has proven unreliable in exactly one direction: the request reaches
  the server and the reply never decodes. Route through the dispatcher you already have.
- **The packet arrives with its offset still at the opcode.** `CashShopWnd_HandleSync` consumes
  it itself (`Reader::Skip2()`), relative to the offset it was handed. If your dispatcher has
  already consumed the opcode, drop the skip.

### 1b. Drive the per-frame tick — otherwise the window never opens

```cpp
extern void CashShopWnd_Tick();

// ... early in your per-frame main-thread update (CWvsApp::CallUpdate is where the
// source repo puts it), before the stage update loop:
CashShopWnd_Tick();
```

`CashShopWnd_HandleSync` runs on the **receive thread**. Creating a `CWnd` is a **main-thread**
job, so the sync handler only fills the catalogue under a mutex and raises a flag; this tick is
what acts on it. Without it the server's open reply is received, parsed, and then does nothing at
all — which looks exactly like a dead button.

### 1c. Build

Add `cashshopwnd.cpp` to your sources. **C++17** is required.

---

## 2. Server: opcodes and handler

`net/opcodes/RecvOpcode.java`:

```java
CASHSHOP_WINDOW_ACTION(0x3730),
```

`net/opcodes/SendOpcode.java`:

```java
CASHSHOP_WINDOW_SYNC(0x3731),
```

`net/PacketProcessor.java` — import, then register alongside the other handlers:

```java
import net.server.channel.handlers.CashShopWindowHandler;

registerHandler(RecvOpcode.CASHSHOP_WINDOW_ACTION, new CashShopWindowHandler());
```

Pick a different pair if `0x3730`/`0x3731` collide with something of yours — but change
`cashshopwnd.h` to match, since nothing enforces that the two sides agree.

---

## 3. Server: open the window instead of the stage

In `EnterCashShopHandler` (whatever handles the player pressing the cash shop button), replace
the stage transition with the window:

```java
if (USE_STANDALONE_WINDOW) {
    // No stage change, no channel/map detach, no buff teardown: the player stays
    // exactly where they are and a window opens over the field.
    c.sendPacket(PacketCreator.enableActions());
    c.sendPacket(CashShopWindowPackets.open(mc));
    CashShopWindowHandler.sendCatalog(c, mc);
    return;
}
// ... the original stage path below, kept behind the flag
```

**`enableActions()` is mandatory, not decorative.** The client latches itself on the cash-shop
request and stays input-locked until something releases it. Without this the player is frozen in
place with a window open over them.

Keeping the original path behind a boolean lets you fall back without deleting anything.

---

## 4. Runtime files

Create `<server working dir>/cashshop/` and put `catalog.tsv` in it, or set
`-Dcashshop-path=/some/where`. The catalogue is read **once at startup**; the server logs the row
count, the category count and how many rows it rejected (including duplicate item ids), so check that line before assuming the
shop is empty for a different reason.

---

## 5. The painted plate

The window blits `UI/CashShop.img/Base/WndBg` for all of its chrome. Install it with
`tools/import_cashshop_bg.py` (preserves your file) or by dropping in `wz/UI/CashShop.img`
(overwrites it). Close the client first — a running client holds the file open.

Full detail in README.md under *The painted plate*. Two things worth repeating here:

- **If the node is missing the window still runs**, falling back to primitive-drawn chrome. So a
  window that looks plain rather than broken means the plate did not install, not that the code
  is wrong.
- **These labels are baked into the art and are NOT drawn**: `CASH SHOP`, `PREVIEW`, `CART`, `NX`,
  `MP` on the plate, and `BUY` / `BUY CART` on the button art. If you repaint either without
  them, they disappear entirely — there is no font fallback, because the window loads no font.

---

## Checklist

- [ ] `cashshopwnd.cpp` + `.h` copied, `.cpp` added to the build
- [ ] Reply opcode routed to `CashShopWnd_HandleSync`
- [ ] `CashShopWnd_Tick()` called every frame on the main thread
- [ ] Two opcodes added, handler registered
- [ ] Cash shop button rewired, `enableActions()` sent
- [ ] `catalog.tsv` in place, startup log shows the row count
- [ ] `CashShop.img/Base/WndBg` installed (window looks painted, not plain)
