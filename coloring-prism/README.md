# Coloring Prism

A full dye system using the GMS Coloring Prism item, 5782000. Double-clicking it
opens a window with a live avatar preview and three sliders: Hue, Chroma, and Value.

**Items** dyes whatever you drag onto the well. An item carries two independent colours
under separate keys, its own sprite and the effect art hanging off it, so a glow recolours
independently of the blade it hangs off; a pair of chips on the preview pane picks which one
the sliders edit. The well takes **any equip**, cash or not, and also **cash effect items**
(5010000..5019999), the ones that sit in the Cash tab and play an effect around your character.

**Skills** dyes a skill you drag in from the skill window: the cast, the buff effect on the
caster, the mob-hit art and every projectile in a volley. The preview plays the cast so you
can judge the colour before spending anything, and other players see your colours when you
cast.

**Hair**, **Eyes** and **Skin** dye your own look and need no drop. Skin rotates the body,
arm and head canvases (`Character/000020NN.img` and `000120NN.img`) on top of whichever
fixed skin the character already wears, so it composes with `skincolor` rather than
replacing it.

**Setting it up:** see [INTEGRATION.md](INTEGRATION.md). This file explains what the
feature is and how it works; that one is the checklist.

## How the recolour works

`weapontint.cpp` Detours `CAvatar::PrepareActionLayer` (0x00453AD1) and, for the
duration of that one call, swaps HSV-transformed *clones* of the sprite canvases into
the WZ tree, then restores the originals through an RAII guard. The layers keep refs
to the clones, so the avatar recolours while inventory icons and other characters do
not. The file's own header comment documents the four client behaviours that each
silently defeat this; read it before changing anything there.

**WZ aliases are swapped like any other slot, but the alias is what gets restored.** A UOL
child stands in for a node stored somewhere else, and the tree is full of them: every hair
shade past the first, every frame of the head's actions, and a great deal of art that is
reachable *only* through one. The trap is that a UOL resolves transparently, so a walk
reading through it sees the target canvas and, if it restores *that*, it has replaced the
indirection with a hard reference and the alias is gone for the session. Worse, if the
target was already swapped this pass, the alias resolves to the module's own clone, which a
naive walk tints a second time and then records as the original.

So the walks record the **raw slot contents** rather than the resolved canvas, and refuse to
re-swap a canvas this module cloned. Skipping aliases outright is *not* a safe shortcut,
which is worth stating because it looks like one: a walk is scoped to the action being
built, and plenty of frames alias across to a different action that the walk therefore never
visits, so skipping them leaves those frames vanilla while the rest of the pose recolours.

Three kinds of art sit outside that hook and each needed its own path:

| art | where it lives | how it is dyed |
|---|---|---|
| skin | `Character/000020NN.img` + `000120NN.img` | swapped inside the same hook; the head is a separate img |
| cape auras | `Effect/ItemEff.img/<id>/effect` | `WeaponTint_Begin/EndItemEffSwap` around the host's layer build |
| cash effects | `Item/Cash/0501.img/<id>/effect` | two client Detours, `0x0093BEB9` and `0x0093C218` |

An item's **effect art** is dyed separately from its body, under the key
`itemId + 10000000`, which is what the flame chip on the Items tab writes. In this client that art is
almost always a leaf canvas named `effect` sitting beside the `weapon` canvas in each
animation frame: 922 of the 923 equips that have any, against a single one that uses a
folder of them. Both shapes are handled, and the canvas test has to come first, because
a canvas answers `QueryInterface(IWzProperty)` as well, so descending on that interface
walks into the effect and tints nothing at all.

## Contents

```
INTEGRATION.md                 step-by-step setup, start here
client/coloringprism.cpp/.h    the window, and the four entry points it exposes
client/weapontint.cpp/.h       the recolour, the tint state, both opcodes
server/server/colorprism/      packet definitions
server/net/.../handlers/       the action handler
server/db/coloring-prism.xml   the schema, one file
wz/client/                     .img fragments to merge into the client
wz/server/                     the same data in the server's XML form
```

The backdrop ships only as a finished canvas, inside
`wz/client/UI/UIWindow/ColorPrism.img`. There is no layered source file in the package, so a
redraw starts from that canvas.

## The five tabs

`Items · Skills │ Hair · Eyes · Skin`. The rule after the second tab is not decoration: the
first two take a DROP, the last three dye the character's own look and have no well at all.

**Items covers two tint keys, not one.** An item can carry two independent colours: its own
sprite, keyed on the item id, and the effect art hanging off it, keyed on the item id plus
10000000. Those were once two tabs, which read as two destinations when they are really two
layers of one dropped item. They are now one tab with two chips on the preview pane, a sword and
a flame, and the chip decides which key the sliders edit and which `layer` byte the apply sends.

