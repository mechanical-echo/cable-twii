import hashlib
import struct
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import build_wad as bw
from libWiiPy.title.cert import Certificate, CertificateKeyType, CertificateSignatureLength, CertificateType
from libWiiPy.title.ticket import Ticket
from libWiiPy.title.title import Title
from libWiiPy.title.tmd import TMD

BANNER = b"B" * 100


def dol_with(sections):
    header = bytearray(0x100)
    for i, (address, size) in enumerate(sections):
        struct.pack_into(">I", header, 0x00 + 4 * i, 0x100 + 0x20 * i)
        struct.pack_into(">I", header, 0x48 + 4 * i, address)
        struct.pack_into(">I", header, 0x90 + 4 * i, size)
    struct.pack_into(">I", header, 0xE0, 0x81230000)
    return bytes(header) + bytes(range(256)) * 2


DOL = dol_with([(0x00003400, 0x20), (0x81230000, 0x20)])


class MakeTmd(unittest.TestCase):
    def test_channel_fields(self):
        tmd = bw.make_tmd("CTWA", 58, 1)
        self.assertEqual("0001000143545741", tmd.title_id)
        self.assertEqual("000000010000003a", tmd.ios_tid)
        self.assertEqual(b"\x00\x00\x00\x01", tmd.title_type)
        self.assertEqual((3, 0, 1, 1), (tmd.region, tmd.access_rights, tmd.title_version, tmd.boot_index))
        self.assertEqual("Root-CA00000001-CP00000004", tmd.signature_issuer)

    def test_rejects_bad_title_code(self):
        with self.assertRaises(ValueError):
            bw.make_tmd("CT", 58, 1)


class MakeTicket(unittest.TestCase):
    def test_retail_common_key_and_title(self):
        ticket = bw.make_ticket("0001000143545741", b"\x11" * 16)
        self.assertEqual(bytes.fromhex("0001000143545741"), ticket.title_id)
        self.assertEqual(0, ticket.common_key_index)
        self.assertEqual(b"\x11" * 16, ticket.get_title_key())
        self.assertEqual(676, len(ticket.dump()))
        self.assertEqual("Root-CA00000001-XS00000003", ticket.signature_issuer)


def fake_cert(issuer, child, cert_type):
    cert = Certificate()
    cert.type = cert_type
    cert.signature = b"\x00" * CertificateSignatureLength[cert_type.name].value
    cert.issuer = issuer
    cert.pub_key_type = CertificateKeyType.RSA_2048
    cert.child_name = child
    cert.pub_key_exponent = 0x10001
    return cert.dump()


CHAIN = (fake_cert("Root", "CA00000001", CertificateType.RSA_4096)
         + fake_cert("Root-CA00000001", "CP00000004", CertificateType.RSA_2048)
         + fake_cert("Root-CA00000001", "XS00000003", CertificateType.RSA_2048))


def align(value, step):
    return (value + step - 1) // step * step


class BuildWad(unittest.TestCase):
    def setUp(self):
        self.wad = bw.build_wad(DOL, BANNER, "CTWA", 58, CHAIN, title_key=b"\x22" * 16)
        header_size, _, _, cert, crl, tik, tmd, _, _ = struct.unpack(">I2sHIIIIII", self.wad[:32])
        self.ticket_at = align(header_size, 64) + align(cert, 64) + align(crl, 64)
        self.tmd_at = self.ticket_at + align(tik, 64)
        self.sizes = (cert, crl, tik, tmd)

    def test_header(self):
        header_size, kind, version, cert, crl, tik, tmd, content, footer = struct.unpack(">I2sHIIIIII", self.wad[:32])
        self.assertEqual((0x20, b"Is", 0, len(CHAIN), 0, 676, 556, 0), (header_size, kind, version, cert, crl, tik, tmd, footer))
        self.assertEqual(2560, len(CHAIN))
        self.assertGreaterEqual(content, 512 + 112)

    def test_cert_chain_is_written_first(self):
        self.assertEqual(CHAIN, self.wad[0x40:0x40 + len(CHAIN)])

    def test_ticket_and_tmd_are_fakesigned(self):
        ticket = Ticket()
        ticket.load(self.wad[self.ticket_at:self.ticket_at + 676])
        tmd = TMD()
        tmd.load(self.wad[self.tmd_at:self.tmd_at + 556])
        self.assertTrue(ticket.get_is_fakesigned())
        self.assertTrue(tmd.get_is_fakesigned())
        self.assertEqual("0001000143545741", tmd.title_id)
        self.assertEqual(58, tmd.ios_version)

    def test_contents_are_the_banner_and_the_dol(self):
        tmd = TMD()
        tmd.load(self.wad[self.tmd_at:self.tmd_at + 556])
        records = [(r.content_id, r.index, r.content_type, r.content_size, r.content_hash) for r in tmd.content_records]
        self.assertEqual((0, 0, 1, len(BANNER), hashlib.sha1(BANNER).hexdigest().encode()), records[0])
        self.assertEqual((1, 1, 1, len(DOL), hashlib.sha1(DOL).hexdigest().encode()), records[1])
        self.assertEqual(1, tmd.boot_index)

    def test_round_trips_through_libwiipy(self):
        title = Title()
        title.load_wad(self.wad)
        self.assertEqual(DOL, title.get_content_by_index(1))
        self.assertEqual(BANNER, title.get_content_by_index(0))
        self.assertTrue(title.get_is_fakesigned())

    def test_rejects_a_dol_without_the_nand_boot_stub(self):
        plain = dol_with([(0x81230000, 0x20)])
        with self.assertRaises(ValueError):
            bw.build_wad(plain, BANNER, "CTWA", 58, CHAIN)

    def test_accepts_the_stub_at_the_virtual_address_too(self):
        wad = bw.build_wad(dol_with([(0x80003400, 0x20), (0x80003F00, 0x20)]), BANNER, "CTWA", 58, CHAIN, title_key=b"\x22" * 16)
        self.assertEqual(b"Is", wad[4:6])

    def test_rejects_an_empty_cert_chain(self):
        with self.assertRaises(ValueError):
            bw.build_wad(DOL, BANNER, "CTWA", 58, b"")


if __name__ == "__main__":
    unittest.main()
