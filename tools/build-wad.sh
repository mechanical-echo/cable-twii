#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENV="${VENV:-$ROOT/.tooling/wiipy-venv}"
CERTS="${CERTS:-$ROOT/.tooling/cert-chain.bin}"
TITLE_ID="${TITLE_ID:-CTWA}"
IOS="${IOS:-58}"
NAME="${NAME:-cable-twii}"
BUILD="$ROOT/dist/forwarder"
OUT="$ROOT/dist/cable-twii-forwarder.wad"
LIBWIIPY="libWiiPy @ git+https://github.com/NinjaCheetah/libWiiPy@71ad5b90c659d8a6475c8015e7ea5719d61a1cbb"

export DEVKITPRO="${DEVKITPRO:-/opt/devkitpro}"
export DEVKITPPC="${DEVKITPPC:-$DEVKITPRO/devkitPPC}"

if [ ! -x "$VENV/bin/python" ]; then
  echo "creating $VENV with libWiiPy"
  python3 -m venv "$VENV"
  "$VENV/bin/pip" install -q "$LIBWIIPY"
fi

if [ ! -s "$CERTS" ]; then
  cat >&2 <<MSG
The retail certificate chain is missing: $CERTS
Fetch it once from Nintendo's update server (about 2.5 KB):
  "$VENV/bin/python" -c 'import sys; from libWiiPy.title.nus import download_cert_chain; open(sys.argv[1], "wb").write(download_cert_chain())' "$CERTS"
MSG
  exit 1
fi

make -C "$ROOT/forwarder"
python3 "$ROOT/tools/make_jingle.py" "$BUILD/sound.wav"
python3 "$ROOT/tools/build_banner.py" --banner "$ROOT/forwarder/banner/banner.png" --icon "$ROOT/forwarder/banner/icon.png" \
  --sound "$BUILD/sound.wav" --name "$NAME" --out "$BUILD/opening.bnr"
"$VENV/bin/python" "$ROOT/tools/build_wad.py" --dol "$ROOT/forwarder/forwarder.dol" --banner "$BUILD/opening.bnr" \
  --tid "$TITLE_ID" --ios "$IOS" --certs "$CERTS" --out "$OUT"
