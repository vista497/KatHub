"""
package_staging.py -- KatHub staging builder for Inno Setup.
Copies the built backend binary, runs windeployqt, and gathers all
supporting files (frontend static, prompt templates, MSVC runtimes)
into a flat staging/ directory that installer.iss consumes.

Usage:
    python installer/package_staging.py [--build-dir BUILD_DIR]

Defaults:
    --build-dir  build_p5
"""

import argparse
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# -- Paths (all relative to project root) -----------------------
WINDEPLOYQT = r"C:\Qt_new\6.7.3\msvc2022_64\bin\windeployqt.exe"
SYSTEM32 = r"C:\Windows\System32"

MSVC_DLLS = [
    "msvcp140.dll",
    "msvcp140_1.dll",
    "msvcp140_2.dll",
    "msvcp140_atomic_wait.dll",
    "msvcp140_codecvt_ids.dll",
    "vcruntime140.dll",
    "vcruntime140_1.dll",
    "vcruntime140_threads.dll",
    "vccorlib140.dll",
    "concrt140.dll",
]


def banner(text):
    print(f"\n=== {text} ===")


def fail(msg):
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def ensure_dir(path):
    os.makedirs(path, exist_ok=True)


def copy_tree(src, dst, label=""):
    """Copy a directory tree, overwriting existing files."""
    if not os.path.isdir(src):
        fail(f"{label}: source directory not found: {src}")
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)
    count = sum(1 for _ in _walk_files(dst))
    print(f"  {label}: copied {count} file(s) -> {dst}")


def copy_file(src, dst, label=""):
    """Copy a single file, creating parent dirs as needed."""
    if not os.path.isfile(src):
        fail(f"{label}: source file not found: {src}")
    ensure_dir(os.path.dirname(dst))
    shutil.copy2(src, dst)
    print(f"  {label}: {src} -> {dst}")


def _walk_files(root_dir):
    for dirpath, _, filenames in os.walk(root_dir):
        for fn in filenames:
            yield os.path.join(dirpath, fn)


def get_dir_size_mb(path):
    total = 0
    for fpath in _walk_files(path):
        try:
            total += os.path.getsize(fpath)
        except OSError:
            pass
    return total / (1024 * 1024)


def run_windeployqt(staging_dir):
    """Run windeployqt on the staged exe to pull in Qt DLLs."""
    exe = os.path.join(staging_dir, "kathub-backend.exe")
    if not os.path.isfile(exe):
        fail(f"Binary not found in staging: {exe}")

    cmd = [WINDEPLOYQT, exe, "--no-translations", "--no-compiler-runtime"]
    print(f"  Running: {' '.join(cmd)}")
    result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)

    if result.stdout.strip():
        print(result.stdout.strip())
    if result.stderr.strip():
        print(result.stderr.strip(), file=sys.stderr)

    if result.returncode != 0:
        fail(f"windeployqt exited with code {result.returncode}")

    print("  windeployqt: OK")


def main():
    parser = argparse.ArgumentParser(description="KatHub staging builder")
    parser.add_argument(
        "--build-dir",
        default="build_p5",
        help="CMake build directory (default: build_p5)",
    )
    args = parser.parse_args()

    build_dir = os.path.join(ROOT, args.build_dir)
    staging_dir = os.path.join(ROOT, "installer", "staging")

    # ---------------------------------------------------------------
    # 1. Clean and recreate staging
    # ---------------------------------------------------------------
    banner("Cleaning staging directory")
    if os.path.isdir(staging_dir):
        shutil.rmtree(staging_dir)
        print(f"  Removed: {staging_dir}")
    ensure_dir(staging_dir)
    print(f"  Created: {staging_dir}")

    # ---------------------------------------------------------------
    # 2. Copy kathub-backend.exe
    # ---------------------------------------------------------------
    banner("Copying backend binary")
    src_exe = os.path.join(build_dir, "backend", "Debug", "kathub-backend.exe")
    dst_exe = os.path.join(staging_dir, "kathub-backend.exe")
    copy_file(src_exe, dst_exe, "kathub-backend.exe")

    # ---------------------------------------------------------------
    # 3. Run windeployqt
    # ---------------------------------------------------------------
    banner("Running windeployqt (Qt 6.7.3)")
    run_windeployqt(staging_dir)

    # ---------------------------------------------------------------
    # 4. Copy frontend static
    # ---------------------------------------------------------------
    banner("Copying frontend static (Vue)")
    src_static = os.path.join(ROOT, "backend", "static")
    dst_static = os.path.join(staging_dir, "static")
    copy_tree(src_static, dst_static, "static/")

    # ---------------------------------------------------------------
    # 5. Copy prompt templates
    # ---------------------------------------------------------------
    banner("Copying prompt templates")
    src_tpl = os.path.join(ROOT, "backend", "prompts", "templates")
    dst_tpl = os.path.join(staging_dir, "templates")
    copy_tree(src_tpl, dst_tpl, "templates/")

    # ---------------------------------------------------------------
    # 6. Copy MSVC runtime DLLs from System32
    # ---------------------------------------------------------------
    banner("Copying MSVC runtime DLLs")
    for dll in MSVC_DLLS:
        src = os.path.join(SYSTEM32, dll)
        dst = os.path.join(staging_dir, dll)
        if os.path.isfile(src):
            copy_file(src, dst, dll)
        else:
            print(f"  WARNING: not found in System32: {dll}")

    # ---------------------------------------------------------------
    # Summary
    # ---------------------------------------------------------------
    size_mb = get_dir_size_mb(staging_dir)
    file_count = sum(1 for _ in _walk_files(staging_dir))
    banner("Staging complete")
    print(f"  Staging dir:   {staging_dir}")
    print(f"  Total files:   {file_count}")
    print(f"  Total size:    {size_mb:.1f} MB")


if __name__ == "__main__":
    main()
