#!/usr/bin/env python3
"""Build a STOCK v83 Cash Shop catalogue -- the shareable one.

This is the sibling of gen_cashshop_catalog.py. That script builds THIS server's catalogue,
which pulls ~9,600 rows out of the live GMS v269 Commodity.img; useful here, useless to anyone
else, because those item ids do not exist in a v83 client and would all draw as blank icons.

This one reads the STOCK v83 commodity list that every v83 client already ships, so the
catalogue it produces works on any v83 server with no WZ changes at all.

SOURCE
  Client/Data/Etc/Commodity.img -- and specifically a copy that predates this repo's own
  imports. The file has SN / ItemId / Count / Price / Period / Gender / OnSale per row.

  Commodity.img is a SOURCE here, read once at authoring time. The server never touches it at
  runtime: it reads only the TSV this writes. That is what frees a serial from having to encode
  its own tab and category, and with it the per-category row ceiling the stock shop has.

NO SERIAL NUMBERS. The item id is the key. Stock v83 gives every cash shop OFFER a serial and
lets the same item appear several times -- 103 items carry a stale second price and 73 were sold
in two quantities as a bulk discount -- but an offer id is a thing catalogue authors have to
invent and keep unique for no benefit they can see. Duplicates collapse to the cheaper per-unit
row, which costs the bulk discounts and nothing else.

THE (TAB, CATEGORY) MAPPING is not invented and not guessed from serial arithmetic. v83 ships
the authoritative table in Etc/Category.img as (Category, CategorySub, Name) triples -- 'Hat'
is (2,0), 'Pet Equip.' is (6,1), and so on. Every row this emits is assigned a pair that
appears in that table, and the run aborts if any row lands on a pair that does not.

Within a tab, the category is derived from the ITEM ID PREFIX (itemId // 10000), learned from
the rows whose tab is unambiguous. Equipment ids already encode their slot in v83 -- 1002xxx is
a hat, 1902xxx is a cape -- so the prefix IS the category for tab 2, and the remaining tabs are
small enough to map by hand from Category.img.

    python Tools/gen_cashshop_catalog_v83.py --dry-run
    python Tools/gen_cashshop_catalog_v83.py --write --out share/cashshop-window/data/catalog.tsv
"""
from __future__ import annotations

import argparse
import collections
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)

from maple_workbench import imgbin                    # noqa: E402

# The pre-import copy. Falls back to the live file if the backup is not present, which is the
# normal case for anyone running this outside this repo.
STOCK_CANDIDATES = [
    os.path.join(ROOT, "Client/Data/Etc/Commodity.img.preCashShopCatalog.bak"),
    os.path.join(ROOT, "Client/Data/Etc/Commodity.img"),
]
CATEGORY_IMG = os.path.join(ROOT, "Client/Data/Etc/Category.img")
STRING_DIR = os.path.join(ROOT, "Client/Data/String")

DEFAULT_OUT = os.path.join(ROOT, "share/cashshop-window/data/catalog.tsv")

# Tabs and categories the window does not show. Serving a row the client cannot display leaves
# it purchasable by a crafted serial and invisible to everyone else.
#   1  New / Event -- a promotional cross-listing, not a home category. Its rows are duplicates
#                     of rows that already live in a real tab, so including it double-lists them.
#   8  the two "How to..." guide pages, which carry no merchandise at all.
DROP_TABS = {1, 8}
DROP_CATS = {
    (2, 10),   # Premium -- Category.img names it but no stock row ever lands there
    (3, 0),    # Scroll
}

# Everything is PERMANENT. The stock rows carry periods of 14 / 30 / 90 days; on a private
# server a cosmetic that silently evaporates three months after purchase is a support ticket,
# not a feature. 0 means "no expiry" to the purchase path.
PERIOD = 0


def kids(n):
    c = getattr(n, "children", None)
    if c is None:
        return []
    if isinstance(c, dict):
        return list(c.items())
    return [(getattr(x, "name", str(i)), x) for i, x in enumerate(c)]


def fields(node):
    return {k: getattr(v, "value", None) for k, v in kids(node)}


