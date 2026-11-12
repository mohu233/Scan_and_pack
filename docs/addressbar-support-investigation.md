# 地址栏环境标识支持核查

核查日期：2026-11-10。仅检查本机软件静态文件；没有更改浏览器、扩展或账号配置，没有执行目标程序中的 JavaScript。

## 已确认的支持

紫鸟扩展使用浏览器新增的 `chrome.superbrowser` API 更新地址栏文字。扩展负责组织文字及设置弹层，紫鸟浏览器负责提供原生地址栏支持。这个接口名已从实际扩展调用和浏览器 DLL 内嵌 API schema 两处相互核对。

| 接口 | DLL 中声明的参数 | 作用及证据范围 |
| --- | --- | --- |
| `chrome.superbrowser.setProductName(name)` | `name: string` | 扩展后台生成环境标识文字后调用此接口。 |
| `chrome.superbrowser.setCustomLocationTextVisible(visible)` | `visible: boolean` | DLL 注册的自定义地址栏文字显示控制接口。尚未运行调用。 |
| `chrome.superbrowser.getCustomLocationTextVisible(callback)` | 回调参数 `result: boolean` | DLL 注册的显示状态读取接口。尚未运行调用。 |
| `chrome.superbrowser.showNativePageInfoDialog()` | 无参数 | `product.js` 的“更多设置”点击处理调用此接口，随后关闭弹层。 |

地址栏文字的后台生成逻辑使用 `name`、`platform`、`ip`、`ipRegion` 等字段，并依据 `isUrlPannelShowName`、`isUrlPannelShowPlatform`、`isUrlPannelShowIP`、`isUrlPannelShowIpRegion` 决定展示内容，最后以 ` | ` 拼接。该逻辑位于 `generateUrlPannelShowText`，随后由 `refreshUrlPannelShowText` 调用 `setProductName`。

截图中的弹层与扩展的 `product.html` / `js/product.js` 对应，包含账号、所属平台、设备信息、设备归属等开关。

## 客户端到浏览器的配置链

在客户端 `app.asar` 的 `entries/main.index.js` 中确认：

1. 启动配置默认设置 `force_custom_location_text: true`。
2. 客户端通过 `getPluginUrl(..., "product.html")` 生成扩展页面地址。
3. 将该页面地址写入 `Raw.product_infos.product_name_click_url`。

浏览器 DLL 中也存在 `product_name_click_url` 与 `force_custom_location_text` 字符串。因此，地址栏标签的点击入口由浏览器启动配置关联到扩展页面，而标签文字由扩展通过私有 API 更新。这里没有把配置键推断为可直接使用的命令行参数。

## 本机证据位置

- 浏览器：`<browser-root>\Core\chrome_64_142.1.2.74\chrome.dll`。
- 浏览器 SHA-256：`0244fdf23bf29440fbb7badec0670aa806574bb94e97f695f7716bef41573206`。
- DLL 的 `superbrowser` JSON schema 所在 NUL 分隔段起点：`0xE9DC6F0`；namespace 字段位于 `0xE9DC6F1`。
- DLL 内嵌后端源码路径：`chrome\browser\extensions\api\superbrowser\superbrowser_api.cc`，可定位到文件偏移 `0xE3B8004`。这只是编译产物中的源码路径提示，没有取得对应 C++ 实现源码。
- 运行中的浏览器扩展：`<profile-root>\SBP\SBBackground`。
- 扩展名称：`Ziniao Browser`；版本：`6.28.38.12`；Manifest V3；声明了 `superbrowser` 权限。
- `js/background.js`：`generateUrlPannelShowText` 定义字符偏移约 `1211235`；`refreshUrlPannelShowText` 定义字符偏移约 `1211931`。
- `js/product.js`：“更多设置”处理内调用 `showNativePageInfoDialog`，字符偏移约 `11269`。
- 客户端：`<client-root>\resources\app.asar` 中的 `entries/main.index.js`，点击页面配置字符偏移约 `147930`。

精确文件偏移和源文件字符偏移以重新运行诊断脚本生成的 JSON 为准；它们仅对应本次检查版本。

## 对当前 Qt 项目的含义

当前项目启动的是系统 Chrome/Edge。对照检查的普通 Chrome `146.0.7680.154` DLL 没有发现 `superbrowser` schema 及上述接口注册字符串；这不能外推为所有浏览器版本的完整能力证明。

要实现同样的地址栏标识，需要浏览器提供等价支持，再由项目扩展传入环境名称与经过代理验证的出口 IP。仅添加扩展 manifest 的 `superbrowser` 权限不会给系统 Chrome 增加对应的原生接口。Chromium 142 上游的地址栏布局入口可参考 [LocationBarView](https://chromium.googlesource.com/chromium/src/+/refs/tags/142.0.7444.134/chrome/browser/ui/views/location_bar/location_bar_view.cc)。

以下是支持该接口的浏览器扩展环境中的示意调用，未在用户现有浏览器上执行：

```js
const api = globalThis.chrome?.superbrowser;
if (typeof api?.setProductName === "function") {
  api.setProductName([environmentName, verifiedExitIp].filter(Boolean).join(" | "));
}
```

仍需验证：接口是否限制扩展 ID/安装类型、独立启动所需配置及授权条件。尚未尝试将紫鸟内核接入本项目，也没有修改或替换现有 Chrome。

## 复查方式

运行 `tools/inspect_browser_addressbar.py --help` 查看参数。传入 `--kernel`、`--extension`、`--compare-kernel`、`--client-asar` 可以复查以上静态证据；`--output` 仅写指定的诊断报告。脚本不会读取 Cookie、登录状态或账号运行配置。
