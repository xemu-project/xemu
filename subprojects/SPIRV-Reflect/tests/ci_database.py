import os
import sys
import argparse
import subprocess

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='run SPIRV-Database in CI')
    # Main reason for passing dir in is so GitHub Actions can group things, otherwise logs get VERY long for a single action
    parser.add_argument('--dir', action='store', required=True, type=str, help='path to SPIR-V files')
    args = parser.parse_args()

    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
    exe_path = os.path.join(root_dir, 'bin/spirv-reflect')

    for currentpath, folders, files in os.walk(args.dir):
        for file in files:
            spirv_file = os.path.join(currentpath, file)
            command = f'{exe_path} {spirv_file} -ci'
            exit_code = subprocess.call(command.split())
            if exit_code != 0:
                print(f'ERROR for {spirv_file}')
                sys.exit(1)