#!/usr/bin/env python3
import random
import sys
from pathlib import Path

CHANNELS = 50
SHOWS_PER_CHANNEL = 4
EPISODES_PER_SHOW = 25
FIRST_SEASON = 13
SEASONLESS_SHOW = 3
ADS = 40
LOOPING_EVERY = 5
NO_ADS_EVERY = 7
SEED = 20261118
GENERATED = "2026-11-18T12:00:00"
EPISODE_MIN_MS = 22 * 60 * 1000
EPISODE_SPREAD_MS = 8 * 60 * 1000
AD_MIN_MS = 15 * 1000
AD_SPREAD_MS = 45 * 1000

CATEGORIES = ["Kids", "Music", "Movies", "Comedy", "Drama", "News", "Sports", "Classics"]
CHANNEL_WORDS = [
    "Cartoon", "Retro", "Café", "Télé", "Sunset", "Comet", "Harbor", "Pixel", "Atlas", "Velvet",
    "Maple", "Neon", "Orbit", "Prairie", "Quartz", "Rocket", "Saffron", "Tidal", "Umbra", "Willow",
    "Zenith", "Arcade", "Beacon", "Cobalt", "Dune",
]
CHANNEL_KINDS = ["Network", "Jeunesse"]
ADJECTIVES = [
    "Amazing", "Brave", "Curious", "Daring", "Electric", "Fearless", "Golden", "Hidden", "Infinite", "Jolly",
    "Lucky", "Midnight", "Northern", "Odd", "Purple", "Quiet", "Restless", "Silver", "Tiny", "Wandering",
]
NOUNS = [
    "Garden", "Rangers", "Detectives", "Island", "Kitchen", "Lighthouse", "Robots", "School", "Express", "Circus",
    "Pirates", "Workshop", "Valley", "Orchestra", "Planet", "Diner", "Academy", "Caravan", "Museum", "Harbour",
]
TITLE_WORDS = [
    "The", "Big", "Day", "Secret", "Storm", "Return", "Night", "Race", "Map", "Party", "Lost", "Key", "Bridge",
    "Winter", "Picnic", "Trouble", "Contest", "Mystery", "Parade", "Visitor", "Clock", "Signal", "Treasure",
]
SENTENCES = [
    "A surprise visitor turns the whole week upside down.",
    "Everyone has to work together before the sun goes down.",
    "An old map leads the gang somewhere nobody expected.",
    "A small mistake grows into the biggest problem of the season.",
    "The friends discover that winning is not the only thing that matters.",
    "A storm knocks out the power just before the big show.",
    "Someone has been keeping a secret, and it is about to come out.",
    "The new neighbour is not quite what they seem.",
]


