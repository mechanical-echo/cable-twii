import hashlib
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build_banner as bb

RED = (255, 0, 0)
GREEN = (0, 255, 0)
BLUE = (0, 0, 255)


def solid(width, height, color):
    return (width, height, [color] * (width * height))


def sections(blob, start):
    out = []
    pos = start
    while pos < len(blob):
        magic = blob[pos:pos + 4].decode("ascii")
        size = struct.unpack(">I", blob[pos + 4:pos + 8])[0]
        out.append((magic, pos, size))
        pos += size
    return out


def read_u8(blob):
    magic, root_offset, _, _ = struct.unpack(">IIII", blob[:16])
    assert magic == 0x55AA382D
    count = struct.unpack(">I", blob[root_offset + 8:root_offset + 12])[0]
    strings = root_offset + 12 * count
    files = {}
    stack = [("", count)]
    for i in range(1, count):
        node = blob[root_offset + 12 * i:root_offset + 12 * (i + 1)]
        kind = node[0]
        name_offset = int.from_bytes(node[1:4], "big")
        data, size = struct.unpack(">II", node[4:12])
        end = blob.index(b"\x00", strings + name_offset)
        name = blob[strings + name_offset:end].decode("ascii")
        while i >= stack[-1][1]:
            stack.pop()
        path = (stack[-1][0] + "/" + name).strip("/")
        if kind == 1:
            stack.append((path, size))
        else:
            files[path] = (data, blob[data:data + size])
    return files


class Rgb565(unittest.TestCase):
    def test_packs_texels_big_endian(self):
        pixels = [RED] * 16
        pixels[1] = GREEN
        data = bb.encode_rgb565(4, 4, pixels)
        self.assertEqual(32, len(data))
        self.assertEqual(b"\xf8\x00\x07\xe0\xf8\x00", data[:6])

    def test_tiles_in_4_by_4_blocks(self):
        pixels = []
        for _ in range(4):
            pixels += [RED] * 4 + [BLUE] * 4
        data = bb.encode_rgb565(8, 4, pixels)
        self.assertEqual(b"\xf8\x00" * 16, data[:32])
        self.assertEqual(b"\x00\x1f" * 16, data[32:64])


class Tpl(unittest.TestCase):
    def test_single_texture_header(self):
        width, height, pixels = solid(8, 4, RED)
        data = bb.tpl(width, height, pixels)
        self.assertEqual((0x0020AF30, 1, 0x0C), struct.unpack(">III", data[:12]))
        image_header, palette = struct.unpack(">II", data[12:20])
        self.assertEqual(0, palette)
        fields = struct.unpack(">HHIIIIIIfBBBB", data[image_header:image_header + 0x24])
        h, w, fmt, offset, wrap_s, wrap_t, min_f, mag_f, lod, edge, min_lod, max_lod, unpacked = fields
        self.assertEqual((4, 8, bb.RGB565, 0, 0, 1, 1, 0.0, 0, 0, 0, 0), (h, w, fmt, wrap_s, wrap_t, min_f, mag_f, lod, edge, min_lod, max_lod, unpacked))
        self.assertEqual(0, offset % 32)
        self.assertEqual(offset + 64, len(data))
        self.assertEqual(bb.encode_rgb565(width, height, pixels), data[offset:])


class Imd5(unittest.TestCase):
    def test_wraps_payload_with_md5(self):
        data = bb.imd5(b"abc")
        self.assertEqual(b"IMD5" + struct.pack(">I", 3) + b"\x00" * 8 + hashlib.md5(b"abc").digest() + b"abc", data)


class U8(unittest.TestCase):
    def test_round_trips_files_under_directories(self):
        files = {"arc/blyt/x.brlyt": b"L" * 5, "arc/timg/t.tpl": b"T" * 40, "arc/anim/a.brlan": b"A"}
        blob = bb.u8(files)
        read = read_u8(blob)
        self.assertEqual(files, {path: data for path, (_, data) in read.items()})
        for offset, _ in read.values():
            self.assertEqual(0, offset % 32)

    def test_header_offsets(self):
        blob = bb.u8({"meta/sound.bin": b"S" * 3})
        magic, root_offset, header_size, data_offset = struct.unpack(">IIII", blob[:16])
        self.assertEqual((0x55AA382D, 0x20), (magic, root_offset))
        self.assertEqual(0, data_offset % 32)
        self.assertEqual(data_offset, root_offset + header_size)
        self.assertEqual(b"\x00" * 16, blob[16:32])


