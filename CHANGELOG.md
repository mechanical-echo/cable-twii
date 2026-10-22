# Changelog

## Unreleased

- Wii app redesigned at system detail, after the "Wii app redesign" deck: glossy cards with highlights and soft shadows, a curved system bar with the clock, date, HOME disc and channel pills, channel-number pills in the guide (blue for the channel playing), an "A Watch" hint on the preview, an "Up next" column with a countdown in the info panel and the OSD, a channel chip with a blue tile and the channel name dropping in, a glossy volume panel, an up-next card with a bloom and a commercial-break pill, a redesigned HOME menu, an intro panel with a shine and loading dots, an empty-library screen, and pills that press under A and act on release. The OSD panel no longer has buttons: the D-pad flips channels, B returns to the guide, and the pointer presses only the volume panel's − and +.
- Wii app: text is narrowed to the pixel width on 16:9 TVs, so letters keep their shape instead of stretching.
- Wii Menu channel: a forwarder channel (title `CTWA`) that starts cable-twii from `apps/cable-twii/boot.dol` on the SD card or a USB drive and returns to the Wii Menu with a message when it is missing, with a banner, icon and jingle in the app's style. Built by `tools/build-wad.sh`; not yet checked in Dolphin or on a Wii.

## 0.9.0

Feature-complete, not yet verified on real Wii hardware.

- Wii app: plays your videos, channels and ads from a USB flash drive or SD card on a schedule, like your own cable TV, with a guide, on-screen channel info, a HOME menu and remembered channel and volume.
- Studio: a Mac tool used in the browser to import and convert videos, set up channels, build schedules and preview the lineup the Wii will air.