def toml_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def channel_names():
    names = []
    for i in range(CHANNELS):
        word = CHANNEL_WORDS[i % len(CHANNEL_WORDS)]
        kind = CHANNEL_KINDS[i // len(CHANNEL_WORDS)]
        names.append(f"{word} {kind}")
    return names


def episode_title(rng):
    return " ".join(rng.sample(TITLE_WORDS, 3))


def build(rng):
    channels = []
    for c, name in enumerate(channel_names()):
        shows = []
        for s in range(SHOWS_PER_CHANNEL):
            n = c * SHOWS_PER_CHANNEL + s
            folder = f"The {ADJECTIVES[n % len(ADJECTIVES)]} {NOUNS[(n // len(ADJECTIVES) + n) % len(NOUNS)]}"
            seasonless = s == SEASONLESS_SHOW
            episodes = []
            for e in range(EPISODES_PER_SHOW):
                title = episode_title(rng)
                if seasonless:
                    season, number = 0, e + 1
                    path = f"{name}/{folder}/{number:02d} - {title}.mpg"
                else:
                    season = 1 if e < FIRST_SEASON else 2
                    number = e + 1 if e < FIRST_SEASON else e + 1 - FIRST_SEASON
                    path = f"{name}/{folder}/Season {season}/S{season:02d}E{number:02d} - {title}.mpg"
                episodes.append({
                    "season": season,
                    "episode": number,
                    "title": title,
                    "description": " ".join(rng.sample(SENTENCES, 2)),
                    "path": path,
                    "duration_ms": EPISODE_MIN_MS + rng.randrange(EPISODE_SPREAD_MS),
                })
            shows.append({"folder": folder, "episodes": episodes})
        channels.append({
            "number": c + 1,
            "name": name,
            "category": CATEGORIES[c % len(CATEGORIES)],
            "ads": c % NO_ADS_EVERY != NO_ADS_EVERY - 1,
            "looping": c % LOOPING_EVERY == LOOPING_EVERY - 1,
            "shows": shows,
        })
    ads = [{"path": f"ads/ad-{i + 1:02d}.mpg", "duration_ms": AD_MIN_MS + rng.randrange(AD_SPREAD_MS)} for i in range(ADS)]
    return channels, ads


def library_toml(channels, ads):
    out = ["version = 1", f"generated = {GENERATED}", ""]
    for ad in ads:
        out += ["[[ad]]", f"path = {toml_str(ad['path'])}", 'channel = ""', f"duration_ms = {ad['duration_ms']}", ""]
    for ch in channels:
        for show in ch["shows"]:
            out += [
                "[[show]]",
                f"channel = {toml_str(ch['name'])}",
                f"folder = {toml_str(show['folder'])}",
                f"name = {toml_str(show['folder'])}",
                f"icon = {toml_str(ch['name'] + '/' + show['folder'] + '/show.png')}",
                "",
            ]
            for ep in show["episodes"]:
                out += [
                    "  [[show.episode]]",
                    f"  season = {ep['season']}",
                    f"  episode = {ep['episode']}",
                    f"  title = {toml_str(ep['title'])}",
                    f"  description = {toml_str(ep['description'])}",
                    f"  path = {toml_str(ep['path'])}",
                    f"  duration_ms = {ep['duration_ms']}",
                    "",
                ]
    return "\n".join(out)


def item(fields):
    return "{ " + ", ".join(f"{k} = {v}" for k, v in fields) + " }"


def timed_slots(shows):
    s0, s1, s2, s3 = (toml_str(s["folder"]) for s in shows)
    return [
        ("06:00", [
            [("type", '"random-run"'), ("show", s0), ("count", "4")],
            [("type", '"ads"'), ("count", "2")],
            [("type", '"fill"'), ("show", s1), ("ads_between", "1")],
        ]),
        ("12:00", [
            [("type", '"random"'), ("show", s2), ("season", "1"), ("count", "3")],
            [("type", '"fill"'), ("show", s3), ("ads_between", "2")],
        ]),
        ("18:00", [
            [("type", '"episode"'), ("show", s0), ("season", "1"), ("episode", "1")],
            [("type", '"fill"'), ("show", s2)],
        ]),
        ("23:00", [
            [("type", '"fill"'), ("show", s1), ("ads_between", "1")],
        ]),
    ]


def loop_items(shows):
    s0, s1, _, s3 = (toml_str(s["folder"]) for s in shows)
    return [
        [("type", '"random-run"'), ("show", s0), ("count", "3")],
        [("type", '"ads"'), ("count", "1")],
        [("type", '"random"'), ("show", s1), ("count", "2")],
        [("type", '"random"'), ("show", s3), ("count", "2")],
    ]


def config_toml(channels):
    out = ["version = 1", f"seed = {SEED}", ""]
    for ch in channels:
        out += [
            "[[channel]]",
            f"number = {ch['number']}",
            f"name = {toml_str(ch['name'])}",
            f"category = {toml_str(ch['category'])}",
            f"ads = {'true' if ch['ads'] else 'false'}",
            f"mode = {toml_str('looping' if ch['looping'] else 'timed')}",
        ]
        if ch["looping"]:
            out.append("loop = [")
            out += [f"  {item(fields)}," for fields in loop_items(ch["shows"])]
            out += ["]", ""]
            continue
        out.append("")
        for at, items in timed_slots(ch["shows"]):
            out += ["  [[channel.slot]]", f"  at = {toml_str(at)}", "  items = ["]
            out += [f"    {item(fields)}," for fields in items]
            out += ["  ]", ""]
    return "\n".join(out)


def main(argv):
    if len(argv) != 2:
        print("usage: make-big-library.py <dir>", file=sys.stderr)
        return 2
    root = Path(argv[1]) / "cable-twii"
    root.mkdir(parents=True, exist_ok=True)
    channels, ads = build(random.Random(SEED))
    library = library_toml(channels, ads)
    config = config_toml(channels)
    (root / "library.toml").write_text(library, encoding="utf-8")
    (root / "config.toml").write_text(config, encoding="utf-8")
    episodes = sum(len(s["episodes"]) for ch in channels for s in ch["shows"])
    print(f"{root}: {len(channels)} channels, {episodes} episodes, {len(ads)} ads")
    print(f"library.toml {len(library.encode('utf-8'))} bytes, config.toml {len(config.encode('utf-8'))} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
