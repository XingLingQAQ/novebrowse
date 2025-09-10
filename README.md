## NoveBrowse 扩展说明

本目录提供 NoveBrowse 的指纹防护组件与构建脚本，并新增 Windows 任务栏窗口序号徽标能力与 CI 构建示例。

### 窗口序号徽标（Windows）

在启动浏览器时可通过参数为任务栏图标添加一个数字徽标（1~99）。示例：

```bat
rem 使用启动器执行 chrome.exe，并在任务栏图标上显示数字 3
build_novebrowse.exe --window-badge=3 --chrome-path "path\to\chrome.exe" --enable-novebrowse-fingerprint
```

- `--window-badge`：设置任务栏图标上的数字；小于等于 0 则清除徽标。
- `--chrome-path`：可选，指定 `chrome.exe` 路径；若省略且启动器与 `chrome.exe` 同目录，会自动定位。
- 其他参数将原样转发给 `chrome.exe`。

实现位于：
- `src/windows/taskbar_badge.h`：创建带数字的叠加图标并应用到任务栏。
- `build/build_main.cc`：简单启动器，在拉起 `chrome.exe` 后查找主窗口并设置徽标。

### 构建说明（本地）

在 Chromium 源码根（`src` 旁）执行 GN 生成并构建：

```bat
python tools\mb\mb.py gen out\Default --args="is_debug=false is_component_build=false"
ninja -C out\Default build_novebrowse
```

产物：`out\Default\build_novebrowse.exe`

### GitHub Actions CI（示例）

在 `novebrowse/ci` 提供：

- `scripts/build.py`：拉取/同步/构建 Chromium 与 NoveBrowse 目标（仅示例，需匹配仓库权限）。
- `.github/workflows/build.yml`：调用 `build.py` 进行构建并上传产物。

> 注意：Chromium 完整构建体量与时长较大，建议在自托管 Runner 或使用预置缓存/镜像进行。

### 指纹防护组件

核心代码位于 `src/`，并通过 `BUILD.gn` 汇入：

- `fingerprint_protection` 组件
- 浏览器/渲染进程集成
- 配置文件复制规则

### 参数与示例

```bat
rem 仅设置徽标
build_novebrowse.exe --window-badge 7

rem 指定 chrome.exe 路径与传递其它浏览器参数
build_novebrowse.exe --window-badge=12 --chrome-path "D:\\chromium\\src\\out\\Default\\chrome.exe" --user-data-dir="D:\\profiles\\p1"

rem 清除徽标
build_novebrowse.exe --window-badge 0
```


