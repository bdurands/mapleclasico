#!/usr/bin/env python3
"""Import the hand-painted Cash Shop window background into UI/CashShop.img.

The window used to draw its whole plate from primitives, which is why it survived at any size.
A painted plate replaces the frame, the tab bands, the section-head strips and the well outlines
with one blit, so the DLL only has to draw what actually CHANGES: text, icons, tab selection,
the avatar and the buttons.

TARGET  UI/CashShop.img/Base/WndBg
        Base/ already holds this window's three preview backdrops (Preview/0..2), so the new
        node sits with its siblings rather than inventing a tree.

FORMAT  pixformat 2 (BGRA8888, 32bpp), magLevel 0 -- lossless and the simplest thing the client
        decodes. The art has real alpha (the rounded corners are cut out of it), so a 16-bit
        format would band the anti-aliased edges.

        NOT format 517/513: that is 16bpp with a magnification level, and re-encoding a painted
        plate through it loses the corner anti-aliasing that makes the window read as rounded.

    python Tools/import_cashshop_bg.py --src art/backgrnd.png --dry-run
    python Tools/import_cashshop_bg.py --src ... --write
"""
from __future__ import annotations

import argparse
import os
import shutil
import sys
import zlib

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
sys.path.insert(0, HERE)

from maple_workbench import imgbin                      # noqa: E402
from maple_workbench.imgbin import CanvasData, Node     # noqa: E402

TARGET = os.path.join(ROOT, "Client/Data/UI/CashShop.img")
NODE_PARENT = "Base"
NODE_NAME = "WndBg"


def child(n, name):
    """Node.children is a LIST of Nodes, not a dict -- the writer iterates it and reads
    .name off each item, so a dict here fails deep inside _prop_list."""
    if n is None:
        return None
    for c in n.children:
        if c.name == name:
            return c
    return None


def set_child(parent, name, node):
    """Attach `node` under `parent`, replacing any existing child of that name."""
    for i, x in enumerate(parent.children):
        if x.name == name:
            parent.children[i] = node
            return
    parent.children.append(node)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True)
    ap.add_argument("--write", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    im = Image.open(args.src).convert("RGBA")
    W, H = im.size
    print(f"source : {args.src}  {W}x{H}")

    # BGRA, bottom-up? No: wz canvases are top-down BGRA, same order PIL gives after a swap.
    b, g, r, a = im.split()[2], im.split()[1], im.split()[0], im.split()[3]
    bgra = Image.merge("RGBA", (b, g, r, a)).tobytes()
    assert len(bgra) == W * H * 4, "unexpected raw size"
    packed = zlib.compress(bgra, 9)
    print(f"raw    : {len(bgra):,} bytes -> zlib {len(packed):,} "
          f"({100 * len(packed) // len(bgra)}%)")

    root, key = imgbin.parse_img(open(TARGET, "rb").read(), "CashShop.img")
    base = child(root, NODE_PARENT)
    if base is None:
        print(f"ABORT: {NODE_PARENT}/ not found in CashShop.img")
        return 1
    existing = child(base, NODE_NAME)
    print(f"target : {NODE_PARENT}/{NODE_NAME} "
          f"({'REPLACING existing' if existing is not None else 'new node'})")

    canvas = Node("canvas", NODE_NAME)
    canvas.canvas = CanvasData(W, H, 2, 0, zlib_data=packed)
    # Every stock canvas carries an origin; the window blits at (0,0) but a missing origin is
    # the kind of omission that makes other tools reject the node.
    canvas.children = [Node("vector", "origin", (0, 0))]
    set_child(base, NODE_NAME, canvas)

    out = imgbin.write_img(root, key)
    print(f"rebuilt: {len(out):,} bytes (was {os.path.getsize(TARGET):,})")

    # Round-trip before touching anything on disk: if it does not parse back, nothing is written.
    chk, _ = imgbin.parse_img(out, "CashShop.img")
    got = child(child(chk, NODE_PARENT), NODE_NAME)
    if got is None or got.canvas is None:
        print("ABORT: the rebuilt image does not parse back with the new node")
        return 1
    if (got.canvas.width, got.canvas.height) != (W, H):
        print(f"ABORT: round-trip size mismatch {got.canvas.width}x{got.canvas.height}")
        return 1
    from maple_workbench import canvas as cv
    back = cv.decode_canvas(got.canvas).convert("RGBA")
    if back.size != im.size or back.tobytes() != im.tobytes():
        diff = sum(1 for p, q in zip(back.getdata(), im.getdata()) if p != q)
        print(f"ABORT: round-trip pixels differ ({diff:,} of {W * H})")
        return 1
    print("verify : round-trips pixel-for-pixel")

    # The three preview backdrops must still be readable; a writer that silently drops or
    # corrupts siblings is the real risk when rebuilding a whole .img.
    for i in range(3):
        n = child(child(chk, "Base"), "Preview")
        n = child(n, str(i)) if n is not None else None
        if n is None or n.canvas is None:
            print(f"ABORT: Base/Preview/{i} did not survive the rebuild")
            return 1
    print("verify : Base/Preview/0..2 intact")

    if not args.write:
        print("\n(dry run; pass --write to install)")
        return 0

    bak = TARGET + ".preWndBg.bak"
    if not os.path.exists(bak):
        shutil.copy2(TARGET, bak)
        print("backup :", os.path.relpath(bak, ROOT))
    with open(TARGET, "wb") as fh:
        fh.write(out)
    print("wrote  :", os.path.relpath(TARGET, ROOT))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
