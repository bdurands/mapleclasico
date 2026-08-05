#!/usr/bin/env python3
"""Bake the Cash Shop window's two buttons, labels and all, into UI/CashShop.img.

WHY. The window used to 3-slice Shop/BtBuy to width at runtime and letter it in MapleUI
Button04, which meant shipping a TTF for exactly two strings -- "BUY" and "BUY CART". Every
other user of that face went away as the painted plate absorbed the window's labels, so the
font was being carried for two words.

Baking them removes the dependency outright: the face is used HERE, at authoring time, and the
client blits finished art. It is also strictly more faithful, because the label lands on the
same pixels in every state instead of being re-centred by whatever font the client resolves.

SOURCE   UI/UIWindow.img/Shop/BtBuy -- the NPC store's own purchase button, 80x18, four states.
         Sliced to 76 with caps (3,3) and a fill column at x=3, which measures a per-channel
         seam error of exactly zero. Its baked "BUY ITEM" is erased by the repeat, then the real
         label goes on top.

TARGET   UI/CashShop.img/Base/BtBuy/<state>  and  Base/BtCart/<state>
         state in normal / pressed / disabled / mouseOver.

    python Tools/make_cashshop_buttons.py --dry-run
    python Tools/make_cashshop_buttons.py --write
"""
from __future__ import annotations

import argparse
import os
import shutil
import sys
import zlib

from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)

from maple_workbench import imgbin, canvas as cv                # noqa: E402
from maple_workbench.imgbin import CanvasData, Node             # noqa: E402

UIWINDOW = os.path.join(ROOT, "Client/Data/UI/UIWindow.img")
TARGET = os.path.join(ROOT, "Client/Data/UI/CashShop.img")
FONT = os.path.join(ROOT, "Client/MapleUI-Button04.ttf")

BTN_W, ART_W, ART_H = 76, 80, 18
CAP_L, CAP_R, FILL_X = 3, 3, 3
STATES = ("normal", "pressed", "disabled", "mouseOver")

# White with a 1px dark outline is how every stock button letters itself; the disabled state
# drops to a dark brown, which is what the drawn version used and measures 4.4:1 on the
# desaturated face where the window's usual disabled grey vanishes at 1.2:1.
INK = {"normal": (255, 255, 255), "pressed": (255, 255, 255),
       "mouseOver": (255, 255, 255), "disabled": (0x55, 0x33, 0x22)}
OUTLINE = (0x33, 0x22, 0x11)

BUTTONS = [("BtBuy", "BUY"), ("BtCart", "BUY CART")]


def kids(n):
    return n.children if n is not None else []


def child(n, name):
    for c in kids(n):
        if c.name == name:
            return c
    return None


def set_child(parent, name, node):
    for i, x in enumerate(parent.children):
        if x.name == name:
            parent.children[i] = node
            return
    parent.children.append(node)


def slice_to(src, w):
    im = Image.new("RGBA", (w, ART_H), (0, 0, 0, 0))
    lc = src.crop((0, 0, CAP_L, ART_H))
    im.paste(lc, (0, 0), lc)
    fc = src.crop((FILL_X, 0, FILL_X + 1, ART_H))
    for x in range(CAP_L, w - CAP_R):
        im.paste(fc, (x, 0), fc)
    rc = src.crop((ART_W - CAP_R, 0, ART_W, ART_H))
    im.paste(rc, (w - CAP_R, 0), rc)
    return im


