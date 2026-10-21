#!/bin/bash
here="$(cd "$(dirname "$0")" && pwd)"
engine="$(dirname "$here")"
bin="$engine/build/cable-twii-sched"
fixture="$here/fixtures/root_basic"
expected_file="$here/wii_check_expected.txt"
dol="$engine/wii-check/wii-check-autotest.dol"
dolphin="${DOLPHIN:-/Applications/Dolphin.app/Contents/MacOS/Dolphin}"
limit="${WII_CHECK_TIMEOUT:-60}"
dates="2026-10-20 2026-10-21 2026-12-31 2027-01-01"

tmp="$(mktemp -d)"
pid=""
cleanup() {
    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        kill "$pid" 2>/dev/null
        sleep 1
        kill -9 "$pid" 2>/dev/null
    fi
    make -C "$engine/wii-check" AUTOTEST=1 clean >/dev/null 2>&1
    rm -rf "$tmp"
}
trap cleanup EXIT

make -C "$engine" cli >/dev/null 2>&1 || { echo "FAIL cli build"; exit 1; }
make -C "$engine/wii-check" AUTOTEST=1 clean >/dev/null 2>&1
make -C "$engine/wii-check" AUTOTEST=1 >"$tmp/wii-build.log" 2>&1 || {
    cat "$tmp/wii-build.log"
    echo "FAIL wii build"
    exit 1
}
if grep -E '(/src/|/wii-check/)[^:]*:[0-9]+:[0-9]+: warning:' "$tmp/wii-build.log"; then
    echo "FAIL wii build has warnings"
    exit 1
fi
echo "ok build"

channels="$(sed -n 's/^number = \([0-9][0-9]*\)$/\1/p' "$fixture/config.toml")"
: >"$tmp/expected.txt"
for d in $dates; do
    for c in $channels; do
        h="$("$bin" hash "$fixture" --channel "$c" --date "$d")" || { echo "FAIL cli hash $d $c"; exit 1; }
        echo "HASH $d $c $h" >>"$tmp/expected.txt"
    done
done
[ -s "$expected_file" ] || { echo "FAIL $expected_file is empty"; exit 1; }
if ! diff -u "$expected_file" "$tmp/expected.txt"; then
    echo "FAIL $expected_file is stale"
    exit 1
fi
echo "ok expected"

[ -x "$dolphin" ] || { echo "FAIL dolphin not found at $dolphin"; exit 1; }
user="$tmp/dolphin-user"
sync="$tmp/sd"
mkdir -p "$user/Config" "$sync"
cp -R "$fixture" "$sync/cable-twii"
cat >"$user/Config/Dolphin.ini" <<EOF
[General]
WiiSDCardSyncFolder = $sync/
WiiSDCardPath = $tmp/WiiSD.raw
[Core]
WiiSDCard = True
WiiSDCardEnableFolderSync = True
WiiSDCardAllowWrites = True
[Interface]
ConfirmStop = False
[Analytics]
PermissionAsked = True
Enabled = False
[AutoUpdate]
UpdateTrack =
[DSP]
Backend = No Audio Output
EOF
cat >"$user/Config/Logger.ini" <<EOF
[Options]
Verbosity = 4
WriteToFile = True
WriteToConsole = False
WriteToWindow = False
[Logs]
OSREPORT = True
OSREPORT_HLE = True
EOF

log="$user/Logs/dolphin.log"
"$dolphin" -u "$user" -b -v Null -e "$dol" >"$tmp/dolphin.out" 2>&1 &
pid=$!
waited=0
while kill -0 "$pid" 2>/dev/null; do
    if [ -f "$log" ] && tr -d '\r' <"$log" | grep -q 'OSREPORT.*: DONE$'; then
        sleep 1
        break
    fi
    if [ "$waited" -ge "$limit" ]; then
        echo "FAIL dolphin timed out after ${limit}s"
        [ -f "$log" ] && tail -20 "$log"
        exit 1
    fi
    sleep 1
    waited=$((waited + 1))
done

[ -f "$log" ] || { echo "FAIL no dolphin log"; cat "$tmp/dolphin.out"; exit 1; }
tr -d '\r' <"$log" >"$tmp/report.log"
grep -o 'OSREPORT[^:]*: ERROR .*' "$tmp/report.log" | sed 's/^[^:]*: //'
grep -o 'OSREPORT[^:]*: HASH .*' "$tmp/report.log" | sed 's/^[^:]*: //' >"$tmp/actual.txt"
cat "$tmp/actual.txt"
count="$(wc -l <"$tmp/actual.txt" | tr -d ' ')"
if [ "$count" != 8 ]; then
    echo "FAIL expected 8 HASH lines from dolphin, got $count"
    exit 1
fi
if diff -u "$tmp/expected.txt" "$tmp/actual.txt"; then
    echo "ok wii hashes match ($count lines)"
    exit 0
fi
echo "FAIL wii hashes differ"
exit 1
