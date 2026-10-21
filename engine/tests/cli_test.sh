#!/bin/bash
here="$(cd "$(dirname "$0")" && pwd)"
engine="$(dirname "$here")"
bin="$engine/build/cable-twii-sched"
fixture="$here/fixtures/root_basic"
node_bin=/opt/homebrew/opt/node@22/bin/node
[ -x "$node_bin" ] || node_bin=node
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
failed=0

pass() { echo "ok $1"; }
fail() { echo "FAIL $1"; failed=1; }
check() { if [ "$2" = "$3" ]; then pass "$1"; else fail "$1 (expected '$3', got '$2')"; fi; }
js() { "$node_bin" -e "const d=JSON.parse(require('fs').readFileSync(0,'utf8'));$1"; }
jscheck() { if printf '%s' "$2" | js "process.exit(($3)?0:1)" 2>/dev/null; then pass "$1"; else fail "$1"; fi; }

make -C "$engine" cli >/dev/null 2>&1 || { echo "FAIL build"; exit 1; }
pass build

out="$("$bin" validate "$fixture")"; rc=$?
check "validate exit" "$rc" 0
jscheck "validate ok" "$out" "d.ok===true&&d.issues.length===0"

out="$("$bin" validate "$fixture" --check-files)"; rc=$?
check "check-files exit" "$rc" 0
jscheck "check-files ok" "$out" "d.ok===true"

lineup1="$("$bin" lineup "$fixture" --channel 1 --date 2026-10-20)"; rc=$?
check "lineup exit" "$rc" 0
jscheck "lineup contiguous and covers 24h" "$lineup1" "(()=>{let t=d.airings[0].start_ms;const s=t;for(const a of d.airings){if(a.start_ms!==t)return false;t+=a.duration_ms}return t-s===86400000&&d.channel===1&&d.date==='2026-10-20'&&Array.isArray(d.warnings)})()"
jscheck "lineup hash shape" "$lineup1" "/^[0-9a-f]{16}\$/.test(d.hash)"
jscheck "lineup airing fields" "$lineup1" "d.airings.every(a=>['start_ms','duration_ms','kind','path','show','season','episode','title','slot','item'].every(k=>k in a))"
jscheck "lineup episode shape" "$lineup1" "(()=>{const e=d.airings.find(a=>a.kind==='episode');return e&&e.path&&e.show==='Gumball'&&e.title!==null&&e.season!==null&&e.episode!==null})()"
jscheck "lineup upnext shape" "$lineup1" "d.airings.filter(u=>u.kind==='upnext').every(u=>u.path===null&&u.show===null&&u.season===null&&u.episode===null&&u.title===null&&u.slot===-1&&u.item===-1)"
jscheck "lineup ad shape" "$lineup1" "(()=>{const l=d.airings.filter(a=>a.kind==='ad');return l.length>0&&l.every(a=>a.path&&a.show===null&&a.season===null&&a.episode===null&&a.title===null)})()"

lineup2="$("$bin" lineup "$fixture" --channel 2 --date 2026-10-20)"; rc=$?
check "looping lineup exit" "$rc" 0
jscheck "looping lineup covers day" "$lineup2" "d.airings.reduce((t,a)=>t+a.duration_ms,0)===86400000"

for ch in 1 2; do
    l="$("$bin" lineup "$fixture" --channel $ch --date 2026-10-20 | js "process.stdout.write(d.hash)")"
    h="$("$bin" hash "$fixture" --channel $ch --date 2026-10-20)"; rc=$?
    check "hash exit channel $ch" "$rc" 0
    check "hash equals lineup hash channel $ch" "$h" "$l"
    echo "hash channel $ch 2026-10-20: $h"
done
check "hash output is only the hash" "$("$bin" hash "$fixture" --channel 1 --date 2026-10-20 | wc -l | tr -d ' ')" 1

out="$("$bin" now "$fixture" --channel 1 --at "2026-10-20 07:00:05")"; rc=$?
check "now exit" "$rc" 0
jscheck "now offset 5000" "$out" "d.offset_ms===5000&&d.airing.start_ms===25200000"

out="$("$bin" now "$fixture" --channel 1 --at "2026-10-20 03:00:00")"; rc=$?
check "now before first slot exit" "$rc" 0
jscheck "now before first slot is covered by yesterday spill" "$out" "d.airing!==null&&d.offset_ms>=0&&d.offset_ms<d.airing.duration_ms"

out="$("$bin" now "$fixture" --channel 2 --at "2026-10-20 23:59:59")"
jscheck "now end of day" "$out" "d.airing!==null&&d.offset_ms<d.airing.duration_ms"

expect_usage() {
    local name="$1"; shift
    local so rc
    so="$("$bin" "$@" 2>"$tmp/err")"; rc=$?
    check "exit 2: $name" "$rc:${#so}:$([ -s "$tmp/err" ] && echo err)" "2:0:err"
}
expect_usage "no args"
expect_usage "unknown command" bogus
expect_usage "validate without root" validate
expect_usage "missing date" lineup "$fixture" --channel 1
expect_usage "bad date month" lineup "$fixture" --channel 1 --date 2026-13-01
expect_usage "short date" lineup "$fixture" --channel 1 --date 2026-10-2
expect_usage "non-numeric channel" lineup "$fixture" --channel x --date 2026-10-20
expect_usage "missing channel" lineup "$fixture" --date 2026-10-20
expect_usage "date only in at" now "$fixture" --channel 1 --at 2026-10-20
expect_usage "at with T" now "$fixture" --channel 1 --at 2026-10-20T07:00:00
expect_usage "at without seconds" now "$fixture" --channel 1 --at "2026-10-20 07:00"
expect_usage "unexpected option" hash "$fixture" --channel 1 --date 2026-10-20 --extra
expect_usage "channel on validate" validate "$fixture" --channel 1
for badch in 01 +1 " 1" "1 " 0 ""; do
    expect_usage "channel '$badch'" lineup "$fixture" --channel "$badch" --date 2026-10-20
