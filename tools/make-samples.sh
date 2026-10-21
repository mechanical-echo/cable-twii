#!/bin/bash
set -euo pipefail

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "usage: $0 <out-dir> [--short]" >&2
    exit 1
fi

OUT="$1"
DIV=1
if [ "${2:-}" = "--short" ]; then
    DIV=20
elif [ -n "${2:-}" ]; then
    echo "usage: $0 <out-dir> [--short]" >&2
    exit 1
fi

for tool in ffmpeg ffprobe; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: $tool not found on PATH" >&2
        exit 1
    fi
done

FONT=""
for candidate in /System/Library/Fonts/Supplemental/Arial.ttf /System/Library/Fonts/Helvetica.ttc; do
    if [ -f "$candidate" ]; then
        FONT="$candidate"
        break
    fi
done

ROOT="$OUT/cable-twii"

FILTERS=$(ffmpeg -hide_banner -filters 2>/dev/null || true)
HAS_DRAWTEXT=0
case "$FILTERS" in
    *" drawtext "*) HAS_DRAWTEXT=1 ;;
    *) echo "warning: ffmpeg has no drawtext filter, labels are skipped (testsrc2 still shows its own timecode)" >&2 ;;
esac
if [ "$HAS_DRAWTEXT" = 1 ] && [ -z "$FONT" ]; then
    HAS_DRAWTEXT=0
    echo "warning: no usable font file found, labels are skipped" >&2
fi

encode() {
    local file="$1" source="$2" label="$3" seconds="$4" size="$5" freq="$6"
    local dur
    dur=$(awk -v s="$seconds" -v d="$DIV" 'BEGIN { v = s / d; if (v < 2.05) v = 2.05; printf "%.3f", v }')
    local vf="null"
    if [ "$HAS_DRAWTEXT" = 1 ]; then
        vf="drawtext=fontfile=${FONT}:text='${label} %{pts\:hms}':fontcolor=white:fontsize=28:box=1:boxcolor=black@0.6:x=(w-text_w)/2:y=h-text_h-20"
    fi
    mkdir -p "$(dirname "$file")"
    ffmpeg -nostdin -v error -y \
        -f lavfi -i "${source}=size=${size}:rate=30" \
        -f lavfi -i "sine=frequency=${freq}:sample_rate=48000" \
        -t "$dur" \
        -vf "$vf" \
        -af "loudnorm=I=-16:TP=-1.5:LRA=11" \
        -c:v mpeg1video -r 30 -g 15 -b:v 1500k -maxrate 2000k -bufsize 1000k \
        -c:a mp2 -ar 48000 -ac 2 -b:a 192k \
        -f mpeg "$file.part"
    mv "$file.part" "$file"
    echo "$file"
}

png() {
    local file="$1" color="$2"
    mkdir -p "$(dirname "$file")"
    ffmpeg -nostdin -v error -y -f lavfi -i "color=c=${color}:s=128x128" -frames:v 1 -f image2 "$file.part"
    mv "$file.part" "$file"
    echo "$file"
}

word() {
    case "$1" in
        1) echo One ;;
        2) echo Two ;;
        3) echo Three ;;
        4) echo Four ;;
        5) echo Five ;;
        6) echo Six ;;
    esac
}

SHOWS="$ROOT/Cartoon Network"
GUMBALL="Gumball"
n=0
for ep in 1 2 3 4; do
    n=$((n + 1))
    code=$(printf "S01E%02d" "$ep")
    encode "$SHOWS/Gumball/Season 1/$code - Test $(word $n).mpg" testsrc2 "$GUMBALL $code" 600 640x360 $((300 + n * 50))
done
for ep in 1 2; do
    n=$((n + 1))
    code=$(printf "S02E%02d" "$ep")
    encode "$SHOWS/Gumball/Season 2/$code - Test $(word $n).mpg" testsrc2 "$GUMBALL $code" 600 640x360 $((300 + n * 50))
done
for ep in 1 2 3 4 5; do
    code=$(printf "S05E%02d" "$ep")
    encode "$SHOWS/Adventure Time/Season 5/$code - Test $(word $ep).mpg" testsrc2 "Adventure Time $code" 660 640x360 $((500 + ep * 50))
done
encode "$SHOWS/Ben 10/Season 1/S01E05 - Long One.mpg" testsrc2 "Ben 10 S01E05" 1320 480x360 440

encode "$ROOT/ads/ad-a.mpg" smptebars "Ad A" 30 640x360 600
encode "$ROOT/ads/ad-b.mpg" smptebars "Ad B" 15 640x360 700
encode "$ROOT/ads/ad-c.mpg" smptebars "Ad C" 60 640x360 800

for i in 1 2 3 4 5 6; do
    encode "$ROOT/mtv-00s/Music Videos/Artist $i - Song $i.mpg" testsrc2 "Artist $i - Song $i" 200 640x360 $((250 + i * 60))
done

png "$ROOT/Cartoon Network/channel.png" orange
png "$ROOT/mtv-00s/channel.png" purple
png "$ROOT/Cartoon Network/Gumball/show.png" blue