def load_categories():
    """Etc/Category.img -> {(tab, sub): name}. The authoritative v83 table."""
    root, _ = imgbin.parse_img(open(CATEGORY_IMG, "rb").read(), "Category.img")
    out = {}
    for _, node in kids(root):
        f = fields(node)
        try:
            tab = int(f.get("Category"))
            sub = int(f.get("CategorySub"))
        except (TypeError, ValueError):
            continue
        out[(tab, sub)] = str(f.get("Name") or "")
    return out


# Tab 2 is EQUIPMENT and v83 equip ids encode the slot, so the category falls straight out of
# the id prefix. These are the prefixes Category.img has a home for.
EQUIP_PREFIX = {
    100: 0,    # Hat
    101: 1,    # Face accessory
    102: 2,    # Eye accessory
    103: 9,    # Earrings. v83 has no earring category, and Ring is the only other accessory
               # bucket Category.img defines, so they are homed there rather than dropped.
    104: 4,    # Top
    105: 3,    # Overall
    106: 5,    # Bottom
    107: 6,    # Shoes
    108: 7,    # Glove
    109: 8,    # Shield. Same reasoning: no Shield category exists, and it is a hand slot.
    110: 11,   # Cape
    111: 9,    # Ring
}
# Weapons are 130..170; they all share one category.
for _p in range(130, 171):
    EQUIP_PREFIX[_p] = 8

# PET GEAR IS EQUIPMENT-FAMILY, NOT CASH-FAMILY. 180x-183x are pet equips, pet skill items and
# the label/quote rings. Missing them cost 55 rows -- the entire Mini Kargo Wings / Meso Magnet
# / Pet Label Ring range -- which is most of what the Pet Equip. category should hold.
PET_EQUIP_PREFIX = {180, 181, 182, 183}


def categorise(item_id, sn_tab):
    """(tab, sub) for a row, or None if it has no home the window can show."""
    pre3 = item_id // 10000            # 1002000 -> 100
    fam = item_id // 1000000           # the inventory family

    if fam == 1:                       # equipment
        if pre3 in PET_EQUIP_PREFIX:
            return (6, 1)              # pet gear, before the normal equip slots
        sub = EQUIP_PREFIX.get(pre3)
        return (2, sub) if sub is not None else None
    if fam == 4:                       # ETC-family cash items; no category of their own
        return (5, 2)
    if fam == 9:
        # 91xxxxx CASH PACKAGES. Deliberately dropped: they are not items, they are bundles,
        # and ItemConstants.getInventoryType returns UNDEFINED for them, so the purchase path
        # has nowhere to deliver one. Serving them would sell the player nothing.
        return None
    if fam == 5:                       # cash
        p4 = item_id // 1000
        if p4 == 5000:
            return (6, 0)              # pets
        if p4 == 5010:
            return (6, 1)              # pet equip
        if p4 in (5240, 5241):
            return (6, 2)              # pet food / "Use"
        if p4 == 5150:
            return (3, 1)              # messenger
        if p4 == 5120:
            return (3, 2)              # weather
        if p4 in (5152, 5160):
            return (5, 3)              # facial expression
        if p4 in (5210, 5211):
            return (5, 0)              # beauty parlor (hair / face coupons)
        if p4 == 5220:
            return (7, 0)              # gachapon tickets sit in Package
        if p4 == 5170:
            return (5, 6)              # character (name / look change)
        if p4 == 5190:
            return (5, 4)              # wedding
        if p4 == 5140:
            return (5, 1)              # store permit
        if p4 in (5202, 5203, 5204):
            return (5, 5)              # effect
        if p4 == 5076 or p4 == 5062:
            return (5, 2)              # game / misc cash
        return (5, 2)                  # anything else cash-family lands in Game
    return None


