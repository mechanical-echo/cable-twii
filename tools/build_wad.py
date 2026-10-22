#!/usr/bin/env python3
import argparse
import hashlib
import secrets
from pathlib import Path

from libWiiPy.title.crypto import encrypt_content, encrypt_title_key
from libWiiPy.title.ticket import Ticket, _TitleLimit
from libWiiPy.title.title import Title
from libWiiPy.title.tmd import TMD

CHANNEL = "00010001"
SIGNATURE_RSA2048 = b"\x00\x01\x00\x01"
TMD_ISSUER = "Root-CA00000001-CP00000004"
TICKET_ISSUER = "Root-CA00000001-XS00000003"
REGION_FREE = 3
CONTENT_NORMAL = 1
BANNER_INDEX = 0
DOL_INDEX = 1
TITLE_LIMITS = 8
RETAIL_COMMON_KEY = 0
NAND_STUB_ADDRESS = 0x3400


def title_id_for(code):
    if len(code) != 4 or not code.isascii() or not code.isalnum():
        raise ValueError(f"title code must be 4 ASCII letters or digits, got {code!r}")
    return CHANNEL + code.encode("ascii").hex()


def make_tmd(code, ios, title_version):
    tmd = TMD()
    tmd.signature_type = SIGNATURE_RSA2048
    tmd.signature = b"\x00" * 256
    tmd.signature_issuer = TMD_ISSUER
    tmd.ios_tid = f"00000001{ios:08x}"
    tmd.ios_version = ios
    tmd.title_id = title_id_for(code)
    tmd.title_type = b"\x00\x00\x00\x01"
    tmd.region = REGION_FREE
    tmd.ratings = b"\x00" * 16
    tmd.reserved1 = b"\x00" * 12
    tmd.ipc_mask = b"\x00" * 12
    tmd.reserved2 = b"\x00" * 18
    tmd.access_rights = 0
    tmd.title_version = title_version
    tmd.boot_index = DOL_INDEX
    return tmd


def make_ticket(title_id, title_key):
    ticket = Ticket()
    ticket.signature_type = SIGNATURE_RSA2048
    ticket.signature = b"\x00" * 256
    ticket.signature_issuer = TICKET_ISSUER
    ticket.ecdh_data = b"\x00" * 60
    ticket.title_key_enc = encrypt_title_key(title_key, RETAIL_COMMON_KEY, title_id)
    ticket.ticket_id = b"\x00" * 8
    ticket.title_id = bytes.fromhex(title_id)
    ticket.unknown1 = b"\x00" * 2
    ticket.permitted_titles = b"\x00" * 4
    ticket.permit_mask = b"\x00" * 4
    ticket.common_key_index = RETAIL_COMMON_KEY
    ticket.unknown2 = b"\x00" * 48
    ticket.content_access_permissions = b"\x00" * 64
    ticket.title_limits_list = [_TitleLimit(0, 0) for _ in range(TITLE_LIMITS)]
    return ticket


def has_nand_stub(dol):
    if len(dol) < 0x100:
        return False
    for i in range(7):
        address = int.from_bytes(dol[0x48 + 4 * i:0x4C + 4 * i], "big")
        size = int.from_bytes(dol[0x90 + 4 * i:0x94 + 4 * i], "big")
        if size and (address & 0x7FFFFFFF) == NAND_STUB_ADDRESS:
            return True
    return False


def build_wad(dol, banner, code, ios, cert_chain, title_version=1, title_key=None):
    if not has_nand_stub(dol):
        raise ValueError("the DOL has no boot stub at 0x3400, so IOS could not start it as a channel")
    title_key = title_key or secrets.token_bytes(16)
    title = Title()
    title.tmd = make_tmd(code, ios, title_version)
    title.ticket = make_ticket(title.tmd.title_id, title_key)
    title.ticket.title_version = title_version
    if not cert_chain:
        raise ValueError("a retail certificate chain is required to pack a WAD")
    title.load_cert_chain(cert_chain)
    banner_hash = hashlib.sha1(banner).hexdigest().encode()
    title.content.add_enc_content(encrypt_content(banner, title_key, BANNER_INDEX), BANNER_INDEX, BANNER_INDEX, CONTENT_NORMAL, len(banner), banner_hash)
    title.tmd.content_records = title.content.content_records
    title.add_content(dol, DOL_INDEX, CONTENT_NORMAL)
    title.tmd.boot_index = DOL_INDEX
    title.fakesign()
    return title.dump_wad()


def main():
    parser = argparse.ArgumentParser(description="Pack a fakesigned channel WAD from a forwarder DOL and an opening.bnr.")
    parser.add_argument("--dol", required=True, type=Path)
    parser.add_argument("--banner", required=True, type=Path)
    parser.add_argument("--tid", required=True, help="4-character title code, for example CTWA")
    parser.add_argument("--ios", type=int, default=58)
    parser.add_argument("--certs", required=True, type=Path, help="retail certificate chain")
    parser.add_argument("--version", type=int, default=1)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()
    wad = build_wad(args.dol.read_bytes(), args.banner.read_bytes(), args.tid, args.ios, args.certs.read_bytes(), args.version)
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_bytes(wad)
    print(f"{args.out} {len(wad)} bytes, title {title_id_for(args.tid)}, IOS{args.ios}, version {args.version}")


if __name__ == "__main__":
    main()
