#!/usr/bin/env python3
"""
Generate application icons for USB Groove.
Creates a music-note-on-USB icon in all required sizes.

Output:
  - icons/app.ico          (Windows: 16, 32, 48, 256)
  - icons/app_16.png       (macOS menu bar 1x)
  - icons/app_32.png       (macOS menu bar 2x)
  - icons/app_256.png      (macOS app icon base)

Usage:
  python3 icons/generate_icons.py
"""

from PIL import Image, ImageDraw
import struct, io, os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def draw_icon(size):
    """Draw a USB Groove icon at the given size."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    s = size  # shorthand
    pad = max(1, s // 16)

    # -- Background: rounded rectangle (purple/indigo gradient feel) --
    bg_color = (88, 86, 214)  # indigo/purple
    corner = max(2, s // 6)
    draw.rounded_rectangle(
        [pad, pad, s - pad - 1, s - pad - 1],
        radius=corner,
        fill=bg_color,
    )

    # -- USB connector shape (top portion) --
    usb_color = (255, 255, 255, 220)
    # USB body (rectangle in upper-center)
    uw = max(2, s * 5 // 16)   # usb width
    uh = max(2, s * 3 // 16)   # usb height
    ux = (s - uw) // 2
    uy = pad + max(1, s // 8)
    draw.rounded_rectangle(
        [ux, uy, ux + uw, uy + uh],
        radius=max(1, s // 32),
        fill=usb_color,
    )
    # USB connector prongs (two small rectangles at top)
    prong_w = max(1, uw // 5)
    prong_h = max(1, uh // 3)
    prong_gap = max(1, uw // 5)
    px1 = ux + (uw - 2 * prong_w - prong_gap) // 2
    px2 = px1 + prong_w + prong_gap
    py = uy - prong_h + 1
    draw.rectangle([px1, py, px1 + prong_w, uy], fill=usb_color)
    draw.rectangle([px2, py, px2 + prong_w, uy], fill=usb_color)

    # -- Music note (lower portion) --
    note_color = (255, 255, 255, 240)
    # Note head (filled ellipse)
    nh_w = max(2, s * 3 // 16)  # note head width
    nh_h = max(2, s * 2 // 16)  # note head height
    nh_x = s // 2 - nh_w // 2 + max(0, s // 16)
    nh_y = s - pad - max(2, s // 6) - nh_h
    draw.ellipse(
        [nh_x, nh_y, nh_x + nh_w, nh_y + nh_h],
        fill=note_color,
    )
    # Note stem (vertical line from note head up)
    stem_w = max(1, s // 16)
    stem_x = nh_x + nh_w - stem_w
    stem_top = uy + uh + max(1, s // 16)
    draw.rectangle(
        [stem_x, stem_top, stem_x + stem_w, nh_y + nh_h // 2],
        fill=note_color,
    )
    # Note flag (small curve at top of stem)
    flag_w = max(2, s * 2 // 16)
    flag_h = max(2, s * 2 // 16)
    draw.arc(
        [stem_x, stem_top, stem_x + flag_w, stem_top + flag_h],
        start=270, end=45,
        fill=note_color,
        width=max(1, s // 16),
    )

    return img


def draw_menubar_icon(size):
    """Draw a monochrome template icon for macOS menu bar."""
    img = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    s = size
    color = (0, 0, 0, 255)  # black for template image (macOS handles tinting)

    # Simple music note
    nh_w = max(2, s * 4 // 16)
    nh_h = max(2, s * 3 // 16)
    nh_x = s // 2 - nh_w // 2
    nh_y = s - max(2, s // 5) - nh_h
    draw.ellipse([nh_x, nh_y, nh_x + nh_w, nh_y + nh_h], fill=color)

    # Stem
    stem_w = max(1, s // 8)
    stem_x = nh_x + nh_w - stem_w
    stem_top = max(1, s // 6)
    draw.rectangle(
        [stem_x, stem_top, stem_x + stem_w, nh_y + nh_h // 2],
        fill=color,
    )

    # Flag
    flag_w = max(2, s * 3 // 16)
    flag_h = max(2, s * 3 // 16)
    draw.arc(
        [stem_x, stem_top, stem_x + flag_w, stem_top + flag_h],
        start=270, end=60,
        fill=color,
        width=max(1, s // 8),
    )

    # Small USB indicator (two prongs at top-left)
    pw = max(1, s // 8)
    ph = max(1, s // 6)
    px = max(1, s // 6)
    py = max(1, s // 6)
    draw.rectangle([px, py, px + pw, py + ph], fill=color)
    draw.rectangle([px + pw + max(1, s // 16), py, px + 2 * pw + max(1, s // 16), py + ph], fill=color)
    # USB body
    bx = px - max(0, s // 32)
    bw = 2 * pw + max(1, s // 16) + max(0, s // 16)
    bh = max(1, s // 8)
    draw.rectangle([bx, py + ph, bx + bw, py + ph + bh], fill=color)

    return img


def save_ico(images, path):
    """Save multiple PIL images as a .ico file."""
    # Use Pillow's built-in ICO saving
    images[0].save(
        path,
        format="ICO",
        sizes=[(img.width, img.height) for img in images],
        append_images=images[1:],
    )


def main():
    # Generate app icons at all sizes
    sizes = [16, 32, 48, 256]
    icons = []
    for sz in sizes:
        icon = draw_icon(sz)
        icons.append(icon)
        icon.save(os.path.join(SCRIPT_DIR, f"app_{sz}.png"))
        print(f"  Created app_{sz}.png")

    # Save Windows .ico
    ico_path = os.path.join(SCRIPT_DIR, "app.ico")
    save_ico(icons, ico_path)
    print(f"  Created app.ico")

    # Generate macOS menu bar template images
    for sz, suffix in [(16, ""), (32, "@2x")]:
        img = draw_menubar_icon(sz)
        name = f"StatusBarIconTemplate{suffix}.png"
        img.save(os.path.join(SCRIPT_DIR, name))
        print(f"  Created {name}")

    print("\nDone! All icons generated in icons/")


if __name__ == "__main__":
    main()