def load_names():
    """itemId -> name, from the client's own String.wz images."""
    names = {}
    if not os.path.isdir(STRING_DIR):
        return names
    for fn in ("Eqp.img", "Consume.img", "Ins.img", "Etc.img", "Cash.img", "Pet.img"):
        p = os.path.join(STRING_DIR, fn)
        if not os.path.exists(p):
            continue
        try:
            root, _ = imgbin.parse_img(open(p, "rb").read(), fn)
        except Exception:
            continue

        def walk(node):
            for k, v in kids(node):
                ch = dict(kids(v))
                if "name" in ch:
                    try:
                        names[int(k)] = str(getattr(ch["name"], "value", "") or "")
                    except ValueError:
                        pass
                walk(v)
        walk(root)
    return names


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--out", default=DEFAULT_OUT)
    ap.add_argument("--all", action="store_true",
                    help="include rows Nexon had switched off, not just OnSale ones")
    args = ap.parse_args()

    src = next((p for p in STOCK_CANDIDATES if os.path.exists(p)), None)
    if src is None:
        print("no Commodity.img found; looked in:", *STOCK_CANDIDATES, sep="\n  ")
        return 2
    print("source     :", os.path.relpath(src, ROOT))

    cats = load_categories()
    print("categories :", len(cats), "pairs from Category.img")
    names = load_names()
    print("names      :", len(names), "item names from String.wz")

    root, _ = imgbin.parse_img(open(src, "rb").read(), "Commodity.img")
    raw = [fields(n) for _, n in kids(root)]
    print("rows       :", len(raw), "in the commodity list")

    drop = collections.Counter()
    seen = {}
    out = []
    for f in raw:
        sn, iid = f.get("SN"), f.get("ItemId")
        if not sn or not iid:
            drop["no SN or ItemId"] += 1
            continue
        if not args.all and f.get("OnSale") != 1:
            drop["not on sale"] += 1
            continue
        pair = categorise(int(iid), int(sn) // 10000000)
        if pair is None:
            drop["no category"] += 1
            continue
        if pair[0] in DROP_TABS or pair in DROP_CATS:
            drop["tab/category not shown"] += 1
            continue
        if pair not in cats:
            # The one hard error: a pair the client has no name for cannot be rendered.
            print(f"  ABORT: item {iid} mapped to {pair}, which Category.img does not define")
            return 1
        nm = names.get(int(iid), "")
        if not nm:
            drop["no name in String.wz"] += 1
            continue
        price = int(f.get("Price") or 0)
        if price <= 0:
            drop["no price"] += 1
            continue
        # ONE ROW PER ITEM. The item id IS the key -- there is no serial. Where the source
        # lists the same item more than once (103 items carry a stale second price, and 73
        # were sold in two quantities as a bulk discount) the CHEAPER per-unit offer wins,
        # which is the one a player would have picked anyway.
        prev = seen.get(int(iid))
        cand = (int(iid), price, int(f.get("Count") or 1), pair[0], pair[1], PERIOD,
                int(f.get("Gender") if f.get("Gender") is not None else 2), nm)
        if prev is not None:
            drop["duplicate item id"] += 1
            if price / max(1, cand[2]) >= prev[1] / max(1, prev[2]):
                continue                      # the row we already have is no worse
        seen[int(iid)] = cand

    out = list(seen.values())
    out.sort(key=lambda r: (r[3], r[4], r[0]))
    print("\ndropped:")
    for k, v in drop.most_common():
        print(f"  {v:>6}  {k}")
    print(f"\nkept   : {len(out)} rows")

    per = collections.Counter((r[3], r[4]) for r in out)
    print("\nper category:")
    for pair in sorted(per):
        print(f"  ({pair[0]},{pair[1]:>2}) {cats[pair]:<22} {per[pair]:>5}")

    if args.write:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w", encoding="utf-8", newline="\n") as fh:
            fh.write("# Cash Shop catalogue, generated from the STOCK v83 Etc/Commodity.img.\n")
            fh.write("# The ITEM ID is the key: one row per item, no serial numbers.\n")
            fh.write("# itemId\tprice\tcount\ttab\tcategory\tperiod\tgender\tname\n")
            for r in out:
                fh.write("\t".join(str(x) for x in r) + "\n")
        print("\nwrote", os.path.relpath(args.out, ROOT), f"({len(out)} rows)")
    else:
        print("\n(dry run; pass --write to emit)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
