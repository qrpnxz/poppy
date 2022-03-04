
# Poppy Music Player

`poppy` is a simple music player that is controlled by process signals.

# Features

Poppy plays sound via [PulseAudio],
and is controlled via [D-Bus] [MPRIS] interface.

Up to 8 [channels][vorbis-channel-map] are supported.
All audio is resampled to 48khz.

The initial playlist is determined by command line arguments.
Links in an Ogg chain will be considered distinct tracks.

## Filetypes supported

 - [FLAC] (Native, Ogg)
 - [Opus] (Ogg)
 - [Vorbis] (Ogg)

# Usage

## Running

```sh
poppy track1.flac track2.opus track3.ogg ...
```

## Controlling

### [playerctl]

```sh
playerctl -p poppy play-pause
playerctl -p poppy next
playerctl -p poppy previous
...
```

### [dbus-send]

```sh
dbus-send --session --dest=org.mpris.MediaPlayer2.poppy --print-reply /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.PlayPause
dbus-send --session --dest=org.mpris.MediaPlayer2.poppy --print-reply /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Next
dbus-send --session --dest=org.mpris.MediaPlayer2.poppy --print-reply /org/mpris/MediaPlayer2 org.mpris.MediaPlayer2.Player.Previous
```

# Build

Before anything:

```sh
meson setup build
cd build
```

## Requirements

See [poppy/meson.build](poppy/meson.build)
and [poppyctl/meson.build](poppyctl/meson.build)

## Compiling

```sh
ninja
```

See `meson configure -h` for configuration options
(i.e. install location, optimization, etc.).

Executables will be in [build/poppy](build/poppy)
and [build/poppyctl](build/poppyctl).

## Installing

```sh
ninja install
```

## Uninstalling

```sh
ninja uninstall
```

# License

[![GPLv3 logo]][GPLv3]

Use of this work is governed by the [GNU General Public License, version 3 or later][license].

- [GPL FAQ]



[PulseAudio]: https://www.freedesktop.org/wiki/Software/PulseAudio/ (PulseAudio)
[D-Bus]: https://www.freedesktop.org/wiki/Software/dbus/ (D-Bus)
[MPRIS]: https://specifications.freedesktop.org/mpris-spec/latest/ (MPRIS Spec)

[vorbis-channel-map]: https://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-810004.3.9 (Vorbis Channel Map)

[FLAC]: https://xiph.org/flac/ (Free Lossless Audio Codec)
[Opus]: https://www.opus-codec.org/ (Opus)
[Vorbis]: https://xiph.org/vorbis/ (Vorbis)

[playerctl]: https://github.com/altdesktop/playerctl (playerctl)
[dbus-send]: https://dbus.freedesktop.org/doc/dbus-send.1.html (dbus-send)

[GPLv3 logo]: https://www.gnu.org/graphics/gplv3-with-text-136x68.png
[GPLv3]: https://www.gnu.org/licenses/gpl-3.0.html
[license]: GPL-3.0.txt
[GPL FAQ]: https://www.gnu.org/licenses/gpl-faq.html
