#!/usr/bin/env python3
"""Package the identical portable rasterizer/fonts for the standalone Desktop repo."""
from pathlib import Path
import hashlib
import shutil

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'components/flint/render'
TARGET = ROOT.parent / 'flint_desktop/src/watch_render/shared'
NAMES = ['scene.h', 'renderer.h', 'builders.h', 'boot.h', 'fonts.cpp',
         *['fonts/' + p.name for p in sorted((SOURCE / 'fonts').glob('*.inc'))]]

def main():
    checksum = hashlib.sha256()
    for name in NAMES:
        checksum.update(name.encode() + b'\0' + (SOURCE / name).read_bytes())
    identity = checksum.hexdigest()
    (SOURCE / 'version.h').write_text('#pragma once\nnamespace FlintRender { inline constexpr const char* RendererId = "' + identity + '"; }\n')
    for name in [*NAMES, 'version.h', 'fonts/Lato-LICENSE.txt']:
        out = TARGET / name
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(SOURCE / name, out)
    print('Shared renderer:', identity)

if __name__ == '__main__':
    main()
