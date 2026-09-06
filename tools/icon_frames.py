#!/usr/bin/env python3
"""Rebuilds kurva.ico with every frame size Windows asks for.

Explorer, the taskbar, Alt+Tab and the notification area draw the icon at 16, 20, 24, 32,
40, 48 pixels and more, depending on the view and the display scaling. A size the .ico does
not have is made by shrinking the nearest larger frame pixel by pixel, which looks jagged.
This script takes the largest frame as the master and produces every size from it with a
Lanczos filter on premultiplied alpha, so the edges stay smooth and free of dark fringes.
The 256 frame is stored PNG-compressed, as Windows does with its own icons.

    python tools/icon_frames.py kurva.ico

Needs Python 3 only. Running it again on its own output gives the same file.
"""

import math
import struct
import sys
import zlib
from operator import mul

# Small icons at 100/125/150/200 % scaling (16, 20, 24, 32), the taskbar (24 DIP: 30, 36),
# large icons (32 DIP: 40, 48, 64), Explorer's medium view (48 DIP: 60, 72, 96), 128 and 256.
SIZES = [16, 20, 24, 30, 32, 36, 40, 48, 60, 64, 72, 96, 128, 256]
PNG_FROM = 256  # Frames of this size and up are stored as PNG.
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


class Frame:
    """An image as four flat channel lists (r, g, b, a) of 0..255 floats, rows top-down."""

    def __init__(self, width, height, channels):
        self.width = width
        self.height = height
        self.channels = channels


# --- PNG -----------------------------------------------------------------------------------


def decode_png(data):
    if not data.startswith(PNG_SIGNATURE):
        raise ValueError("not a PNG")
    position = len(PNG_SIGNATURE)
    width = height = 0
    color_type = 0
    idat = bytearray()
    while position < len(data):
        length, kind = struct.unpack(">I4s", data[position:position + 8])
        body = data[position + 8:position + 8 + length]
        position += 12 + length
        if kind == b"IHDR":
            width, height, depth, color_type, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or color_type not in (2, 6) or interlace != 0:
                raise ValueError("only 8-bit RGB/RGBA non-interlaced PNG frames are supported")
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    samples = 4 if color_type == 6 else 3
    stride = width * samples
    raw = zlib.decompress(bytes(idat))
    previous = bytearray(stride)
    channels = [[], [], [], []]
    for y in range(height):
        start = y * (stride + 1)
        filter_type = raw[start]
        line = bytearray(raw[start + 1:start + 1 + stride])
        for i in range(stride):
            left = line[i - samples] if i >= samples else 0
            up = previous[i]
            up_left = previous[i - samples] if i >= samples else 0
            if filter_type == 1:
                line[i] = (line[i] + left) & 0xFF
            elif filter_type == 2:
                line[i] = (line[i] + up) & 0xFF
            elif filter_type == 3:
                line[i] = (line[i] + (left + up) // 2) & 0xFF
            elif filter_type == 4:
                p = left + up - up_left
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - up_left)
                predictor = left if pa <= pb and pa <= pc else (up if pb <= pc else up_left)
                line[i] = (line[i] + predictor) & 0xFF
        for x in range(width):
            pixel = line[x * samples:x * samples + samples]
            channels[0].append(float(pixel[0]))
            channels[1].append(float(pixel[1]))
            channels[2].append(float(pixel[2]))
            channels[3].append(float(pixel[3]) if samples == 4 else 255.0)
        previous = line
    return Frame(width, height, channels)


def encode_png(frame):
    raw = bytearray()
    r, g, b, a = frame.channels
    for y in range(frame.height):
        raw.append(0)  # Filter type: none. The deflate step still compresses flat art well.
        base = y * frame.width
        for x in range(base, base + frame.width):
            raw += bytes((round(r[x]), round(g[x]), round(b[x]), round(a[x])))

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    header = struct.pack(">IIBBBBB", frame.width, frame.height, 8, 6, 0, 0, 0)
    return PNG_SIGNATURE + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")


# --- ICO -----------------------------------------------------------------------------------


def decode_bmp_frame(data):
    (header_size, width, double_height, _, bit_count, compression) = struct.unpack("<IiiHHI", data[:20])
    if header_size != 40 or bit_count != 32 or compression != 0:
        raise ValueError("only 32-bit uncompressed BMP frames are supported")
    height = double_height // 2
    channels = [[], [], [], []]
    for y in range(height - 1, -1, -1):  # Rows are stored bottom-up.
        row = data[header_size + y * width * 4:header_size + (y + 1) * width * 4]
        channels[0].extend(float(v) for v in row[2::4])
        channels[1].extend(float(v) for v in row[1::4])
        channels[2].extend(float(v) for v in row[0::4])
        channels[3].extend(float(v) for v in row[3::4])
    return Frame(width, height, channels)