done
expect_usage "unknown channel lineup" lineup "$fixture" --channel 99 --date 2026-10-20
expect_usage "unknown channel now" now "$fixture" --channel 99 --at "2026-10-20 07:00:00"
expect_usage "unknown channel hash" hash "$fixture" --channel 99 --date 2026-10-20
expect_usage "bad hour" now "$fixture" --channel 1 --at "2026-10-20 25:00:00"
expect_usage "bad second" now "$fixture" --channel 1 --at "2026-10-20 07:00:60"

bad="$tmp/bad"
cp -R "$fixture" "$bad"
sed -i.bak 's/show = "Ben 10"/show = "Nope"/' "$bad/config.toml"
rm "$bad/config.toml.bak"
expect_invalid() {
    local name="$1"; shift
    local so rc
    so="$("$bin" "$@" 2>"$tmp/err")"; rc=$?
    check "exit 1 broken root: $name" "$rc:$([ -s "$tmp/err" ] && echo err)" "1:"
    jscheck "validate json on broken root: $name" "$so" "d.ok===false&&d.issues.some(i=>i.code==='unknown_show'&&i.level==='error')"
}
expect_invalid validate validate "$bad"
expect_invalid validate-check-files validate "$bad" --check-files
expect_invalid lineup lineup "$bad" --channel 1 --date 2026-10-20
expect_invalid now now "$bad" --channel 1 --at "2026-10-20 07:00:00"
expect_invalid hash hash "$bad" --channel 1 --date 2026-10-20
expect_invalid unknown-channel lineup "$bad" --channel 99 --date 2026-10-20
so="$("$bin" validate "$bad")"
jscheck "issue shape" "$so" "(()=>{const i=d.issues[0];return Object.keys(i).join()==='level,code,channel,slot,item,line,file,message'&&i.file==='config'&&i.channel===1&&typeof i.message==='string'})()"

missing="$tmp/missing"
cp -R "$fixture" "$missing"
victim="$(sed -n 's/^  path = "\(.*\)"$/\1/p' "$missing/library.toml" | head -1)"
rm "$missing/$victim"
so="$("$bin" validate "$missing" --check-files)"; rc=$?
check "check-files missing exit" "$rc" 1
jscheck "check-files missing_file code" "$so" "d.ok===false&&d.issues.some(i=>i.code==='missing_file')"
"$bin" validate "$missing" >/dev/null; check "validate without check-files ignores missing" "$?" 0

quoted="$tmp/quoted"
cp -R "$fixture" "$quoted"
sed -i.bak 's/title = "Gumball/title = "Say \\"hi\\" \\\\ Gumball/' "$quoted/library.toml"
rm "$quoted/library.toml.bak"
so="$("$bin" lineup "$quoted" --channel 1 --date 2026-10-20)"; rc=$?
check "quoted title exit" "$rc" 0
jscheck "quoted title escaped" "$so" "d.airings.some(a=>a.title&&a.title.startsWith('Say \"hi\" \\\\ Gumball'))"
case "$so" in *'Say \"hi\" \\ Gumball'*) pass "quoted title raw escapes" ;; *) fail "quoted title raw escapes" ;; esac

upnext="$tmp/upnext"
cp -R "$fixture" "$upnext"
cat >> "$upnext/config.toml" <<'TOML'

[[channel]]
number = 3
name = "Quiet"
category = "Test"
folder = "Cartoon Network"
ads = false
mode = "timed"

  [[channel.slot]]
  at = "07:00"
  items = [
    { type = "episode", show = "Ben 10", season = 1, episode = 5 },
  ]
TOML
so="$("$bin" lineup "$upnext" --channel 3 --date 2026-10-20)"; rc=$?
check "upnext lineup exit" "$rc" 0
jscheck "upnext shape" "$so" "(()=>{const l=d.airings.filter(a=>a.kind==='upnext');return l.length>0&&l.every(u=>u.path===null&&u.show===null&&u.season===null&&u.episode===null&&u.title===null&&u.slot===-1&&u.item===-1)})()"

badutf="$tmp/badutf"
cp -R "$fixture" "$badutf"
{
    while IFS= read -r line; do
        if [ "$line" = '  title = "Gumball 1"' ]; then
            printf '  title = "Bad\377X"\n'
        else
            printf '%s\n' "$line"
        fi
    done < "$fixture/library.toml"
} > "$badutf/library.toml"
so="$("$bin" lineup "$badutf" --channel 1 --date 2026-10-20)"; rc=$?
if [ "$rc" = 0 ]; then
    jscheck "invalid utf-8 becomes U+FFFD" "$so" "d.airings.some(a=>a.title==='Bad\uFFFDX')"
else
    echo "note: toml parser rejects invalid utf-8 (exit $rc), escaper not reachable through a title"
fi

exit $failed