class Brlyt(unittest.TestCase):
    def setUp(self):
        self.blob = bb.brlyt(608.0, 456.0, "banner.tpl", "Picture")

    def test_header(self):
        magic, bom, version, size, header_len, count = struct.unpack(">4sHHIHH", self.blob[:16])
        self.assertEqual((b"RLYT", 0xFEFF, 0x000A, len(self.blob), 0x10, 8), (magic, bom, version, size, header_len, count))

    def test_section_order_and_sizes(self):
        found = sections(self.blob, 16)
        self.assertEqual(["lyt1", "txl1", "mat1", "pan1", "pas1", "pic1", "pae1", "grp1"], [s[0] for s in found])
        sizes = {s[0]: s[2] for s in found}
        self.assertEqual(0x14, sizes["lyt1"])
        self.assertEqual(0x4C, sizes["pan1"])
        self.assertEqual(8, sizes["pas1"])
        self.assertEqual(0x80, sizes["pic1"])
        self.assertEqual(8, sizes["pae1"])
        self.assertEqual(0x1C + 16, sizes["grp1"])
        for _, pos, size in found:
            self.assertEqual(0, size % 4)

    def test_layout_is_centred_with_the_banner_size(self):
        pos = sections(self.blob, 16)[0][1]
        centred, width, height = struct.unpack(">B3xff", self.blob[pos + 8:pos + 0x14])
        self.assertEqual((1, 608.0, 456.0), (centred, width, height))

    def test_texture_list_names_the_tpl(self):
        pos = sections(self.blob, 16)[1][1]
        count, _, name_offset, unused = struct.unpack(">HHII", self.blob[pos + 8:pos + 0x14])
        self.assertEqual((1, 0), (count, unused))
        start = pos + 0x0C + name_offset
        self.assertEqual(b"banner.tpl\x00", self.blob[start:start + 11])

    def test_material_uses_one_texture(self):
        pos = sections(self.blob, 16)[2][1]
        count, _, offset = struct.unpack(">HHI", self.blob[pos + 8:pos + 0x10])
        self.assertEqual((1, 0x10), (count, offset))
        material = self.blob[pos + offset:]
        self.assertEqual(b"Material", material[:8])
        flags = struct.unpack(">I", material[0x3C:0x40])[0]
        self.assertEqual(1, flags & 0xF)
        self.assertEqual(1, (flags >> 4) & 0xF)
        self.assertEqual(1, (flags >> 8) & 0xF)
        self.assertEqual(0, (flags >> 20) & 0x1F)
        tex_index, tex_settings = struct.unpack(">HH", material[0x40:0x44])
        self.assertEqual((0, 0), (tex_index, tex_settings))
        self.assertEqual((0.0, 0.0, 0.0, 1.0, 1.0), struct.unpack(">5f", material[0x44:0x58]))
        self.assertEqual((1, 4, 30, 0), struct.unpack(">BBBB", material[0x58:0x5C]))
        self.assertEqual(0x10 + 0x5C, sections(self.blob, 16)[2][2])

    def test_root_pane_and_picture_pane(self):
        found = sections(self.blob, 16)
        root = found[3][1]
        flags, origin, alpha, name = struct.unpack(">BBBx16s", self.blob[root + 8:root + 8 + 0x14])
        self.assertEqual((1, 4, 255, b"RootPane".ljust(16, b"\x00")), (flags, origin, alpha, name))
        pic = found[5][1]
        flags, origin, alpha, name = struct.unpack(">BBBx16s", self.blob[pic + 8:pic + 8 + 0x14])
        self.assertEqual((1, 4, 255, b"Picture".ljust(16, b"\x00")), (flags, origin, alpha, name))
        scale_x, scale_y, width, height = struct.unpack(">ffff", self.blob[pic + 8 + 0x34:pic + 8 + 0x44])
        self.assertEqual((1.0, 1.0, 608.0, 456.0), (scale_x, scale_y, width, height))
        colours = self.blob[pic + 0x4C:pic + 0x5C]
        self.assertEqual(b"\xff" * 16, colours)
        material, uv_count = struct.unpack(">HBx", self.blob[pic + 0x5C:pic + 0x60])
        self.assertEqual((0, 1), (material, uv_count))
        uv = struct.unpack(">8f", self.blob[pic + 0x60:pic + 0x80])
        self.assertEqual((0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 1.0, 1.0), uv)

    def test_group_lists_the_picture(self):
        pos = sections(self.blob, 16)[7][1]
        name, count = struct.unpack(">16sH", self.blob[pos + 8:pos + 0x1A])
        self.assertEqual((b"RootGroup".ljust(16, b"\x00"), 1), (name, count))
        self.assertEqual(b"Picture".ljust(16, b"\x00"), self.blob[pos + 0x1C:pos + 0x2C])


