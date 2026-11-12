# ProfilePower

当前版本：**11.11.19**。版本号统一记录在 `VERSION.txt`，供应用及安装脚本读取。变更内容见 [CHANGELOG.md](CHANGELOG.md)。

### 界面布局修复（2026-11）

环境列表的表头与七列实际数据对齐，恢复备注列；名称和备注随窗口宽度伸缩，操作列保留在右侧。操作入口统一为“打开 / 更多”，清理菜单及运行按钮中的损坏字符。侧栏、表格、按钮和空状态采用统一配色。

`ui_appearance.cpp` 集中处理界面样式及按内容调整选项卡高度。表单不再依赖样式设置前算出的固定高度，代理、时区、语言、分辨率等面板会在字体、布局和选项变化后重新测量，避免输入框重叠或被裁切。界面回归测试覆盖不同窗口尺寸下的字段边界、表格宽度和操作按钮。重新运行 qmake 并编译即可生效，无需重新制作安装包。

### 代理配置（2026-11）

新建浏览器中的“代理方式”提供“无代理”和“添加代理”。“添加代理”包含：

- **手动填写**：HTTP、HTTPS 或 SOCKS5 主机、端口，以及可选的用户名和密码。
- **导入配置文件**：可多选 WireGuard `.conf`、代理 JSON 或代理 URL 文本；“导入文件夹”会递归查找子目录中的配置。重复节点自动跳过，导入后可在下拉框中选择节点并检查网络。

使用代理打开浏览器环境后，当前浏览器的标签栏右侧显示白色圆角环境标签，例如“亚马逊-美国 | 199.87.229.56”，每分钟通过该环境正在使用的代理重新查询。环境名称过长时省略，IP 地址保留完整。首次查询显示“IP 检测中”，查询失败显示“IP 未确认”；不会把节点服务器地址或本机直连地址误标为代理出口。无代理时不显示标注。标签是直接创建在浏览器窗口内的 Windows 原生子控件（WS_CHILD），没有独立的 Qt 浮动工具窗口；它随父窗口移动、裁切、遮挡和最小化，不需要用桌面坐标追踪浏览器，也不拦截鼠标操作。当前位置是标签栏右侧，不改变原生地址栏的布局；如需参考图中地址栏内部为标签预留空间的效果，需要定制 Chromium。重新编译并重新打开浏览器环境后生效。

环境管理的 IP 列显示检测到的代理出口 IP。加载或进入环境管理时会后台检测未确认或过期的代理结果，最多同时检测两个环境；运行中的环境直接同步浏览器顶部标注的检测结果。结果保存在环境配置中，并绑定当前代理配置，修改代理后不会沿用旧节点的 IP。未检测和检测失败时显示相应状态，无代理显示“直连”，不再用 `127.0.0.1` 作为演示值。动态代理在启动并获取新节点后确认出口 IP。

WireGuard 配置必须包含 `[Interface]` 的 `PrivateKey`、`Address`、`DNS`，以及一个 `[Peer]` 的 `PublicKey`、`AllowedIPs`、`Endpoint`。支持双栈地址、多个 DNS、`MTU`、`PresharedKey` 和 `PersistentKeepalive`。不支持执行 `PreUp`/`PostUp` 等脚本，也不修改 Windows 系统代理、网卡或路由。

