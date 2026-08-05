# In-Game Cash Shop

A windowed cash shop for MapleStory v83 private servers. It opens **over the field** instead of
switching to the cash shop stage, and its merchandise lives in a **server-side file** instead of
`Etc/Commodity.img`.

Built for a Cosmic-derived server (https://github.com/P0nk/Cosmic) plus a Detours-based client
DLL. The client half is one `.cpp` and one `.h`; the server half is four Java files.

---

## What it does that the stock shop does not

**No stage transition.** The vanilla shop is a separate stage: entering it detaches you from the
map, tears down buffs and closes interactions. This is a `CWnd` drawn over the field, so you keep
playing. Opening and closing costs nothing.

**No row cap, and serials do not encode their category.** The stock client reads `Commodity.img`
and infers a row's tab from its serial, which caps how much can live in any one category. Here
the server owns the catalogue and sends **one category at a time, on demand**, so the shop's size
is bounded by nothing. The included v83 catalogue is 1,843 items; the author's own is far larger.

**A live avatar preview.** The preview is a playground, not a portrait: the character walks with
real momentum (the constants come from `Map.wz/Physics.img`), jumps, attacks, plays facial
expressions and cash-item effects, and **climbs the ladder** — which is the only way to see the
back of a cape or hat before buying it. Three swappable backdrops, picked from the tabs in the
preview header.

**A cart.** Double-click an item to pin it to the preview and the cart; buy the lot in one
transaction that validates, checks affordability and reserves inventory space **cumulatively**
before delivering anything.

**Search**, because a category can hold two thousand hats.

**A focus lamp** beside the PREVIEW label — green when the window owns the keyboard, red when it
does not. The preview responds to arrow keys, so knowing where input is going matters. It is
drawn in both states deliberately: a lamp that disappears when unfocused is indistinguishable
from no lamp at all.

---

## What is in this folder

```
art/
  backgrnd.png         the painted 760x495 window plate (source art)
client/
  cashshopwnd.cpp      the window: ~3,500 lines, the whole client half
  cashshopwnd.h        the opcode pair and the three entry points
data/
  catalog.tsv          1,843 items, generated from the STOCK v83 commodity list
server/
  server/cashshop/CashShopCatalog.java          reads the TSV
  server/cashshop/CashShopWindowPackets.java    the wire format
  server/cashshop/CashShopWindowPurchase.java   validate / afford / space / deliver / debit
  net/server/channel/handlers/CashShopWindowHandler.java
tools/
  gen_cashshop_catalog_v83.py    regenerates the catalogue from any v83 client
  import_cashshop_bg.py          installs the plate into your own CashShop.img
  make_cashshop_buttons.py       re-bakes the two buttons if you change their look
wz/
  UI/CashShop.img      a ready-made CashShop.img with the plate already in it
INTEGRATION.md         the edits to make in files you already have
```

## Requirements

- A v83 client.
- **A client DLL project with a `CWnd` reimplementation.** This is the one part that is not
  drop-in, so it is worth being blunt about: `cashshopwnd.cpp` includes nine headers that are
  NOT in this package, because they are the host DLL's framework rather than the feature —

  ```
  pch.h  hook.h  debug.h            logging and precompiled headers
  wvs/wnd.h  wvs/wndman.h           the CWnd / window-manager reimplementation
  wvs/packet.h                      COutPacket / CInPacket
  wvs/iteminfo.h  wvs/util.h        item icons, the ResMan singleton
  ztl/ztl.h                         Ztl_bstr_t and friends
  ```

  `CUICashShop` **derives from** `wvs/wnd.h`'s `CWnd` and overrides ten of its virtuals, so if
  your DLL's window base differs, that is real adaptation work rather than a rename. Everything
  else in this package is drop-in.
- Java 17+ and a Cosmic-derived server.

**No fonts to install.** The window draws everything in Dotum, which every Windows install
already has. Its capitalised labels are not drawn at all: the plate carries `CASH SHOP`,
`PREVIEW`, `CART`, `NX` and `MP`, and the button art carries `BUY` and `BUY CART`. Earlier
versions shipped a TTF for those; it is no longer needed at runtime.

**Art.** Almost everything the window loads is stock: `Basic.img` (close button, scrollbar),
`CashShop.img/Base/Preview/0..2` (the three backdrops) and `UIWindow.img` (`Item/New/Tab0|Tab1`
for the category tabs). The additions all live under `CashShop.img/Base/` — the painted plate at
`WndBg` and the two pre-lettered buttons at `BtBuy/` and `BtCart/`. See below.

**The client half is pinned to one binary.** Every address it reads is from the standard v83
`MapleStory.exe` at image base `0x400000`. It installs no hooks of its own, so it is safe to drop
in, but on a different or repacked executable the addresses will be wrong.

---

## The painted plate

The window does not draw its own chrome. One blit of `CashShop.img/Base/WndBg` supplies the
frame, the caption, both tab bands, the red rule, the section-head strips, every icon well and
the footer band; the DLL draws only what changes — text, icons, tab selection, hover, the avatar
and the buttons. Several labels (`CASH SHOP`, `PREVIEW`, `CART`, `NX`, `MP`) are baked into the
art and deliberately not drawn.

The two buttons are art too: `Base/BtBuy/` and `Base/BtCart/`, four states each, 76x18, with
their labels already lettered. `tools/make_cashshop_buttons.py` bakes them from the stock
`Shop/BtBuy` — run it if you restyle them. Both node groups ship inside `wz/UI/CashShop.img`.

**Two ways to install it. Pick one.**

1. **Import into your own file (recommended).**
   ```bash
   python tools/import_cashshop_bg.py --src art/backgrnd.png --write
   ```
   Point `TARGET` at your `CashShop.img` first. This preserves whatever else your file contains,
   backs the original up, and refuses to write unless the result round-trips pixel-for-pixel with
   `Base/Preview/0..2` still intact.

2. **Drop in `wz/UI/CashShop.img`.** Faster, but it **overwrites your whole file** — if you have
   your own `CashShop.img` edits you will lose them. It is a v83 file with one node added.

**Close the client first either way.** A running client holds `CashShop.img` open and the write
fails with a permission error.

If the node is missing entirely the window still runs: it falls back to drawing chrome from
primitives. Usable, but plain.

**Changing the size means repainting.** Every geometry constant is derived from the plate, so the
window is 760x495 until the art is.

---

## Install

Copy the files, then make the edits in **INTEGRATION.md** — those are the parts that are easy to
miss, and every one of them fails silently rather than loudly.

**Server**

1. Copy `server/*` into `src/main/java/`.
2. Copy `data/catalog.tsv` to `<server>/cashshop/catalog.tsv`. Override the directory with
   `-Dcashshop-path=...` if you want it elsewhere.
3. Add the two opcodes and register the handler (INTEGRATION.md §2).
4. Point the cash shop button at the window (INTEGRATION.md §3).

**Client**

1. Copy `client/cashshopwnd.cpp` and `cashshopwnd.h` into your DLL sources; add the `.cpp` to
   your build. **C++17** required.
2. Install the plate — see *The painted plate* above.
3. Route the reply opcode and drive the per-frame tick (INTEGRATION.md §1). **The tick is not
   optional**: the window is created on the main thread in response to a flag the receive thread
   raises, so without it the server's reply does nothing.

   There is no init or attach call. The window installs no hooks and loads no fonts.

---

## The catalogue

Tab-separated, one row per purchasable item, `#` comments ignored:

```
itemId  price  count  tab  category  period  gender  name
```

**The item id is the key — there are no serial numbers.** One row per item, and buying sends
the item id. Stock v83 gives every *offer* a serial and derives its tab from the number
(`sn/10000000`), which is exactly why the vanilla shop has a per-category ceiling. Here the row
carries its own `tab` and `category`, so nothing has to be encoded in an id and there is no cap.

`tab` and `category` are the v83 pair from `Etc/Category.img` — `(2,0)` is Hat, `(6,1)` is
Pet Equip. `period` is days, **0 meaning permanent**. `gender` is 0 male / 1 female / 2 both.
`name` is what the window prints; the client never looks it up.

The trade: an item can only be sold one way. v83 used serials to list 73 items at two
quantities (a 1x and an 11x bulk discount) and 103 at two prices; the generator keeps the
cheaper per-unit row and drops the rest.

### Regenerating it

```bash
python tools/gen_cashshop_catalog_v83.py --dry-run
python tools/gen_cashshop_catalog_v83.py --write --out data/catalog.tsv
```

It reads your client's `Etc/Commodity.img` and `Etc/Category.img`, names from `String.wz`, and
validates every row against the real category table — a row landing on a pair `Category.img` does
not define is a hard error, not a warning.

Defaults to the 1,843 items Nexon had **on sale**; `--all` includes the 6,937 they had switched
off. Cash packages (`91xxxxx`) are always excluded: they are bundles, not items, and the purchase
path has no inventory type to deliver one into.

---

## Known constraints

- **The client half is v83-address-pinned.** See above.
- **Opcodes `0x3730`/`0x3731` are a convention, not a contract.** They are declared once in
  `cashshopwnd.h` and once in `RecvOpcode`/`SendOpcode`. Nothing checks that they agree, so
  change both sides or neither.
- **760x495**, so it assumes at least an 800x600 client.