class Brlan(unittest.TestCase):
    def setUp(self):
        self.blob = bb.brlan("Picture", 90, False, [(0.0, 0.0, 0.0), (30.0, 255.0, 0.0)])

    def test_header(self):
        magic, bom, version, size, header_len, count = struct.unpack(">4sHHIHH", self.blob[:16])
        self.assertEqual((b"RLAN", 0xFEFF, 0x0008, len(self.blob), 0x10, 1), (magic, bom, version, size, header_len, count))

    def test_pai1_describes_one_pane_animation(self):
        pos = 16
        magic, size, frames, flags, _, textures, animations, table = struct.unpack(">4sIHBBHHI", self.blob[pos:pos + 0x14])
        self.assertEqual((b"pai1", len(self.blob) - 16, 90, 0, 0, 1, 0x14), (magic, size, frames, flags, textures, animations, table))
        entry = pos + struct.unpack(">I", self.blob[pos + table:pos + table + 4])[0]
        name, tags, kind = struct.unpack(">20sBB", self.blob[entry:entry + 0x16])
        self.assertEqual((b"Picture".ljust(20, b"\x00"), 1, 0), (name, tags, kind))
        tag = entry + struct.unpack(">I", self.blob[entry + 0x18:entry + 0x1C])[0]
        tag_magic, entries = struct.unpack(">4sB", self.blob[tag:tag + 5])
        self.assertEqual((b"RLVC", 1), (tag_magic, entries))
        item = tag + struct.unpack(">I", self.blob[tag + 8:tag + 12])[0]
        index, target, key_type, _, key_count, _, key_offset = struct.unpack(">BBBBHHI", self.blob[item:item + 12])
        self.assertEqual((0, 0x10, 2, 2, 0x0C), (index, target, key_type, key_count, key_offset))
        keys = struct.unpack(">6f", self.blob[item + 12:item + 36])
        self.assertEqual((0.0, 0.0, 0.0, 30.0, 255.0, 0.0), keys)

    def test_loop_flag(self):
        blob = bb.brlan("Picture", 2, True, [(0.0, 255.0, 0.0)])
        self.assertEqual(1, blob[16 + 0x0A])


class Imet(unittest.TestCase):
    def test_header_layout_and_hash(self):
        data = bb.imet(1000, 2000, 3000, "cable-twii")
        self.assertEqual(0x640, len(data))
        self.assertEqual(b"\x00" * 0x80, data[:0x80])
        self.assertEqual(b"IMET", data[0x80:0x84])
        self.assertEqual((0x600, 3, 1000, 2000, 3000, 0), struct.unpack(">IIIIII", data[0x84:0x9C]))
        name = "cable-twii".encode("utf-16-be").ljust(84, b"\x00")
        for language in range(10):
            self.assertEqual(name, data[0x9C + 84 * language:0x9C + 84 * (language + 1)])
        self.assertEqual(b"\x00" * 588, data[0x3E4:0x630])
        expected = hashlib.md5(data[0x40:0x630] + b"\x00" * 16).digest()
        self.assertEqual(expected, data[0x630:0x640])


class OpeningBnr(unittest.TestCase):
    def test_assembles_the_three_parts(self):
        banner = solid(8, 4, RED)
        icon = solid(4, 4, BLUE)
        sound = b"RIFF" + b"\x00" * 60
        data = bb.opening_bnr(banner, icon, sound, "cable-twii")
        self.assertEqual(b"IMET", data[0x80:0x84])
        meta = read_u8(data[0x640:])
        self.assertEqual({"meta/banner.bin", "meta/icon.bin", "meta/sound.bin"}, set(meta))
        sizes = struct.unpack(">III", data[0x8C:0x98])
        self.assertEqual((len(meta["meta/icon.bin"][1]), len(meta["meta/banner.bin"][1]), len(meta["meta/sound.bin"][1])), sizes)
        self.assertEqual(bb.imd5(sound), meta["meta/sound.bin"][1])
        banner_bin = meta["meta/banner.bin"][1]
        self.assertEqual(b"IMD5", banner_bin[:4])
        inner = read_u8(banner_bin[32:])
        self.assertEqual({"arc/anim/banner.brlan", "arc/blyt/banner.brlyt", "arc/timg/banner.tpl"}, set(inner))
        self.assertEqual(bb.tpl(*banner), inner["arc/timg/banner.tpl"][1])
        icon_inner = read_u8(meta["meta/icon.bin"][1][32:])
        self.assertEqual({"arc/anim/icon.brlan", "arc/blyt/icon.brlyt", "arc/timg/icon.tpl"}, set(icon_inner))
        self.assertEqual(bb.brlyt(4.0, 4.0, "icon.tpl", "Picture"), icon_inner["arc/blyt/icon.brlyt"][1])


if __name__ == "__main__":
    unittest.main()
