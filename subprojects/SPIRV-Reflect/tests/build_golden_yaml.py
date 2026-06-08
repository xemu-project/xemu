# Usage:
#   Prereq: build spirv-reflect
#   Prereq: build shader SPVs
#   python tests/build_golden_yaml.py
import argparse
import os
import pathlib
import platform
import subprocess
import sys

if __name__ == "__main__":
  parser = argparse.ArgumentParser(description="Generate golden YAML from test shader .spv files")
  parser.add_argument("--verbose", "-v", help="enable verbose output", action='store_true')
  args = parser.parse_args()

  print("""\
WARNING: This script regenerates the golden YAML output for all test shaders.
The new YAML will be considered the expected correct output for future test
runs. Before commiting the updated YAML to GitHub, it is therefore critical
to carefully inspect the diffs between the old and new YAML output, to ensure
that all differences can be traced back to intentional changes to either the
reflection code or the test shaders.
""")

  test_dir = pathlib.Path(__file__).parent.resolve()
  root_dir = test_dir.parent.resolve()

  spirv_reflect_exe_paths_windows = [
    os.path.join(root_dir, "bin", "Debug", "spirv-reflect.exe"),
    os.path.join(root_dir, "bin", "Release", "spirv-reflect.exe"),
  ]
  spirv_reflect_exe_paths_unix = [
    os.path.join(root_dir, "bin", "spirv-reflect"),
  ]
  spirv_reflect_exe = None
  if platform.system() == "Windows":
    for path in spirv_reflect_exe_paths_windows:
      if os.path.isfile(path):
        spirv_reflect_exe = path
        break
  else:
    for path in spirv_reflect_exe_paths_unix:
      if os.path.isfile(path):
        spirv_reflect_exe = path
        break

  if spirv_reflect_exe is None:
    exit("spirv-reflect executable not found!")

  spv_paths = []
  for root, dirs, files in os.walk(test_dir):
    for f in files:
      base, ext = os.path.splitext(f)
      if ext.lower() == ".spv":
        spv_paths.append(os.path.normpath(os.path.join(root, f)))

  for spv_path in spv_paths:
    yaml_path = spv_path + ".yaml"
    try:
      # TODO Replace hard-coded EXE path with something less brittle.
      yaml_cmd_args = [spirv_reflect_exe, "-y", "-v", "1", spv_path]
      if args.verbose:
        print(" ".join(yaml_cmd_args))
      subprocess.run(yaml_cmd_args, stdout=open(yaml_path, "w"))
      subprocess.run(yaml_cmd_args)
      print("%s -> %s" % (spv_path, yaml_path))
    except NameError:
      print("spirv-reflect application not found; did you build it first?")
      sys.exit()
    except subprocess.CalledProcessError as error:
      print("YAML generation failed with error code %d:\n%s" % (error.returncode, error.output.decode('utf-8')))
