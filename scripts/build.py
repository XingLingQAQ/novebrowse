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


def run(cmd, cwd=None, check=True, use_shell=False):
    if isinstance(cmd, (list, tuple)):
        printable = " ".join(map(str, cmd))
    else:
        printable = str(cmd)
    print("+", printable)
    result = subprocess.run(cmd, cwd=cwd, check=check, shell=use_shell)
    return result.returncode


def ensure_dir(p: Path):
    p.mkdir(parents=True, exist_ok=True)


def main():
    # 在 GitHub Actions 中优先使用 GITHUB_WORKSPACE；否则退回到脚本上级目录（novebrowse）
    workspace = os.environ.get("GITHUB_WORKSPACE")
    if workspace:
        repo_root = Path(workspace)
    else:
        # repo_root 指向包含 novebrowse 的仓库根目录
        repo_root = Path(__file__).resolve().parents[1]

    # 所有工作目录均放在 novebrowse 下，避免越界
    nove_dir = repo_root if (repo_root / "BUILD.gn").exists() else Path(__file__).resolve().parents[1]
    work_root = nove_dir / "_work"
    build_dir = work_root  # 统一工作根
    chromium_dir = work_root / "chromium"
    depot_tools = work_root / "depot_tools"

    ensure_dir(build_dir)

    os.environ.setdefault("DEPOT_TOOLS_WIN_TOOLCHAIN", "0")
    # 追加 depot_tools 到 PATH
    os.environ["PATH"] = str(depot_tools) + os.pathsep + os.environ.get("PATH", "")

    is_windows = os.name == "nt"
    fetch_cmd = str(depot_tools / ("fetch.bat" if is_windows else "fetch"))
    gclient_cmd = str(depot_tools / ("gclient.bat" if is_windows else "gclient"))
    ninja_cmd = str(depot_tools / ("ninja.exe" if is_windows else "ninja"))

    # Fetch depot_tools if missing
    if not depot_tools.exists():
        run(["git", "clone", "https://chromium.googlesource.com/chromium/tools/depot_tools.git", str(depot_tools)])

    # Fetch chromium (shallow), if missing
    if not chromium_dir.exists():
        chromium_dir.mkdir(parents=True)
        # 在 Windows 上通过 cmd 执行 .bat，避免找不到可执行的问题
        if is_windows:
            run(f'"{fetch_cmd}" --nohooks --no-history chromium', cwd=str(chromium_dir), use_shell=True)
        else:
            run([fetch_cmd, "--nohooks", "--no-history", "chromium"], cwd=str(chromium_dir))

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
    if is_windows:
        run(f'"{gclient_cmd}" sync --nohooks --no-history --shallow', cwd=str(src_dir), use_shell=True)
    else:
        run([gclient_cmd, "sync", "--nohooks", "--no-history", "--shallow"], cwd=str(src_dir))
    # run hooks
    if is_windows:
        run(f'"{gclient_cmd}" runhooks', cwd=str(src_dir), use_shell=True)
    else:
        run([gclient_cmd, "runhooks"], cwd=str(src_dir))

    # Generate and build launcher only (fast target)
    run([sys.executable, "tools/mb/mb.py", "gen", "out/Default", "--args=is_debug=false is_component_build=false"], cwd=str(src_dir))
    if Path(ninja_cmd).exists():
        if is_windows:
            run(f'"{ninja_cmd}" -C out/Default build_novebrowse', cwd=str(src_dir), use_shell=True)
        else:
            run([ninja_cmd, "-C", "out/Default", "build_novebrowse"], cwd=str(src_dir))
    else:
        run(["ninja", "-C", "out/Default", "build_novebrowse"], cwd=str(src_dir))

    out = src_dir / "out/Default/build_novebrowse.exe"
    artifacts = nove_dir / "artifacts"
    ensure_dir(artifacts)
    shutil.copy2(out, artifacts / "build_novebrowse.exe")
    print("Artifacts at:", artifacts)


if __name__ == "__main__":
    sys.exit(main())


