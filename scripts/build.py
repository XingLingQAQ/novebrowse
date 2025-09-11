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
    gclient_cmd = str(depot_tools / ("gclient.bat" if is_windows else "gclient"))
    ninja_cmd = str(depot_tools / ("ninja.exe" if is_windows else "ninja"))
    gn_cmd = str(depot_tools / ("gn.bat" if is_windows else "gn"))

    # Fetch depot_tools if missing
    if not depot_tools.exists():
        run(["git", "clone", "https://chromium.googlesource.com/chromium/tools/depot_tools.git", str(depot_tools)])

    # Checkout chromium from GitHub mirror (shallow), if missing
    if not chromium_dir.exists():
        chromium_dir.mkdir(parents=True)
    src_dir = chromium_dir / "src"
    if not src_dir.exists():
        # Create .gclient that points to GitHub mirror for 'src'
        gclient_text = (
            'solutions = [\n'
            '  {\n'
            '    "name": "src",\n'
            '    "url": "https://github.com/chromium/chromium.git",\n'
            '    "deps_file": "DEPS",\n'
            '    "managed": False,\n'
            '    "custom_deps": {},\n'
            '    "custom_vars": {},\n'
            '  },\n'
            ']\n'
            'target_os = ["win"]\n'
            'target_os_only = True\n'
            'target_cpu = ["x64"]\n'
            'with_branch_heads = False\n'
            'with_tags = False\n'
        )
        (chromium_dir / ".gclient").write_text(gclient_text, encoding="utf-8")

        # Shallow clone Github mirror for src
        run([
            "git", "clone", "--depth=1", "--filter=blob:none", "--no-tags",
            "--single-branch", "--branch", "main",
            "https://github.com/chromium/chromium.git", str(src_dir)
        ])

    # Mirror current NoveBrowse repo into chromium/src/novebrowse for integration
    src_novebrowse = src_dir / "novebrowse"
    if src_novebrowse.exists():
        shutil.rmtree(src_novebrowse)
    ignore_names = shutil.ignore_patterns("_work", "artifacts", ".git", ".github", "__pycache__")
    shutil.copytree(nove_dir, src_novebrowse, ignore=ignore_names)

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

    # Apply NoveBrowse patch (best-effort)
    patch_path = src_novebrowse / "patches" / "fingerprint_core.patch"
    if patch_path.exists():
        if is_windows:
            run(f'git apply --ignore-whitespace "{patch_path}"', cwd=str(src_dir), use_shell=True, check=False)
        else:
            run(["git", "apply", "--ignore-whitespace", str(patch_path)], cwd=str(src_dir), check=False)

    # Generate and build launcher only (fast target)
    # Generate build files with GN (mb may not accept --args here in Actions)
    if is_windows:
        run(f'"{gn_cmd}" gen out/Default --args="is_debug=false is_component_build=false"', cwd=str(src_dir), use_shell=True)
    else:
        run([gn_cmd, "gen", "out/Default", "--args=is_debug=false is_component_build=false"], cwd=str(src_dir))
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


