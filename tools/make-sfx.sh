#!/usr/bin/env bash
set -euo pipefail

OUT="$(cd "$(dirname "$0")/../wii/data" && pwd)"
mkdir -p "$OUT"
cd "$OUT"

synth() {
  local name="$1" freq="$2" dur="$3" fade_out="$4"
  local fade_start
  fade_start="$(awk "BEGIN { printf \"%.4f\", ${dur} - ${fade_out} }")"
  ffmpeg -y -v error -f lavfi -i "sine=frequency=${freq}:duration=${dur}" \
    -af "afade=t=in:d=0.004,afade=t=out:st=${fade_start}:d=${fade_out}" \
    -ar 48000 -ac 1 -f s16be "${name}.pcm"
}

synth sfx_hover 1400 0.06 0.02
synth sfx_select 1800 0.09 0.03
synth sfx_back 700 0.10 0.03
synth sfx_channel 2000 0.05 0.015

echo "wrote sfx_hover.pcm sfx_select.pcm sfx_back.pcm sfx_channel.pcm to ${OUT}"
