#!/usr/bin/env python3
"""Production-source Death show/fortune/assets sanitizer tests, no hardware.

The fortune test uses the cJSON source bundled with the selected ESP-IDF checkout.
Set IDF_PATH (or pass --idf-path); no external Python dependencies are needed.
"""
import argparse
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--idf-path',default=os.environ.get('IDF_PATH'))
    args=parser.parse_args()
    if not args.idf_path:
        parser.error('supply --idf-path or export IDF_PATH for SDK cJSON source')
    cjson=Path(args.idf_path)/'components/json/cJSON'
    if not (cjson/'cJSON.c').is_file():
        parser.error(f'cJSON source unavailable at {cjson}')
    def run(cmd):subprocess.run(cmd,cwd=ROOT,check=True)
    flags=['-std=c++17','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
           '-Inative_scripts/tests/show_stubs','-Ifirmware/main']
    with tempfile.TemporaryDirectory(prefix='death-host-') as temp:
        temp=Path(temp)
        executable=temp/'show'
        run(shlex.split(os.environ.get('CXX','c++'))+flags+
            ['native_scripts/tests/test_death_show.cpp','firmware/main/death_show.cpp','-o',str(executable)])
        run([str(executable)])
        executable=temp/'assets'
        run(shlex.split(os.environ.get('CXX','c++'))+flags+
            ['native_scripts/tests/test_death_assets.cpp','firmware/main/skull_catalog.cpp',
             'firmware/main/skull_wave.cpp','-o',str(executable)])
        run([str(executable)])
        obj=temp/'cjson.o'
        run(shlex.split(os.environ.get('CC','cc'))+['-Wno-deprecated-declarations','-fsanitize=address,undefined',
            '-c',str(cjson/'cJSON.c'),'-o',str(obj)])
        executable=temp/'fortune'
        run(shlex.split(os.environ.get('CXX','c++'))+flags+['-I'+str(cjson),
            'native_scripts/tests/test_death_fortune.cpp','firmware/main/death_fortune.cpp',str(obj),'-o',str(executable)])
        run([str(executable)])

if __name__=='__main__':main()
