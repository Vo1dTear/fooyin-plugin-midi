# Release source package

This archive contains the plugin and its initialized submodules, the fooyin
source used as the build SDK, the pinned vcpkg recipes and patches, and the
patched FLAC, Vorbis, Ogg and zlib source trees used on Linux and Windows.
`build-support/{linux,windows}/revisions.json` and `vcpkg-status.txt` record the
commits and installed package versions. No ROMs or SoundFonts are included.

## Build the plugin

See README.md for host development dependencies. With a compatible fooyin SDK:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH=/path/to/fooyin-install \
    -DMIDI_USE_BUNDLED_SPESSASYNTH=ON \
    -DMIDI_USE_BUNDLED_RTMIDI=ON \
    -DMIDI_ENABLE_EXTERNAL=ON \
    -DMIDI_ENABLE_NUKED_SC55=ON
cmake --build build --parallel 4
```

Submodules are already included; no `git submodule update` is needed.
This command uses system codecs. For the static codec release configuration,
also supply `-DMIDI_STATIC_CODEC_DIR=/path/to/codec-install/TRIPLET`.

## Reproduce the release configuration

`.github/workflows/build.yml` contains the exact configuration commands, SDK
versions, and platform dependencies. Use `build-support/fooyin` instead of the
workflow's fooyin checkout. Windows uses MSVC and Qt 6.8.3; Linux uses Ubuntu
24.04. A compatible compiler, Qt, fooyin dependencies and build tools are needed.

For vcpkg, use the commit recorded in `revisions.json`. The complete recipes,
patches and scripts are included in `build-support/vcpkg`. To restore the Git
history needed by manifest version resolution in that directory:

```sh
git init
git remote add origin https://github.com/microsoft/vcpkg.git
git fetch --depth 1 origin 40f3c709db80acf154ac4b17a1f83c564ebd022e
git reset --mixed FETCH_HEAD
```

Bootstrap vcpkg and run the workflow's codec install commands with
`.github/codecs` as the manifest directory. Use `x64-linux` or
`x64-windows-static-md`; the latter retains the shared MSVC runtime. This route
requires network access for tools and downloads. The corresponding patched
codec sources are also included under `build-support/linux` and
`build-support/windows`, so they can be inspected, modified or built directly
using their CMake projects. The included vcpkg portfiles document the build
options and patches; do not reapply patches to these already patched trees.

The binary packages include licences in `Licences/`. The source trees retain
upstream copyright and licence notices. The plugin is GPL-3.0-or-later;
dependencies retain their respective licences.
