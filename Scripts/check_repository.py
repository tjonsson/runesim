"""Validate the Git index before committing; does not modify files or the index."""
from pathlib import Path, PurePosixPath
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def git(*args, data=None):
    return subprocess.check_output(['git', *args], cwd=ROOT, input=data)


def main():
    files = [p for p in git('ls-files', '-z').decode('utf-8').split('\0') if p]
    errors = []
    binaries = []
    art_extensions = {'.blend', '.fbx', '.glb', '.png', '.jpg', '.jpeg', '.tga', '.exr', '.wav', '.ogg', '.mp3', '.zip'}
    generated = {'saved', 'intermediate', 'deriveddatacache', '__pycache__', 'node_modules', '.venv', '.test_venv'}
    for name in files:
        path = PurePosixPath(name)
        parts = [p.lower() for p in path.parts]
        if parts[0] == 'plugins' and len(parts) > 1 and parts[1] != 'airsim':
            errors.append(f'External plugin is staged: {name}')
        if parts[:2] == ['samples', 'pixelstreaming2']:
            errors.append(f'External engine infrastructure is staged: {name}')
        if generated.intersection(parts[:-1]) or parts[0] in {'binaries', 'build', '.vs', '.idea'}:
            errors.append(f'Generated/local file is staged: {name}')
        if path.suffix.lower() in {'.pyc', '.pdb', '.blend1', '.blend2'}:
            errors.append(f'Cache/backup file is staged: {name}')
        if path.suffix.lower() in {'.uasset', '.umap'} or (parts[0] == 'art' and path.suffix.lower() in art_extensions):
            binaries.append(name)
    if binaries:
        fields = git('check-attr', '--cached', '-z', '--stdin', 'filter',
                     data=('\0'.join(binaries)+'\0').encode('utf-8')).decode('utf-8').split('\0')
        for i in range(0, len(fields)-1, 3):
            if fields[i+2] != 'lfs':
                errors.append(f'Binary asset is missing LFS attributes: {fields[i]}')
    if errors:
        print('\n'.join(errors), file=sys.stderr)
        return 1
    print(f'Repository index OK: {len(files)} files, {len(binaries)} LFS asset paths; no external plugins or generated caches.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