def letter(im, text, state, font):
    d = ImageDraw.Draw(im)
    bb = d.textbbox((0, 0), text, font=font)
    tw, th = bb[2] - bb[0], bb[3] - bb[1]
    # Rows 5..13 of an 18-tall canvas is the glyph band every stock button uses.
    x = (im.width - tw) // 2 - bb[0]
    y = 5 - bb[1]
    if state != "disabled":
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if dx or dy:
                    d.text((x + dx, y + dy), text, font=font, fill=OUTLINE + (255,))
    d.text((x, y), text, font=font, fill=INK[state] + (255,))
    return im


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--preview", default="")
    args = ap.parse_args()

    if not os.path.exists(FONT):
        print("ABORT: font not found at", FONT)
        return 1
    font = ImageFont.truetype(FONT, 15)

    ui, _ = imgbin.parse_img(open(UIWINDOW, "rb").read(), "UIWindow.img")
    shop = child(child(ui, "Shop"), "BtBuy")
    if shop is None:
        print("ABORT: Shop/BtBuy not found")
        return 1

    built = {}
    for node_name, label in BUTTONS:
        for st in STATES:
            s = child(child(shop, st), "0")
            if s is None or s.canvas is None:
                print(f"ABORT: Shop/BtBuy/{st}/0 missing")
                return 1
            im = letter(slice_to(cv.decode_canvas(s.canvas).convert("RGBA"), BTN_W),
                        label, st, font)
            built[(node_name, st)] = im
    print(f"built {len(built)} canvases, {BTN_W}x{ART_H}")

    if args.preview:
        sheet = Image.new("RGB", (BTN_W * 4 + 30, ART_H * 2 + 18), (0x99, 0xB0, 0xC6))
        for r, (nn, _lab) in enumerate(BUTTONS):
            for c_, st in enumerate(STATES):
                b = built[(nn, st)]
                sheet.paste(b, (6 + c_ * (BTN_W + 6), 6 + r * (ART_H + 6)), b)
        sheet.resize((sheet.width * 3, sheet.height * 3), Image.NEAREST).save(args.preview)
        print("preview ->", args.preview)

    root, key = imgbin.parse_img(open(TARGET, "rb").read(), "CashShop.img")
    base = child(root, "Base")
    for node_name, _lab in BUTTONS:
        grp = child(base, node_name)
        if grp is None:
            grp = Node("imgdir", node_name)
            set_child(base, node_name, grp)
        for st in STATES:
            im = built[(node_name, st)]
            b, g, r, a = im.split()[2], im.split()[1], im.split()[0], im.split()[3]
            raw = Image.merge("RGBA", (b, g, r, a)).tobytes()
            cn = Node("canvas", st)
            cn.canvas = CanvasData(im.width, im.height, 2, 0,
                                   zlib_data=zlib.compress(raw, 9))
            cn.children = [Node("vector", "origin", (0, 0))]
            set_child(grp, st, cn)

    out = imgbin.write_img(root, key)
    chk, _ = imgbin.parse_img(out, "CashShop.img")
    cb = child(chk, "Base")
    for node_name, _lab in BUTTONS:
        for st in STATES:
            n = child(child(cb, node_name), st)
            if n is None or n.canvas is None:
                print(f"ABORT: {node_name}/{st} did not round-trip")
                return 1
            back = cv.decode_canvas(n.canvas).convert("RGBA")
            if back.tobytes() != built[(node_name, st)].tobytes():
                print(f"ABORT: {node_name}/{st} pixels differ after round-trip")
                return 1
    print("verify: all 8 round-trip pixel-for-pixel")
    if child(child(cb, "WndBg"), None) is None and child(cb, "WndBg") is None:
        print("ABORT: Base/WndBg lost")
        return 1
    for i in range(3):
        if child(child(child(cb, "Base") or cb, "Preview"), str(i)) is None:
            print(f"ABORT: Base/Preview/{i} lost")
            return 1
    print("verify: Base/WndBg and Base/Preview/0..2 intact")

    if not args.write:
        print("\n(dry run; pass --write to install)")
        return 0
    bak = TARGET + ".preButtons.bak"
    if not os.path.exists(bak):
        shutil.copy2(TARGET, bak)
        print("backup:", os.path.relpath(bak, ROOT))
    with open(TARGET, "wb") as fh:
        fh.write(out)
    print("wrote :", os.path.relpath(TARGET, ROOT), f"({len(out):,} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
