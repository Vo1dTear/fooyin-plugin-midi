"""Capture sources actually used by each CI build (no binaries or ROMs)."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tarfile
import tempfile

PORTS = ('libflac', 'libvorbis', 'libogg', 'zlib')


def git(repo, *args):
    return subprocess.check_output(['git', '-C', str(repo), *args])


def archive_repo(repo, destination):
    destination.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryFile() as stream:
        subprocess.run(['git', '-C', str(repo), 'archive', 'HEAD'], stdout=stream, check=True)
        stream.seek(0)
        with tarfile.open(fileobj=stream) as archive:
            archive.extractall(destination, filter='data')
    # git archive itself does not include submodule contents.
    if (repo / '.gitmodules').exists():
        paths = git(repo, 'config', '--file', '.gitmodules', '--get-regexp', r'^submodule\..*\.path$')
        for line in paths.decode().splitlines():
            path = line.split(None, 1)[1]
            child = repo / path
            if not (child / '.git').exists():
                raise RuntimeError(f'Uninitialized submodule: {child}')
            archive_repo(child, destination / path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--platform', choices=['linux', 'windows'], required=True)
    parser.add_argument('--vcpkg', type=Path, required=True)
    parser.add_argument('--installed', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    repo = Path.cwd()
    baseline = json.loads((repo / '.github/codecs/vcpkg.json').read_text())['builtin-baseline']
    if git(args.vcpkg, 'rev-parse', 'HEAD').decode().strip() != baseline:
        raise RuntimeError('Codec vcpkg checkout differs from the pinned baseline')
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary) / 'fooyin-plugin-midi-source'
        if args.platform == 'linux':
            archive_repo(repo, root)
            archive_repo(repo / 'fooyin', root / 'build-support/fooyin')
            archive_repo(args.vcpkg, root / 'build-support/vcpkg')
            shutil.copy2(repo / '.github/SOURCE-BUILD.md', root / 'SOURCE-BUILD.md')
        deps = root / 'build-support' / args.platform
        deps.mkdir(parents=True)
        for port in PORTS:
            source = args.vcpkg / 'buildtrees' / port / 'src'
            if not source.is_dir() or not any(source.iterdir()):
                raise RuntimeError(f'Missing codec sources: {source}; disable the binary cache')
            shutil.copytree(source, deps / port / 'src')
            shutil.copytree(args.installed / 'share' / port, deps / port / 'installed-metadata')
        shutil.copy2(args.installed.parent / 'vcpkg/status', deps / 'vcpkg-status.txt')
        revisions = {
            'plugin': git(repo, 'rev-parse', 'HEAD').decode().strip(),
            'fooyin': git(repo / 'fooyin', 'rev-parse', 'HEAD').decode().strip(),
            'vcpkg': baseline,
            'submodules': git(repo, 'submodule', 'status', '--recursive').decode(),
        }
        (deps / 'revisions.json').write_text(json.dumps(revisions, indent=2) + '\n')
        with tarfile.open(args.output, 'w:gz') as archive:
            archive.add(root, arcname=root.name)


if __name__ == '__main__':
    main()