def encode_bmp_frame(frame):
    width, height = frame.width, frame.height
    r, g, b, a = frame.channels
    pixels = bytearray()
    mask = bytearray()
    mask_stride = ((width + 31) // 32) * 4
    for y in range(height - 1, -1, -1):
        base = y * width
        row_mask = bytearray(mask_stride)
        for x in range(width):
            alpha = round(a[base + x])
            pixels += bytes((round(b[base + x]), round(g[base + x]), round(r[base + x]), alpha))
            if alpha == 0:
                row_mask[x // 8] |= 0x80 >> (x % 8)  # AND mask: 1 = transparent, for non-alpha paths.
        mask += row_mask
    header = struct.pack("<IiiHHIIiiII", 40, width, height * 2, 1, 32, 0, len(pixels) + len(mask), 0, 0, 0, 0)
    return header + pixels + mask


def read_ico(path):
    data = open(path, "rb").read()
    _, kind, count = struct.unpack("<HHH", data[:6])
    if kind != 1:
        raise ValueError("not an icon file")
    frames = []
    for index in range(count):
        entry = data[6 + 16 * index:6 + 16 * (index + 1)]
        _, _, _, _, _, _, size, offset = struct.unpack("<BBBBHHII", entry)
        body = data[offset:offset + size]
        frames.append(decode_png(body) if body.startswith(PNG_SIGNATURE) else decode_bmp_frame(body))
    return frames


def write_ico(path, frames):
    bodies = []
    for frame in frames:
        bodies.append(encode_png(frame) if frame.width >= PNG_FROM else encode_bmp_frame(frame))
    directory = struct.pack("<HHH", 0, 1, len(frames))
    offset = 6 + 16 * len(frames)
    for frame, body in zip(frames, bodies):
        size = 0 if frame.width >= 256 else frame.width
        directory += struct.pack("<BBBBHHII", size, size, 0, 0, 1, 32, len(body), offset)
        offset += len(body)
    open(path, "wb").write(directory + b"".join(bodies))


# --- Resampling ------------------------------------------------------------------------------


def lanczos(x, lobes=3):
    if x == 0:
        return 1.0
    if abs(x) >= lobes:
        return 0.0
    px = math.pi * x
    return lobes * math.sin(px) * math.sin(px / lobes) / (px * px)


def weights(source, target):
    """For every target index: the first source index it draws from and the normalized taps."""
    scale = source / target
    stretch = max(scale, 1.0)  # Widen the kernel when shrinking, so every source pixel counts.
    result = []
    for i in range(target):
        center = (i + 0.5) * scale - 0.5
        first = max(0, math.ceil(center - 3 * stretch))
        last = min(source - 1, math.floor(center + 3 * stretch))
        taps = [lanczos((j - center) / stretch) for j in range(first, last + 1)]
        total = sum(taps)
        result.append((first, [t / total for t in taps]))
    return result


def resample(frame, size):
    if size == frame.width and size == frame.height:
        return frame
    r, g, b, a = frame.channels
    # Premultiplied: the color of transparent pixels must not bleed into the edges.
    scale_alpha = [v / 255.0 for v in a]
    premultiplied = [[c * s for c, s in zip(channel, scale_alpha)] for channel in (r, g, b)] + [list(a)]

    horizontal = weights(frame.width, size)
    vertical = weights(frame.height, size)
    output = []
    for channel in premultiplied:
        rows = []
        for y in range(frame.height):
            base = y * frame.width
            rows.append([sum(map(mul, channel[base + first:base + first + len(taps)], taps)) for first, taps in horizontal])
        columns = [[row[x] for row in rows] for x in range(size)]
        result = [0.0] * (size * size)
        for x in range(size):
            column = columns[x]
            for y, (first, taps) in enumerate(vertical):
                result[y * size + x] = sum(map(mul, column[first:first + len(taps)], taps))
        output.append(result)

    alpha = [min(255.0, max(0.0, v)) for v in output[3]]
    channels = []
    for channel in output[:3]:
        channels.append([min(255.0, max(0.0, c * 255.0 / v)) if v > 0.5 else 0.0 for c, v in zip(channel, alpha)])
    channels.append(alpha)
    return Frame(size, size, channels)


def main(argv):
    if len(argv) != 2:
        print(__doc__)
        return 2
    path = argv[1]
    frames = read_ico(path)
    master = max(frames, key=lambda frame: frame.width)
    if master.width != master.height:
        raise ValueError("the largest frame is not square")
    print(f"{path}: {len(frames)} frame(s), master {master.width}x{master.height}")
    rebuilt = []
    for size in SIZES:
        if size > master.width:
            continue
        rebuilt.append(resample(master, size))
        print(f"  {size}x{size}")
    write_ico(path, rebuilt)
    print(f"written: {len(rebuilt)} frame(s)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
