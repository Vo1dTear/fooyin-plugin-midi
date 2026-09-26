# Fooyin MIDI plugin

This repository contains the MIDI input plugin for fooyin, modified by Vo1dTear.

The plugin adds MIDI, RMID, karaoke MIDI and related MIDI container formats to fooyin. Playback is provided by SpessaSynth using SoundFont and DLS banks, by the optional Nuked-SC55 engine using SC-55 ROMs, or by an external MIDI application.

The plugin is derived from [kode54's original fooyin plugin repository](https://github.com/kode54/fooyin-kode54-plugins). The original author, copyright notices, and GPL license terms are retained in the source files and plugin metadata.

## Requirements

* CMake 3.20 or newer
* Git
* A C++ compiler with C++23 support (C++20 when Nuked-SC55 is disabled)
* Qt and the fooyin development files
* Ninja or Make
* pkg-config and RtMidi development files for external MIDI output (on Arch: `pkgconf` and `rtmidi`)

## Dependencies

The plugin depends on:

* [fooyin](https://github.com/fooyin/fooyin)
* [spessasynth_core_c](https://github.com/kode54/spessasynth_core_c)

With SpessaSynth, the plugin needs a SoundFont or DLS bank for MIDI files that do not contain an embedded sound bank. Configure the bank in fooyin's MIDI Input settings.

Nuked-SC55 uses [jcmoyer's embeddable backend](https://github.com/jcmoyer/Nuked-SC55), included as a Git submodule at a pinned commit in `3rdparty/Nuked-SC55`. Only the backend is built; SDL and RtMidi are not needed. Its ROM files are not included.

## Build

Clone the repository and configure a release build:

```sh
git clone --recurse-submodules https://github.com/Vo1dTear/fooyin-plugin-midi.git
cd fooyin-plugin-midi
mkdir -p build
cd build
cmake -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    ..
cmake --build .
```

Internal Nuked-SC55 and external MIDI support are enabled by default. To disable
either, add its option to the CMake command:

- Internal engine: `-DMIDI_ENABLE_NUKED_SC55=OFF`
- External MIDI: `-DMIDI_ENABLE_EXTERNAL=OFF`

Disable both to build only SpessaSynth. When using the internal engine, run this
from the repository root before configuring an existing clone or after updating:

```sh
git submodule update --init --recursive
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

## Nuked-SC55 setup

Open fooyin's **MIDI Input** settings:

1. Select **Nuked-SC55** under **Sound engine**.
2. Choose **SC-55 (mk1)** or **SC-55mkII (mk2)**.
3. Select a folder containing one complete ROM set for that model. ROMs are not included.
4. Apply the settings and restart playback.

The first playback may take a moment while the emulator starts. Gain, loop count
and fade are supported, as is audio conversion through fooyin. SoundFont banks
and SpessaSynth-specific controls do not apply.

## External Nuked-SC55 application

This mode sends MIDI to a separate Nuked-SC55 application, which you must launch
and configure yourself. On Linux, it requires the ALSA MIDI sequencer (`/dev/snd/seq`).

1. In **MIDI Input** settings, select **External MIDI / Nuked-SC55 application**.
2. Choose **Virtual output: fooyin MIDI / Output**, click **Open MIDI port**, and apply.
3. Launch Nuked-SC55, configure its ROMs and audio output, and select **fooyin MIDI / Output** as its MIDI input (`--port`; see the application's `--help`).
4. Start playback in fooyin.

For an existing MIDI destination, use **Refresh ports** and select it instead.
Stop playback before changing the port. Only one 16-channel MIDI port is supported.

Use fooyin to control Nuked-SC55's volume and mute. Configure audio output and
effects in Nuked-SC55; fooyin's DSP, gain and fades do not apply.
For audio conversion, select SpessaSynth or the internal Nuked-SC55 engine;
external MIDI mode produces silent files.

## Tests

Enable the callback regression tests with `-DMIDI_BUILD_TESTS=ON`.
To also test audio rendering, seeking and looping, select a local ROM directory
and its model with `MIDI_TEST_ROM_SET`: `mk1` for SC-55 or `mk2` for SC-55mkII
(the default). For example, for SC-55mkII:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMIDI_BUILD_TESTS=ON \
    -DMIDI_TEST_ROM_DIR="/path/to/SC-55mkII-ROMs" \
    -DMIDI_TEST_ROM_SET=mk2
cmake --build build
ctest --test-dir build --output-on-failure
```

For SC-55 mk1:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMIDI_BUILD_TESTS=ON \
    -DMIDI_TEST_ROM_DIR="/path/to/SC-55-ROMs" \
    -DMIDI_TEST_ROM_SET=mk1
cmake --build build
ctest --test-dir build --output-on-failure
```

The ROM tests require `MIDI_ENABLE_NUKED_SC55=ON`. These options affect tests
only; they do not change the model selected in fooyin. ROM files are not included.

## Features

* MIDI, RMID, KAR and related MIDI formats
* MIDI format 2 subsongs
* SoundFont, SoundFont3, DLS, SF2Pack and soundfont list support
* Embedded sound banks in RMID files
* Configurable interpolation and polyphony
* Configurable gain from -12 dB to +12 dB
* Configurable reverb and chorus levels (0% to 500%)
* A default-on switch to enable or disable both reverb and chorus
* Configurable loop count and fade length
* Support for fooyin's `Repeat track` option without fading between repetitions

## Credits and license

Original MIDI plugin by Christopher Snowhill (kode54).

Modifications by Vo1dTear, 2026.

This project is distributed under the GNU General Public License, version 3 or later. See the license notices in the source files and `midiplugin/midiinput.json`.

The plugin uses [spessasynth_core_c](https://github.com/kode54/spessasynth_core_c), which is distributed under the Apache-2.0 license. Its license terms apply to that dependency.

The optional [Nuked-SC55 backend](https://github.com/jcmoyer/Nuked-SC55) is distributed under GPL-2.0-or-later; see its [license](https://github.com/jcmoyer/Nuked-SC55/blob/master/LICENSE).
