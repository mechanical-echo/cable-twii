# Sample media library

`tools/make-samples.sh` generates a fake drive data folder with ffmpeg, used for manual tests.

    tools/make-samples.sh samples/out
    tools/make-samples.sh samples/out --short

Output goes to `<out-dir>/cable-twii/` (`samples/out/` is git-ignored).

It produces:

- Cartoon Network: Gumball (6 episodes of 600 s), Adventure Time (5 of 660 s), Ben 10 (one 4:3 episode of 1320 s)
- ads: `ad-a.mpg` 30 s, `ad-b.mpg` 15 s, `ad-c.mpg` 60 s
- mtv-00s: 6 music videos of 200 s
- `channel.png` for both channels and `show.png` for Gumball, 128x128

Videos are MPEG-1/MP2 in an MPEG-PS container, 30 fps, GOP 15. The test pattern shows a timecode, which is used to check seek accuracy.

`--short` divides every duration by 20 for quick runs. The full-length run takes a long time.

If ffmpeg is built without the `drawtext` filter, the show name and episode label are skipped and only the built-in `testsrc2` timecode is shown.
