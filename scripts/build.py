#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
GitHub Actions 构建脚本（示例）

职责：
- 校验环境
- 同步/更新 Chromium（可选：浅历史）
- 使用 GN/Ninja 构建 NoveBrowse 目标（本例构建 build_novebrowse.exe）
"""

import os
import sys
import subprocess
import shutil
from pathlib import Path


def run(cmd, cwd=None, check=True):
    print("+", " ".join(cmd))
    result = subprocess.run(cmd, cwd=cwd, check=check)
    return result.returncode


def ensure_dir(p: Path):
    p.mkdir(parents=True, exist_ok=True)


def main():
    project_root = Path(__file__).resolve().parents[3]  # .../browse/
    nove_dir = project_root / "novebrowse"
    build_dir = project_root / "build"
    chromium_dir = build_dir / "chromium"
    depot_tools = build_dir / "depot_tools"

    ensure_dir(build_dir)

    os.environ.setdefault("DEPOT_TOOLS_WIN_TOOLCHAIN", "0")
    os.environ["PATH"] = str(depot_tools) + os.pathsep + os.environ["PATH"]

    # Fetch depot_tools if missing
    if not depot_tools.exists():
        run(["git", "clone", "https://chromium.googlesource.com/chromium/tools/depot_tools.git", str(depot_tools)])

    # Fetch chromium (shallow), if missing
    if not chromium_dir.exists():
        chromium_dir.mkdir(parents=True)
        # fetch must run in directory
        run(["fetch", "--nohooks", "--no-history", "chromium"], cwd=str(chromium_dir))

    # Configure platform-only in .gclient
    gclient_file = chromium_dir / ".gclient"
    if gclient_file.exists():
        content = gclient_file.read_text(encoding="utf-8")
        append = []
        if "target_os" not in content:
            append.append('target_os = ["win"]\n')
        if "target_os_only" not in content:
            append.append('target_os_only = True\n')
        if "target_cpu" not in content:
            append.append('target_cpu = ["x64"]\n')
        if append:
            with gclient_file.open("a", encoding="utf-8") as f:
                f.writelines(append)

    src_dir = chromium_dir / "src"
    # sync
    run(["gclient", "sync", "--nohooks", "--no-history", "--shallow"], cwd=str(src_dir))
    # run hooks
    run(["gclient", "runhooks"], cwd=str(src_dir))

    # Generate and build launcher only (fast target)
    run([sys.executable, "tools/mb/mb.py", "gen", "out/Default", "--args=is_debug=false is_component_build=false"], cwd=str(src_dir))
    run(["ninja", "-C", "out/Default", "build_novebrowse"], cwd=str(src_dir))

    out = src_dir / "out/Default/build_novebrowse.exe"
    artifacts = build_dir / "artifacts"
    ensure_dir(artifacts)
    shutil.copy2(out, artifacts / "build_novebrowse.exe")
    print("Artifacts at:", artifacts)


if __name__ == "__main__":
    sys.exit(main())


