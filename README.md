
# Poppy Music Player

`poppy` is a simple music player that is controlled by process signals.

# Build

Before anything:

```sh
meson setup build
cd build
```

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

Use of this work is governed by its [license](GPL-3.0.txt).
