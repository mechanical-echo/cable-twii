#!/bin/bash
set -euo pipefail

if [ $# -ne 2 ]; then
    echo "usage: $0 <episode-file> <out-dir>" >&2
    exit 1
fi

SRC="$1"
OUT="$2"

for tool in ffmpeg ffprobe; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: $tool not found on PATH" >&2
        exit 1
    fi
done

if [ ! -f "$SRC" ]; then
    echo "error: $SRC not found" >&2
    exit 1
fi

DUR=$(ffprobe -v error -show_entries format=duration -of default=nw=1:nk=1 "$SRC")
if awk -v d="$DUR" 'BEGIN { exit !(d < 360) }'; then
    echo "warning: $SRC is ${DUR}s long, the spike wants at least 360s to test seeking to 300s" >&2
fi

mkdir -p "$OUT"

encode() {
    local w="$1" h="$2" rate="$3"
    local maxrate=$((rate * 4 / 3))
    local th=$(( (h * 15 / 100 + 1) / 2 * 2 ))
    local file="$OUT/${w}x${h}.mpg"
    ffmpeg -nostdin -v error -stats -y \
        -i "$SRC" \
        -filter_complex "[0:v]scale=${w}:${h}:force_original_aspect_ratio=decrease,pad=${w}:${h}:(ow-iw)/2:(oh-ih)/2,setsar=1,fps=30,setpts=PTS-STARTPTS[m];testsrc2=s=320x240:r=30,crop=104:38:0:0,scale=-2:${th}:flags=bicubic[t];[m][t]overlay=W-w-8:H-h-8:shortest=1[v];[0:a:0]asetpts=PTS-STARTPTS,loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000[a]" \
        -map "[v]" -map "[a]" \
        -c:v mpeg1video -r 30 -g 15 -b:v "${rate}k" -maxrate "${maxrate}k" -bufsize 1000k \
        -c:a mp2 -ar 48000 -ac 2 -b:a 192k \
        -f mpeg "$file.part"
    mv "$file.part" "$file"
    echo "$file"
}

encode 640 360 1500
encode 480 360 1500
encode 512 288 1200
encode 384 216 900
