# 扫码装箱

[English](README.md)

扫码装箱是一款面向仓库和车间作业的 Windows 桌面程序。程序通过扫码枪接收10位 Amazon FNSKU，将商品记录到指定箱号，对照导入的装箱计划实时统计已装数量和差额，并可输出 Excel 装箱单。

界面采用浅色工业看板设计，重点突出当前箱号、最近扫码、已装数量和计划差额，方便操作员在仓库环境中快速确认作业状态。

## 主要功能

- 接收10位 FNSKU，扫码成功后立即清空输入框。
- 必须先创建箱号，没有箱号时禁止扫码入库并给出提示。
- 历史记录按照箱号和 FNSKU 汇总，最近更新的数据自动置顶。
- 使用大号、高对比度数字显示装箱数量，并支持“删除一个”。
- 导入包含“SKU、数量”两列的 Excel 装箱计划。
- 将索引表中的 SKU 和 MSKU 视为同一 FNSKU 的别名，兼容空格被替换为下划线的情况。
- 显示计划数量、已装数量和带正负号的差额：
  - 多装：`+N`
  - 少装或未装：`-N`
  - 数量一致：`0`
- 支持导入和输出 `.xlsx` 装箱单，并兼容 Excel/WPS 共享字符串格式。
- 输出装箱单时记录 SKU、FNSKU、数量、箱号和时间戳。
- 通过领星 ERP 接口自动分页更新 FNSKU/SKU 索引。
- token 和索引表保存在安装目录之外，升级程序时不会被覆盖。
- 扫码成功后离线语音播报“已装箱”。
- 导入和输出文件时默认打开 Windows 桌面。

## 数据保存位置

程序运行数据保存在当前用户的临时目录：

```text
%TEMP%\扫码入库\
├── token.txt
└── fnsku_sku_index.xlsx
```

文件不存在时程序会自动创建。`token.txt` 中只需填写领星 ERP 请求头里的 `auth-token`。请勿把 token、cookie 或包含店铺数据的索引表提交到 GitHub。

## 开发环境

- Windows 10 或 Windows 11
- Qt 6.11.1，或兼容的 Qt 6 版本
- MinGW 13.1 64位
- Qt Widgets、Network、OpenGL Widgets 和 Core private headers
- 生成 Windows 安装包时需要 NSIS

## 使用 Qt Creator 编译

1. 在 Qt Creator 中打开 `untitled.pro`。
2. 选择 Qt 6 MinGW 64位套件。
3. 选择 Debug 或 Release 配置。
4. 编译并运行项目。
5. 如果不是通过安装包运行，需要把 `assets/packed.wav` 复制到程序 EXE 同一目录。

## 命令行编译

在独立的 build 目录中执行，确保 Qt 和 MinGW 已加入 `PATH`：

```powershell
qmake ..\..\untitled.pro -spec win32-g++ "CONFIG+=release"
mingw32-make -j1 release
```

程序使用 Windows 多媒体库播放离线语音。

## 制作安装包

NSIS 脚本位于 `package/installer.nsi`。生成安装包前：

1. 编译 Release 版本。
2. 将主程序复制到 `package/app/扫码入库.exe`。
3. 将 `assets/packed.wav` 复制到 `package/app/packed.wav`。
4. 使用 `windeployqt` 部署 Qt 运行库。
5. 使用 UTF-8 模式编译安装脚本：

```powershell
makensis /INPUTCHARSET UTF8 package\installer.nsi
```

Git 会自动忽略 build 目录、Qt 运行库、编译后的程序和安装包 EXE。

## 项目结构

```text
Scan_and_pack/
├── assets/                离线语音资源
├── package/installer.nsi  NSIS 安装脚本
├── main.cpp               程序入口
├── mainwindow.cpp         业务逻辑和界面行为
├── mainwindow.h           主窗口声明
├── mainwindow.ui          Qt Designer 界面
├── untitled.pro           qmake 项目配置
└── untitled_zh_CN.ts      Qt 翻译源文件
```

## 安全说明

- 不要提交 token、cookie、请求头或包含店铺信息的索引表。
- 输出的装箱单属于业务数据，应按仓库数据管理要求妥善保存。
- 领星 token 过期后，可通过程序中的“领星cookie”按钮打开并更新 `token.txt`。

