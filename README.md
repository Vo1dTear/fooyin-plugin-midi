# Fooyin MIDI plugin

This repository contains the MIDI input plugin for fooyin, modified by Vo1dTear.

The plugin adds MIDI, RMID, karaoke MIDI and related MIDI container formats to fooyin. Playback is provided by SpessaSynth, using SoundFont and DLS banks.

The plugin is derived from [kode54's original fooyin plugin repository](https://github.com/kode54/fooyin-kode54-plugins). The original author, copyright notices, and GPL license terms are retained in the source files and plugin metadata.

## Requirements

* CMake 3.14 or newer
* Git
* A C++ compiler with C++20 support
* Qt and the fooyin development files
* Ninja or Make

## Dependencies

The plugin depends on:

* [fooyin](https://github.com/fooyin/fooyin)
* [spessasynth_core_c](https://github.com/kode54/spessasynth_core_c)

The plugin also needs a SoundFont or DLS bank for MIDI files that do not contain an embedded sound bank. Configure the bank in fooyin's MIDI Input settings.

## Build

Clone the repository and configure a release build:

```sh
git clone https://github.com/Vo1dTear/fooyin-plugin-midi.git
cd fooyin-plugin-midi
mkdir -p build
cd build
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    ..
cmake --build .
```

If Ninja is not installed, omit `-G Ninja` and use the default CMake generator.

The build generates:

```text
build/midiplugin/fyplugin_midiplugin.so
```

## Installation

### Arch Linux

The plugin is available from the AUR as `fooyin-plugin-midi-git`:

```sh
yay -S fooyin-plugin-midi-git
```

Alternatively, you can use another AUR helper such as `paru`:

```sh
paru -S fooyin-plugin-midi-git
```

### From source

After building the plugin, install it system-wide using CMake:

```sh
sudo cmake --install .
```

With the `/usr` installation prefix shown in the build instructions, the plugin is installed to:

```text
/usr/lib/fooyin/plugins/fyplugin_midiplugin.so
```

For a local user installation, copy the generated plugin to fooyin's user plugin directory:

```sh
mkdir -p ~/.local/lib/fooyin/plugins
cp midiplugin/fyplugin_midiplugin.so ~/.local/lib/fooyin/plugins/
```

The exact plugin directory can vary by fooyin installation. Use the system-wide path configured by fooyin when installing through CMake.

## Features

* MIDI, RMID, KAR and related MIDI formats
* MIDI format 2 subsongs
* SoundFont, SoundFont3, DLS, SF2Pack and soundfont list support
* Embedded sound banks in RMID files
* Configurable interpolation and polyphony
* Configurable gain from -12 dB to +12 dB
* Configurable loop count and fade length
* Support for fooyin's `Repeat track` option without fading between repetitions

## Credits and license

Original MIDI plugin by Christopher Snowhill (kode54).

Modifications by Vo1dTear, 2026.

This project is distributed under the GNU General Public License, version 3 or later. See the license notices in the source files and `midiplugin/midiinput.json`.

The plugin uses [spessasynth_core_c](https://github.com/kode54/spessasynth_core_c), which is distributed under the Apache-2.0 license. Its license terms apply to that dependency.