A chip greys out when the item has no such layer, which is structural rather than a preference:
most equips have no effect art, and a cash EFFECT item (5010000..5019999) is effect art with no
sprite at all. `WeaponTint_ItemHasEffectArt` answers the first question by walking the item's img
for an `effect*` child, cached per item id; the id range answers the second. The selection snaps
to whichever layer exists when an item lands, so the sliders can never point at a key nothing
will draw.

**Skills are keyed `skillId + 30000000`** and persist in their own table, because a skill has no
inventory row to carry a colour and a character may dye any number of them. The tab takes a drop
from the skill window and previews the cast in the pane.

## WZ data

Each file under `wz/client/` is a normal `.img` whose **root children are the nodes to
merge**, nothing else:

| file | merge into |
|---|---|
| `wz/client/UI/UIWindow/ColorPrism.img` | `UI/UIWindow.img`, as a new `ColorPrism` node |
| `wz/client/String/Cash.img` | `String/Cash.img` (the item's name and description) |
| `wz/client/Item/Cash/0578.img` | `Item/Cash/0578.img` (the item's icons and `cash` flag) |

`ColorPrism.img` holds eleven canvases. Four are required: `backgrnd`, the 301x406
hand-drawn backdrop, and `trackTone` / `trackChroma` / `trackBright`, the three 176x8
slider gradients. No v83 client has a hue ramp as a standalone canvas, so those three were
generated rather than lifted. Six more are the layer chips,
`LayerBt/{item,glow}/{normal,on,off}`, the 14x14 sword and flame badges the Items tab draws
under the preview; without them the tab still works but neither chip is visible, so the
sliders appear to edit an arbitrary one of the two keys.

The backdrop **bakes the window title and the three row labels** `HUE` / `CHROMA` /
`VALUE`. Everything else is drawn at runtime, including the banner copy and the tab labels,
which is why the window needs three Dotum faces of its own. Redraw or localize the
backdrop and those two pieces of baked text are yours to carry over; the layout constants
at the top of `coloringprism.cpp` were measured off this exact canvas and have to be
re-measured with it.

The eleventh, `backgrndLook`, is **optional**. Hair, Eyes and Skin have nothing to drag
in, so if that node exists the window swaps to it on those three tabs, for a frame with
no drop well; if it is absent every tab keeps `backgrnd`. Their copy is laid out for it
either way: two lines instead of three, centred on the whole blue band (interior
x8..290) rather than the x76 column that clears the icon.

Everything else the window draws is stock: `Basic.img/BtOK2`, `BtCancel2`, `Tab2`,
`Slider/thumb*` and `BtClose`.

`wz/server/` carries the same String and Item entries in the server's XML form.
The client `.img` and the server XML are **two copies of the same data and must stay in
sync**; the server reads its own copy for item names and the `cash` flag, and will
reject the item if it is missing.

## Host integration

The feature-specific files are all here. These hooks belong to files that also contain
unrelated systems, so they are described rather than bundled.

**Client**

- Build both `.cpp`; call `AttachWeaponTintMod()` and `AttachColoringPrismMod()` at DLL
  startup. The two together own **six** addresses. `weapontint.cpp` takes five:
  `0x00453AD1` (`PrepareActionLayer`, the body), `0x00453696` (the face builder, which is
  what makes the Eyes tab work), `0x00933990` (`CUser::ShowSkillEffect`, the whole Skills
  tab, for local and remote casts alike), and `0x0093BEB9` / `0x0093C218` (the two
  cash-effect builders). `coloringprism.cpp` takes the sixth, `0x004FAA22`
  (`CDraggableSkill::OnDropped`), which is how a skill reaches the well. The one-owner rule
  applies to every one of them.
- `0x00933990` takes **five** stack args (`__thiscall`, `ret 0x14`). A four-arg declaration
  compiles and corrupts the stack at runtime rather than failing at link time.
- Call `WeaponTint_Tick()` once per frame from the update hook.
- Route inbound opcode `0x372F` to `WeaponTint_HandleSync(CInPacket*)`.
- Dispatch the double-click from the three cash-use addresses, `0x00A0A63F` /
  `0x00A1DC5B` / `0x004863D5`, without adding a second Detour to any of them. The first
  two are send paths you intercept and swallow; **the third is a gate that must return
  nonzero for 5782000**, because group 578 is outside the range its stock answer covers
  and the click is otherwise dropped before any packet exists.
- From the `CDraggableItem::OnDropped` hook (`0x004EF140`), forward to
  `ColorPrism_HandleItemDrop(pTo, invType, invPos)`. Same rule: no second Detour. The
  inventory type and position are not parameters of that hook; they are the drag *source*,
  read off the `CDraggableItem` at `+0x18` / `+0x1C`.

**Server**

- `ItemId`: add `COLORING_PRISM = 5782000`.
- Register receive opcode `0x372E` to `WeaponTintHandler`, and add send opcode
  `0x372F` for `ColorPrismPackets`.
- `Equip`: six fields plus `getTintHue/Chroma/Bright`, `setTint`, `clearTint`,
  `isTinted` and the `Fx` variants of all five. Copy all six in the clone path.
  `setTint` normalises hue with `normalizeTintHue` (see below) and clamps the other two
  to -100..100.
- `Character`: nine fields plus `getHairTint*` / `getFaceTint*` / `getSkinTint*` and the
  matching `set` / `clear` / `is...Tinted` for all three. Load them in the character
  `SELECT` and write them in the save path. Also a `skillTints` map with
  `getSkillTints` / `isSkillTinted` / `setSkillTint` / `clearSkillTint`, loaded from
  `skilltints` and written back in the SAME transaction as the rest of the save, so a
  rollback cannot leave the colours and the character disagreeing.
- `ItemFactory`: the six equip columns and the three `efftint*` item columns, on the
  INSERTs and on the readers, so a tint survives a relog. Cosmic prepares **each** of
  those two statements in two places, `saveItemsCommon` and `saveItemsMerchant`; bind all
  four or the one you missed throws `Column count doesn't match value count` the first
  time a hired merchant saves.
- `Item`: three `effTint*` fields for cash effect items, plus `copy()`. Required, not
  optional: both shipped Java files call these accessors unconditionally.
- **No `isCash` gate.** `resolve()` deliberately does not require the item to be a Cash
  item, and the client's drop gate matches. Relax or tighten both together.
- Send a snapshot on login, on map entry, and after an equipment change:
  `ColorPrismPackets.snapshot(chr)` to the player and
  `ColorPrismPackets.broadcastMapTable(map)` to the map.

**Database**: drop `server/db/coloring-prism.xml` into `resources/db/extensions/`. Nothing
else. Stock Cosmic's `changelog-root.xml` already ends with `includeAll path="extensions"`,
so adding an `<include>` on top of that registers every changeset twice and Liquibase
aborts the server at boot.

Eighteen columns and one table, in five changesets: six tint columns on
`inventoryequipment` (the item's own colour, then its effect sprites'), nine on `characters`
(hair, eyes, then skin), three on `inventoryitems` (cash effect items), the `skilltints`
table, and an optional shop row you can retarget or delete.

`skilltints` is the one piece of new schema, and it exists because skills are the one target
with nowhere to put a colour: a character has exactly one hair and one skin, so those are
columns, and an item carries its colours on its own inventory row, but a skill has no row and
a character may dye any number of them. It is keyed `(characterid, skillid)` with a foreign
key to `characters(id)` that cascades on delete, so a deleted character takes its palettes
with it rather than leaving them for whoever next gets that id. No row means vanilla.

The columns append at the end of each table rather than pinning a position, so nothing
here depends on your column order. Make sure your equipment INSERT names its columns
explicitly instead of using positional `VALUES`; if it does, appending cannot break
inventory saves.
## Hue: two forms, and why the sign matters

`hue` is a signed short and its **sign selects the semantic**:

| value | meaning |
|---|---|
| `0` | leave the hue alone |
| `1..359` | **rotate** the sprite's own hue by that many degrees |
| `-1..-360` | **set** an absolute target hue, encoded as `-(degrees + 1)`; `-1` is 0 degrees |

Both are needed. A rotation preserves a sprite's internal hue relationships, so a two-tone
item keeps both tones; an absolute target collapses every pixel onto one hue. Measured over
235 items, 40% are not essentially single-hue and 30% clearly carry several, so a scheme
that can only express one of the two is wrong for a third of the wardrobe either way.

Encoding absolute as **negative** also makes the sign a version tag: every record written
before this existed is positive and keeps rotating, so introducing absolute hue later needs
no migration and resets nobody's dyes. It costs no column and no packet byte, because
`SMALLINT` is signed.

**The trap this creates:** anything that normalises hue must not use a plain
`floorMod(hue, 360)`, which turns `-1` into `359` and silently converts "absolute red" into
"rotate by 359". The server clamps negatives into their own band and wraps only the positive
half; `normalizeTintHue` in `Equip` / `Item` / `Character` is that rule, and
`WeaponTintHandler.isValidTint` rejects anything outside the two bands before a prism is
consumed.

**Chroma and Value interpolate towards their bounds** rather than scaling: positive moves
towards 1, negative towards 0. The old `s *= (100 + chroma)/100` pinned to 1.0 long before
the slider ran out and was a mathematical no-op on an achromatic pixel, so a grey cape did
nothing at any Chroma. One consequence worth knowing: because Value now lifts towards 1,
dark outlines lighten as Brightness rises, and at +100 every pixel reaches 1.0 and the
sprite goes white.