WireGuard 使用随安装包提供的 [wireproxy v1.1.3](https://github.com/windtf/wireproxy/releases/tag/v1.1.3) 转成本地 SOCKS5。不同节点使用独立进程，同一节点在应用内复用连接，避免多个浏览器相互争用同一个 WireGuard 会话。最后一个使用者退出后清理进程与临时配置。节点断开时不会自动切换为直连。

导入的配置保存在当前用户的应用数据目录中；每个浏览器环境另存一份节点快照，原始文件移动或删除后仍可使用。配置包含节点私钥，请勿分享应用数据目录。安装包不含个人节点或私钥。

构建安装包时，`build_package.bat` 会调用 `fetch_wireproxy.ps1` 下载固定版本的 Windows x64 组件，并核对官方发布包的 SHA-256。Qt/MinGW 构建环境仍须按下文配置。

代理功能测试（Qt/MinGW 终端）：

```bat
mkdir build\proxy-tests
cd build\proxy-tests
qmake ..\..\tests\proxy_tests.pro CONFIG+=release CONFIG-=debug
mingw32-make -j4
release\ProfilePowerProxyTests.exe
```

可通过 `PROFILEPOWER_TEST_NODE_DIR` 指定本机包含三个 `.conf` 文件的目录进行额外解析和本地代理启动测试；`PROFILEPOWER_TEST_CONNECT_NODE_FILES` 可指定以分号分隔的文件路径，通过 Qt 网络层检查真实出口 IP（测试约定配置的父文件夹名为预期 IP）。默认测试不访问真实节点。连接测试按钮会通过选中的代理查询出口 IP，不会在导入时联网。

<div align="center">

**[English](#english) | [中文](#中文)**

</div>

---

## English

### ProfilePower

ProfilePower is a Windows desktop application for managing isolated Chromium browser profiles. It combines a Qt user interface, per-profile browser data directories, a runtime extension, Chrome DevTools Protocol (CDP) helpers, and local proxy relay to provide comprehensive browser fingerprinting and automation capabilities.

> The application is under active development. Environment editing and the API page are currently placeholders, while environment creation, launch, deletion, proxy management, fingerprint injection, and module execution are functional.

#### Features

- Detect locally installed Google Chrome and Microsoft Edge browsers.
- Create isolated browser environments with independent `chrome-data` directories.
- Configure User-Agent and UA-CH metadata, language, timezone, geolocation, resolution, WebRTC, WebGL, fonts, and deterministic hardware noise.
- Manage custom, dynamic API, and integrated proxy profiles.
- Relay authenticated HTTP, HTTPS, and SOCKS5 upstream proxies through a local endpoint Chrome can use.
- Import cookies and open configured startup pages through a generated Manifest V3 extension.
- Import and run Python automation modules against a running browser's debugging port.

#### Architecture

| Component | Responsibility |
| --- | --- |
| `mainwindow.cpp` | Main UI, environment lifecycle, process management, and module runner |
| `envmanager.cpp` | Local Chrome and Edge discovery |
| `proxy_manager.cpp` | Proxy profile UI and persistence |
| `profile_runtime.cpp` | Generates the per-environment runtime extension |
| `browser_cdp_runtime.py` | Applies CDP overrides to current and newly created tabs |
| `proxy_relay_v2.py` | Local relay for authenticated HTTP/HTTPS/SOCKS5 upstream proxies |

At runtime, ProfilePower generates the extension, starts the selected browser with an isolated data directory and remote debugging port, starts the CDP helper, and optionally starts a local proxy relay.

#### Requirements

- Windows 10 or later
- Qt 6 with the Widgets and Network modules
- MinGW 64-bit toolchain
- Python 3.10 or later
- Google Chrome or Microsoft Edge
- NSIS, only when building the installer

The core CDP and proxy helpers use the Python standard library. Imported automation modules may declare their own Python dependencies.

#### Build

Open a Qt/MinGW terminal and run:

```bat
mkdir build\local
cd build\local
qmake ..\..\ProfilePower.pro CONFIG+=release CONFIG-=debug
mingw32-make -j4
```

The application executable is produced as `release\ProfilePower.exe` for a release build.

#### Package

Ensure `qmake`, `mingw32-make`, `windeployqt`, and NSIS are available, then run:

```bat
build_package.bat
```

When Qt is not on `PATH`, set `QT_DIR` to the Qt kit directory before running the script. `NSIS` may also be set to the full path of `makensis.exe`.

The installer is written to `build\ProfilePower_Setup.exe`.

#### Runtime Data

Environment and proxy data are stored outside the repository under Qt's `AppLocalDataLocation` in a `ProfilePower` directory. Each environment contains:

```text
profiles/<environment-id>/
  config.json
  chrome-data/
  runtime-extension/
```

Proxy passwords, cookies, browser profiles, logs, modules, build artifacts, and IDE-specific files are intentionally excluded by `.gitignore`.

#### Automation Modules

Modules are installed under the application's `modules/<module-id>` directory. A minimal module configuration is:

```json
{
  "id": "example-module",
  "name": "Example module",
  "env": "python",
  "runner": "python",
  "script": "main.py",
  "format": ["video", "title", "desc"]
}
```

The runner passes the active browser debugging port as `--host` and exposes the same value through the `HOST` environment variable.

#### Security

- Do not commit runtime profiles, cookies, proxy credentials, or logs.
- Treat imported modules as executable code and review them before running.
- Use test-only proxy credentials for `test_relay.py`, supplied through command-line options or `PROFILE_PROXY_*` environment variables.
- Profile deletion is restricted to the application's profile directory and is refused while that environment is running.

#### License

No open-source license has been selected yet. All rights are reserved unless a license is added by the repository owner.

---

## 中文

### ProfilePower

ProfilePower 是一个 Windows 桌面应用程序，用于管理隔离的 Chromium 浏览器配置文件。它结合了 Qt 用户界面、每个配置文件独立的浏览器数据目录、运行时扩展、Chrome DevTools Protocol (CDP) 助手和本地代理中继，提供全面的浏览器指纹识别和自动化功能。

> 该应用程序处于活跃开发阶段。环境编辑和 API 页面目前是占位符，而环境创建、启动、删除、代理管理、指纹注入和模块执行功能已可用。

#### 功能特性

- 检测本地安装的 Google Chrome 和 Microsoft Edge 浏览器。
- 创建隔离的浏览器环境，每个环境拥有独立的 `chrome-data` 目录。
- 配置用户代理 (User-Agent) 和 UA-CH 元数据、语言、时区、地理位置、分辨率、WebRTC、WebGL、字体和确定性硬件噪声。
- 管理自定义、动态 API 和集成代理配置文件。
- 通过本地端点中继已验证的 HTTP、HTTPS 和 SOCKS5 上游代理供 Chrome 使用。
- 通过生成的 Manifest V3 扩展导入 Cookie 和打开配置的启动页面。
- 针对正在运行的浏览器的调试端口导入并运行 Python 自动化模块。

#### 架构

| 组件 | 职责 |
| --- | --- |
| `mainwindow.cpp` | 主 UI、环境生命周期、进程管理和模块运行器 |
| `envmanager.cpp` | 本地 Chrome 和 Edge 发现 |
| `proxy_manager.cpp` | 代理配置文件 UI 和持久化 |
| `profile_runtime.cpp` | 生成每个环境的运行时扩展 |
| `browser_cdp_runtime.py` | 将 CDP 覆盖应用于当前和新创建的标签页 |
| `proxy_relay_v2.py` | 已验证 HTTP/HTTPS/SOCKS5 上游代理的本地中继 |

在运行时，ProfilePower 生成扩展，使用隔离的数据目录和远程调试端口启动选定的浏览器，启动 CDP 助手，并可选地启动本地代理中继。

#### 系统要求

- Windows 10 或更高版本
- 包含 Widgets 和 Network 模块的 Qt 6
- MinGW 64 位工具链
- Python 3.10 或更高版本
- Google Chrome 或 Microsoft Edge
- NSIS（仅在构建安装程序时需要）

核心 CDP 和代理助手使用 Python 标准库。导入的自动化模块可能会声明自己的 Python 依赖项。

#### 构建

打开 Qt/MinGW 终端并运行：

```bat
mkdir build\local
cd build\local
qmake ..\..\ProfilePower.pro CONFIG+=release CONFIG-=debug
mingw32-make -j4
```

发布版本构建会生成应用程序可执行文件 `release\ProfilePower.exe`。

#### 打包

确保 `qmake`、`mingw32-make`、`windeployqt` 和 NSIS 可用，然后运行：

```bat
build_package.bat
```

当 Qt 不在 `PATH` 中时，在运行脚本前将 `QT_DIR` 设置为 Qt 工具包目录。`NSIS` 也可以设置为 `makensis.exe` 的完整路径。

安装程序将写入 `build\ProfilePower_Setup.exe`。

#### 运行时数据

环境和代理数据存储在 Qt 的 `AppLocalDataLocation` 下的 `ProfilePower` 目录中，不在仓库内。每个环境包含：

```text
profiles/<environment-id>/
  config.json
  chrome-data/
  runtime-extension/
```

代理密码、Cookie、浏览器配置文件、日志、模块、构建产物和 IDE 特定文件被 `.gitignore` 有意排除。

#### 自动化模块

模块安装在应用程序的 `modules/<module-id>` 目录下。最小的模块配置是：

```json
{
  "id": "example-module",
  "name": "Example module",
  "env": "python",
  "runner": "python",
  "script": "main.py",
  "format": ["video", "title", "desc"]
}
```

运行器将活跃的浏览器调试端口作为 `--host` 传递，并通过 `HOST` 环境变量公开相同的值。

#### 安全性

- 不要提交运行时配置文件、Cookie、代理凭证或日志。
- 将导入的模块视为可执行代码，运行前进行审查。
- 对 `test_relay.py` 使用仅测试代理凭证，通过命令行选项或 `PROFILE_PROXY_*` 环境变量提供。
- 配置文件删除仅限于应用程序的配置文件目录，当该环境正在运行时拒绝删除。


