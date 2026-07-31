"""
package_staging.py -- KatHub staging builder for Inno Setup.
Copies the built backend binary, runs windeployqt, and gathers all
supporting files (frontend static, prompt templates, MSVC runtimes)
into a flat staging/ directory that installer.iss consumes.

Usage:
    python installer/package_staging.py [--build-dir BUILD_DIR] [--qt-bin-dir QT_BIN_DIR] [--release]

Defaults:
    --build-dir  build_p5
    --release    (use Release instead of Debug)
"""
import argparse
import os
import shutil
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

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


def find_exe(build_dir, name, config):
    """Find .exe in build tree: try Release/Debug first, then rglob."""
    candidates = [
        os.path.join(build_dir, config, f"{name}.exe"),
        os.path.join(build_dir, "backend", config, f"{name}.exe"),
        os.path.join(build_dir, "bin", config, f"{name}.exe"),
        os.path.join(build_dir, f"{name}.exe"),
        os.path.join(build_dir, "bin", f"{name}.exe"),
    ]
    for c in candidates:
        if os.path.isfile(c):
            return c
    matches = list(Path(build_dir).rglob(f"{name}.exe"))
    if matches:
        return str(matches[0])
    fail(f"{name}.exe not found in {build_dir}")


def resolve_windeployqt(qt_bin_dir=None):
    """Find windeployqt.exe from --qt-bin-dir, CMakeCache, or fallback."""
    import re
    from pathlib import Path

    if qt_bin_dir:
        path = os.path.join(qt_bin_dir, "windeployqt.exe")
        if os.path.isfile(path):
            return path

    # Try CMakeCache
    for cache_dir in ["build", "build_p5", "build_new", "build2"]:
        cache = os.path.join(ROOT, cache_dir, "CMakeCache.txt")
        if os.path.isfile(cache):
            text = Path(cache).read_text(encoding="utf-8", errors="replace")
            m = re.search(r'CMAKE_PREFIX_PATH:PATH=(.+)', text)
            if m:
                path = Path(m.group(1).strip()) / "bin" / "windeployqt.exe"
                if path.is_file():
                    return str(path)

    # Fallback: hardcoded local path
    fallback = r"C:\Qt_new\6.7.3\msvc2022_64\bin\windeployqt.exe"
    if os.path.isfile(fallback):
        return fallback

    # System PATH
    import shutil as _shutil
    found = _shutil.which("windeployqt")
    if found:
        return found

    fail("windeployqt not found. Use --qt-bin-dir or ensure Qt is installed.")


def run_windeployqt(staging_dir, windeployqt_path):
    """Run windeployqt on the staged exe to pull in Qt DLLs."""
    exe = os.path.join(staging_dir, "kathub-backend.exe")
    if not os.path.isfile(exe):
        fail(f"Binary not found in staging: {exe}")

    cmd = [windeployqt_path, exe, "--no-translations", "--no-compiler-runtime"]
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
        "--build-dir", default="build_p5",
        help="CMake build directory (default: build_p5)",
    )
    parser.add_argument(
        "--qt-bin-dir", default=None,
        help="Path to Qt bin directory (e.g. C:/Qt/6.7.3/msvc2022_64/bin)",
    )
    parser.add_argument(
        "--release", action="store_true",
        help="Use Release build (default: Debug)",
    )
    args = parser.parse_args()

    build_dir = os.path.join(ROOT, args.build_dir)
    staging_dir = os.path.join(ROOT, "installer", "staging")
    config = "Release" if args.release else "Debug"
    WINDEPLOYQT = resolve_windeployqt(args.qt_bin_dir)

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
    banner(f"Copying backend binary ({config})")
    src_exe = find_exe(build_dir, "kathub-backend", config)
    dst_exe = os.path.join(staging_dir, "kathub-backend.exe")
    copy_file(src_exe, dst_exe, "kathub-backend.exe")

    # ---------------------------------------------------------------
    # 3. Run windeployqt
    # ---------------------------------------------------------------
    banner("Running windeployqt")
    run_windeployqt(staging_dir, WINDEPLOYQT)

    # ---------------------------------------------------------------
    # 4. Copy frontend static
    # ---------------------------------------------------------------
    banner("Copying frontend static (Vue)")
    # CI: dist is in staging/static/ (downloaded artifact)
    # Local: vite build собирает СРАЗУ в backend/static (см. frontend/vite.config.ts)
    ci_static = os.path.join(staging_dir, "static", "index.html")
    if os.path.isfile(ci_static):
        print("  Using CI artifact (already in staging/static/)")
    else:
        src_static = os.path.join(ROOT, "backend", "static")
        dst_static = os.path.join(staging_dir, "static")
        copy_tree(src_static, dst_static, "static/")

    # ---------------------------------------------------------------
    # 5. Copy prompt templates
    # ---------------------------------------------------------------
    banner("Copying prompt templates")
    src_tpl = os.path.join(ROOT, "backend", "prompts", "templates")
    dst_tpl = os.path.join(staging_dir, "templates")
    if os.path.isdir(src_tpl):
        copy_tree(src_tpl, dst_tpl, "templates/")
    else:
        print(f"  SKIP: templates dir not found ({src_tpl})")

    # ---------------------------------------------------------------
    # 6. Copy helper scripts (kanban_move.py — drag&drop status change)
    # ---------------------------------------------------------------
    banner("Copying helper scripts")
    src_scripts = os.path.join(ROOT, "backend", "scripts")
    dst_scripts = os.path.join(staging_dir, "scripts")
    if os.path.isdir(src_scripts):
        copy_tree(src_scripts, dst_scripts, "scripts/")
    else:
        print(f"  SKIP: scripts dir not found ({src_scripts})")

    # ---------------------------------------------------------------
    # 7. Copy MSVC runtime DLLs from System32
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


from pathlib import Path

if __name__ == "__main__":
    main()
