"""Collect upstream licence files and bundled JSON notices for binary packages."""
from pathlib import Path
import argparse
import shutil

parser = argparse.ArgumentParser()
parser.add_argument('--installed', type=Path, required=True)
args = parser.parse_args()
output = Path('package/Licences')
output.mkdir(parents=True, exist_ok=True)
shutil.copy2('LICENSE', output / 'GPL-3.0.txt')
for dependency in ('spessasynth_core_c', 'rtmidi', 'Nuked-SC55'):
    source = Path('3rdparty') / dependency
    for path in source.rglob('*'):
        if path.is_file() and path.name.upper().startswith(('LICENSE', 'LICENCE', 'COPYING', 'NOTICE', 'COPYRIGHT')):
            target = output / dependency / path.relative_to(source)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
# These bundled components keep their BSD notices in source headers.
for name in ('json.h', 'json-builder.h'):
    source = Path('3rdparty/spessasynth_core_c/spessasynth_core/extern/json') / name
    text = source.read_text()
    start = text.index('/*')
    end = text.index('*/', start) + 2
    (output / f'SpessaSynth-{name}.txt').write_text(text[start:end] + '\n')
for port in ('libflac', 'libvorbis', 'libogg', 'zlib'):
    shutil.copy2(args.installed / 'share' / port / 'copyright', output / f'{port}.txt')
(output / 'Plugin-credits.txt').write_text(
    'Original MIDI plugin: Copyright 2025, Christopher Snowhill (kode54).\n'
    'Modifications: Copyright 2026, Vo1dTear.\n'
    'Distributed under GPL-3.0-or-later. See GPL-3.0.txt.\n'
    'Corresponding source: fooyin-plugin-midi-source.tar.gz in the same release.\n'
)
