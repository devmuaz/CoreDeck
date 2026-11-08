#!/usr/bin/env python3
"""Rebuild the platform icons from the two 1024 masters.

- assets/icons/main.png is the square macOS picture. macOS rounds that
square itself, so the Mac icon must not be pre-rounded.

- assets/icons/main-rounded.png is the Windows and Linux picture. Those
systems do not share one mask, so the rounded corners are already in
the artwork and the outside of the curve is transparent.

Downscaling averages coverage in straight-alpha space. A transparent
corner stays transparent, and the fringe keeps the background color
instead of turning into a dark halo.
"""

import shutil
import struct
import subprocess
import tempfile
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ICONS = ROOT / "assets" / "icons"
MAC_MASTER = ICONS / "main.png"
ROUNDED_MASTER = ICONS / "main-rounded.png"
HICOLOR = ROOT / "packaging" / "linux" / "hicolor"
LINUX_SIZES = (16, 32, 48, 64, 128, 256, 512, 1024)
WINDOWS_SIZES = (16, 20, 24, 32, 40, 48, 64, 128, 256)
# iconset filename, pixel size. Pairs that share a size are written twice.
MAC_ICONSET = (
    ("icon_16x16.png", 16),
    ("icon_16x16@2x.png", 32),
    ("icon_32x32.png", 32),
    ("icon_32x32@2x.png", 64),
    ("icon_128x128.png", 128),
    ("icon_128x128@2x.png", 256),
    ("icon_256x256.png", 256),
    ("icon_256x256@2x.png", 512),
    ("icon_512x512.png", 512),
    ("icon_512x512@2x.png", 1024),
)


