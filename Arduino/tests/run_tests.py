"""Run host firmware tests with a native C++17 compiler; no Arduino packages needed."""

import argparse
import pathlib
import shutil
import subprocess
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--compiler", help="Path to native g++, clang++ or zig")
args = parser.parse_args()
compiler = args.compiler or shutil.which("g++") or shutil.which("clang++") or shutil.which("zig")
if not compiler:
    parser.error("Install native g++, clang++ or Zig, or pass --compiler PATH")
root = pathlib.Path(__file__).resolve().parent
command = [compiler]
if pathlib.Path(compiler).stem.lower() == "zig":
    command.append("c++")
with tempfile.TemporaryDirectory(prefix="dialed-firmware-tests-") as directory:
    for name, source, flags in [
        ("touch_tests", "touch_tests.cpp", []),
        ("free_space_tests", "free_space_tests.cpp", []),
        ("idle_playback_tests", "idle_playback_tests.cpp", []),
        ("media_mixer", "media_profile_tests.cpp", []),
        ("media_touch", "media_profile_tests.cpp", ["-DDIALED_CONTROLLER_TOUCH=1"]),
        ("protocol_mixer", "protocol_tests.cpp", []),
        ("protocol_touch", "protocol_tests.cpp", ["-DDIALED_CONTROLLER_TOUCH=1"]),
    ]:
        binary = pathlib.Path(directory) / (name + ".exe")
        print(f"Running {name}", flush=True)
        subprocess.run(
            command
            + ["-std=c++17", "-Wall", "-Wextra", "-Werror"]
            + flags
            + ["-I", str(root / "stubs"), str(root / source), "-o", str(binary)],
            check=True,
        )
        subprocess.run([str(binary)], check=True)
