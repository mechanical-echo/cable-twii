#!/usr/bin/env python3
import argparse
import hashlib
import struct
from pathlib import Path

RGB565 = 4
U8_MAGIC = 0x55AA382D
TPL_MAGIC = 0x0020AF30
IMET_PADDING = 0x40
IMET_HASH_SIZE = 0x600
IMET_VERSION = 3
IMET_LANGUAGES = 10
IMET_NAME_BYTES = 84
PANE = "Picture"
ROOT_PANE = "RootPane"
ROOT_GROUP = "RootGroup"
MATERIAL = "Material"
BANNER_FRAMES = 90
BANNER_FADE = [(0.0, 0.0, 0.0), (30.0, 255.0, 0.0)]
ICON_FRAMES = 2
ICON_HOLD = [(0.0, 255.0, 0.0)]


def align(value, step):
    return (value + step - 1) // step * step


def pad_to(data, step):
    return data + b"\x00" * (align(len(data), step) - len(data))


def encode_rgb565(width, height, pixels):
    out = bytearray()
    for block_y in range(0, height, 4):
        for block_x in range(0, width, 4):
            for y in range(block_y, block_y + 4):
                row = y * width
                for x in range(block_x, block_x + 4):
                    r, g, b = pixels[row + x][:3]
                    out += struct.pack(">H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
    return bytes(out)


def tpl(width, height, pixels):
    image_header_offset = 0x14
    data_offset = align(image_header_offset + 0x24, 32)
    out = struct.pack(">III", TPL_MAGIC, 1, 0x0C)
    out += struct.pack(">II", image_header_offset, 0)
    out += struct.pack(">HHIIIIIIfBBBB", height, width, RGB565, data_offset, 0, 0, 1, 1, 0.0, 0, 0, 0, 0)
    out = pad_to(out, 32)
    return out + encode_rgb565(width, height, pixels)


def imd5(data):
    return b"IMD5" + struct.pack(">I", len(data)) + b"\x00" * 8 + hashlib.md5(data).digest() + data


def u8(files):
    tree = {}
    for path, data in files.items():
        node = tree
        parts = path.split("/")
        for part in parts[:-1]:
            node = node.setdefault(part, {})
        node[parts[-1]] = data
    nodes = [[1, "", 0, 0]]
    blobs = []

    def walk(subtree, parent):
        for name in sorted(subtree, key=str.lower):
            value = subtree[name]
            index = len(nodes)
            if isinstance(value, dict):
                nodes.append([1, name, parent, 0])
                walk(value, index)
                nodes[index][3] = len(nodes)
            else:
                nodes.append([0, name, 0, len(value)])
                blobs.append((index, value))

    walk(tree, 0)
    nodes[0][3] = len(nodes)
    strings = b""
    name_offsets = []
    for node in nodes:
        name_offsets.append(len(strings))
        strings += node[1].encode("ascii") + b"\x00"
    root_offset = 0x20
    data_offset = align(root_offset + 12 * len(nodes) + len(strings), 32)
    out = bytearray(data_offset)
    for index, data in blobs:
        out += b"\x00" * (align(len(out), 32) - len(out))
        nodes[index][2] = len(out)
        out += data
    struct.pack_into(">IIII", out, 0, U8_MAGIC, root_offset, data_offset - root_offset, data_offset)
    for i, (kind, _, a, b) in enumerate(nodes):
        pos = root_offset + 12 * i
        out[pos] = kind
        out[pos + 1:pos + 4] = name_offsets[i].to_bytes(3, "big")
        struct.pack_into(">II", out, pos + 4, a, b)
    strings_at = root_offset + 12 * len(nodes)
    out[strings_at:strings_at + len(strings)] = strings
    return bytes(out)


def section(magic, body):
    body = pad_to(body, 4)
    return magic + struct.pack(">I", 8 + len(body)) + body


def name_field(name, size):
    return name.encode("ascii").ljust(size, b"\x00")


def pane_body(name, width, height):
    out = struct.pack(">BBBx", 1, 4, 255)
    out += name_field(name, 16) + b"\x00" * 8
    out += struct.pack(">3f3f2f", 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0)
    out += struct.pack(">ff", width, height)
    return out


def material_body():
    out = name_field(MATERIAL, 20)
    out += struct.pack(">4h", 0, 0, 0, 0) + struct.pack(">4h", 255, 255, 255, 255) + struct.pack(">4h", 0, 0, 0, 0)
    out += b"\xff" * 16
    out += struct.pack(">I", (1 << 8) | 1)
    out += struct.pack(">HH", 0, 0)
    out += struct.pack(">BBBB", 1, 4, 30, 0)
    return out


def brlyt(width, height, texture_name, pane_name):
    texture = texture_name.encode("ascii") + b"\x00"
    sections = [
        section(b"lyt1", struct.pack(">B3xff", 1, width, height)),
        section(b"txl1", struct.pack(">HHII", 1, 0, 8, 0) + texture),
        section(b"mat1", struct.pack(">HHI", 1, 0, 0x10) + material_body()),
        section(b"pan1", pane_body(ROOT_PANE, width, height)),
        section(b"pas1", b""),
        section(b"pic1", pane_body(pane_name, width, height) + b"\xff" * 16 + struct.pack(">HBx", 0, 1) + struct.pack(">8f", 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0)),
        section(b"pae1", b""),
        section(b"grp1", name_field(ROOT_GROUP, 16) + struct.pack(">HH", 1, 0) + name_field(pane_name, 16)),
    ]
    body = b"".join(sections)
    return struct.pack(">4sHHIHH", b"RLYT", 0xFEFF, 0x000A, 16 + len(body), 0x10, len(sections)) + body


def brlan(pane_name, frames, loop, alpha_keys):
    keys = b"".join(struct.pack(">fff", frame, value, slope) for frame, value, slope in alpha_keys)
    entry = struct.pack(">BBBBHHI", 0, 0x10, 2, 0, len(alpha_keys), 0, 0x0C) + keys
    tag = b"RLVC" + struct.pack(">B3xI", 1, 0x0C) + entry
    animation = name_field(pane_name, 20) + struct.pack(">BBHI", 1, 0, 0, 0x1C) + tag
    table_offset = 0x14
    animation_offset = table_offset + 4
    body = struct.pack(">HBBHHI", frames, 1 if loop else 0, 0, 0, 1, table_offset) + struct.pack(">I", animation_offset) + animation
    pai1 = b"pai1" + struct.pack(">I", 8 + len(body)) + body
    return struct.pack(">4sHHIHH", b"RLAN", 0xFEFF, 0x0008, 16 + len(pai1), 0x10, 1) + pai1


def imet(icon_size, banner_size, sound_size, name):
    block = b"\x00" * IMET_PADDING + b"IMET"
    block += struct.pack(">IIIIII", IMET_HASH_SIZE, IMET_VERSION, icon_size, banner_size, sound_size, 0)
    encoded = name.encode("utf-16-be").ljust(IMET_NAME_BYTES, b"\x00")
    block += encoded * IMET_LANGUAGES
    block += b"\x00" * 588
    digest = hashlib.md5(block + b"\x00" * 16).digest()
    return b"\x00" * IMET_PADDING + block + digest


def layout_bin(name, image, frames, loop, alpha_keys):
    width, height, pixels = image
    files = {
        f"arc/anim/{name}.brlan": brlan(PANE, frames, loop, alpha_keys),
        f"arc/blyt/{name}.brlyt": brlyt(float(width), float(height), f"{name}.tpl", PANE),
        f"arc/timg/{name}.tpl": tpl(width, height, pixels),
    }
    return imd5(u8(files))


def opening_bnr(banner, icon, sound, name):
    banner_bin = layout_bin("banner", banner, BANNER_FRAMES, False, BANNER_FADE)
    icon_bin = layout_bin("icon", icon, ICON_FRAMES, True, ICON_HOLD)
    sound_bin = imd5(sound)
    meta = u8({"meta/banner.bin": banner_bin, "meta/icon.bin": icon_bin, "meta/sound.bin": sound_bin})
    return imet(len(icon_bin), len(banner_bin), len(sound_bin), name) + meta


def load_image(path):
    from PIL import Image

    image = Image.open(path).convert("RGB")
    if image.width % 4 or image.height % 4:
        raise SystemExit(f"{path}: width and height must be multiples of 4, got {image.width}x{image.height}")
    data = image.tobytes()
    return (image.width, image.height, [tuple(data[i:i + 3]) for i in range(0, len(data), 3)])


def main():
    parser = argparse.ArgumentParser(description="Build a channel banner (opening.bnr) from a banner image, an icon image and a WAV sound.")
    parser.add_argument("--banner", required=True, type=Path)
    parser.add_argument("--icon", required=True, type=Path)
    parser.add_argument("--sound", required=True, type=Path)
    parser.add_argument("--name", required=True)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    data = opening_bnr(load_image(args.banner), load_image(args.icon), args.sound.read_bytes(), args.name)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(data)
    print(f"{args.out} {len(data)} bytes")


if __name__ == "__main__":
    main()