def decode_png(path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise SystemExit(f"{path} is not a PNG")
    offset = 8
    width = height = 0
    color = None
    compressed = b""
    while offset + 8 <= len(data):
        length = struct.unpack(">I", data[offset : offset + 4])[0]
        kind = data[offset + 4 : offset + 8]
        chunk = data[offset + 8 : offset + 8 + length]
        if kind == b"IHDR":
            width, height, bit, color = struct.unpack(">IIBB", chunk[:10])
            if bit != 8 or color != 6 or chunk[12] != 0:
                raise SystemExit(f"{path} must be 8-bit non-interlaced RGBA")
        elif kind == b"IDAT":
            compressed += chunk
        offset += 12 + length
        if kind == b"IEND":
            break
    raw = zlib.decompress(compressed)
    stride = width * 4
    rows = []
    previous = bytearray(stride)
    index = 0
    for _y in range(height):
        filter_type = raw[index]
        index += 1
        row = bytearray(raw[index : index + stride])
        index += stride
        for x in range(stride):
            left = row[x - 4] if x >= 4 else 0
            up = previous[x]
            up_left = previous[x - 4] if x >= 4 else 0
            if filter_type == 0:
                predictor = 0
            elif filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = up
            elif filter_type == 3:
                predictor = (left + up) // 2
            elif filter_type == 4:
                estimate = left + up - up_left
                distances = (
                    abs(estimate - left),
                    abs(estimate - up),
                    abs(estimate - up_left),
                )
                predictor = (left, up, up_left)[distances.index(min(distances))]
            else:
                raise SystemExit(f"unsupported PNG filter {filter_type}")
            row[x] = (row[x] + predictor) & 255
        rows.append(row)
        previous = row
    if width != 1024 or height != 1024:
        raise SystemExit(f"{path} is {width}x{height}, expected 1024x1024")
    return rows


def png_bytes(width, height, rgba):
    def chunk(kind, payload):
        return (
            struct.pack(">I", len(payload))
            + kind
            + payload
            + struct.pack(">I", zlib.crc32(kind + payload) & 0xFFFFFFFF)
        )

    raw = b"".join(
        b"\x00" + bytes(rgba[y * width * 4 : (y + 1) * width * 4]) for y in range(height)
    )
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def write_png(path, width, height, rgba):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(png_bytes(width, height, rgba))


def resize(rows, width, height):
    """Coverage average. Color is weighted by alpha so clear corners stay clear."""
    if width == 1024 and height == 1024:
        return b"".join(rows)
    source_w = source_h = 1024
    output = bytearray(width * height * 4)
    for y in range(height):
        y0 = y * source_h / height
        y1 = (y + 1) * source_h / height
        y_start = int(y0)
        y_stop = int(y1 - 1e-9)
        for x in range(width):
            x0 = x * source_w / width
            x1 = (x + 1) * source_w / width
            x_start = int(x0)
            x_stop = int(x1 - 1e-9)
            sum_r = sum_g = sum_b = sum_a = 0.0
            for sy in range(y_start, y_stop + 1):
                y_weight = min(y1, sy + 1) - max(y0, sy)
                row = rows[sy]
                for sx in range(x_start, x_stop + 1):
                    weight = y_weight * (min(x1, sx + 1) - max(x0, sx))
                    offset = sx * 4
                    red, green, blue, alpha = row[offset : offset + 4]
                    sum_a += alpha * weight
                    sum_r += red * alpha * weight
                    sum_g += green * alpha * weight
                    sum_b += blue * alpha * weight
            area = (x1 - x0) * (y1 - y0)
            destination = (y * width + x) * 4
            if sum_a <= 0:
                continue
            output[destination] = min(255, round(sum_r / sum_a))
            output[destination + 1] = min(255, round(sum_g / sum_a))
            output[destination + 2] = min(255, round(sum_b / sum_a))
            output[destination + 3] = min(255, round(sum_a / area))
    return output


def mac_mark_clears_mask(rows):
    """The light mark must sit inside the squircle macOS cuts from a square icon."""
    clipped = 0
    for y, row in enumerate(rows):
        for x in range(1024):
            red = row[x * 4]
            alpha = row[x * 4 + 3]
            if alpha < 128 or red <= 100:
                continue
            nx = (x + 0.5 - 512) / 512
            ny = (y + 0.5 - 512) / 512
            if abs(nx) ** 5 + abs(ny) ** 5 > 1:
                clipped += 1
    return clipped == 0


def bmp_ico_image(size, rgba):
    """Classic 32-bit DIB. Windows Installer reads these sizes reliably."""
    xor = bytearray(size * size * 4)
    mask_stride = ((size + 31) // 32) * 4
    mask = bytearray(mask_stride * size)
    for y in range(size):
        source_y = size - 1 - y
        for x in range(size):
            offset = (source_y * size + x) * 4
            red, green, blue, alpha = rgba[offset : offset + 4]
            pixel = (y * size + x) * 4
            xor[pixel : pixel + 4] = bytes((blue, green, red, alpha))
            if alpha == 0:
                mask[y * mask_stride + x // 8] |= 0x80 >> (x % 8)
    header = struct.pack(
        "<IiiHHIIiiII",
        40,
        size,
        size * 2,
        1,
        32,
        0,
        len(xor) + len(mask),
        0,
        0,
        0,
        0,
    )
    return header + xor + mask


def write_ico(path, images):
    """Smaller sizes are bitmaps. 256 is a PNG, which is the ICO rule for that size."""
    blobs = []
    for size, rgba in images:
        if size >= 256:
            blobs.append(png_bytes(size, size, rgba))
        else:
            blobs.append(bmp_ico_image(size, rgba))
    header = struct.pack("<HHH", 0, 1, len(images))
    directory = bytearray()
    offset = 6 + 16 * len(images)
    payload = b""
    for (size, _rgba), blob in zip(images, blobs):
        stored = 0 if size == 256 else size
        directory += struct.pack("<BBBBHHII", stored, stored, 0, 0, 1, 32, len(blob), offset)
        payload += blob
        offset += len(blob)
    path.write_bytes(header + directory + payload)


def render():
    mac_rows = decode_png(MAC_MASTER)
    rounded_rows = decode_png(ROUNDED_MASTER)
    if not mac_mark_clears_mask(mac_rows):
        raise SystemExit("macOS artwork extends past the system squircle mask")

    mac_cache = {}
    iconset = Path(tempfile.mkdtemp(prefix="coredeck-iconset-")) / "CoreDeck.iconset"
    iconset.mkdir()
    try:
        for name, size in MAC_ICONSET:
            if size not in mac_cache:
                mac_cache[size] = resize(mac_rows, size, size)
            write_png(iconset / name, size, size, mac_cache[size])
        subprocess.run(
            ["iconutil", "--convert", "icns", "--output", str(ICONS / "icon.icns"), str(iconset)],
            check=True,
        )
    finally:
        shutil.rmtree(iconset.parent)

    windows = [(size, resize(rounded_rows, size, size)) for size in WINDOWS_SIZES]
    write_ico(ICONS / "icon.ico", windows)

    shutil.copyfile(ROUNDED_MASTER, ICONS / "icon.png")
    for size in LINUX_SIZES:
        destination = HICOLOR / f"{size}x{size}" / "apps" / "com.devmuaz.coredeck.png"
        if size == 1024:
            shutil.copyfile(ROUNDED_MASTER, destination)
        else:
            write_png(destination, size, size, resize(rounded_rows, size, size))

    fringe = resize(rounded_rows, 32, 32)
    dark_fringe = 0
    for index in range(0, len(fringe), 4):
        red, _green, _blue, alpha = fringe[index : index + 4]
        if 0 < alpha < 250 and red < 20:
            dark_fringe += 1
    if dark_fringe:
        raise SystemExit(f"rounded 32px icon has {dark_fringe} dark fringe pixels")

    print(f"icon.icns  { (ICONS / 'icon.icns').stat().st_size } bytes")
    print(f"icon.ico   { (ICONS / 'icon.ico').stat().st_size } bytes")
    print(f"icon.png   { (ICONS / 'icon.png').stat().st_size } bytes")


if __name__ == "__main__":
    render()
