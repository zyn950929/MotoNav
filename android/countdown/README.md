# MotoNav 读秒版 8.3.26-single.32-fresh.1

这是 single.32-fresh.1 五栏精简版的完整自主源码与构建工具，和 [普通版 8.3.26](../standard/README.md) 分开维护。

[下载源码 ZIP](https://github.com/zyn950929/MotoNav/releases/download/motonav-2026.11.01/MotoNav-Android-Countdown-8.3.26-single.32-fresh.1-Source.zip) · [统一下载页](https://github.com/zyn950929/MotoNav/releases/tag/motonav-2026.11.01)

## 功能与版本范围

- 五个栏目：**首页搜索、圆屏显示、导航语音、圆屏设置、地图 Key**；沿用精简界面，不额外堆放调试状态信息。
- 网页摩托车 / 驾车 / 自行车，路线选择与模拟；模拟速度 60 / 100 / 120 km/h。
- 最多三个途经点、搜索加油站、收藏和历史记录；导航中返回设置与恢复。
- 右侧三栏红绿灯图标与秒数，保留 single.32 的后台和停车读秒处理。
- 省电优化基于 [nongxl 的 PR #9](https://github.com/zyn950929/MotoNav/pull/9) 并有本项目兼容修正，未量测可承诺的耗电比例。

采用指定 Fresh 正式包对应源码，保留五栏、减少调试信息。移除后来叠加的读秒端 BLE v2 / Deflate / XOR / 媒体图片实验，不把后续 PR 的功能写成此版本能力。普通版目录不变，历史提交保留。

**读秒属于实验功能，不保证一直有效；缺少方向或秒数时不补造数据。以现场信号为准。**

## 新手：复制给 WorkBuddy

```text
从 https://github.com/zyn950929/MotoNav 的 main 分支获取最新源码，打包读秒版 8.3.26-single.32-fresh.1，工程在 android/countdown。
准备 Python 3、JDK 17、Android SDK 35/build-tools 35.0.0、Gradle 8.10.2、apktool 2.12.1、Zig 0.13.0；下载慢时使用可信国内镜像。
使用根目录 Build-Countdown.ps1 或“一键打包-读秒版.bat”完成完整构建，让脚本自动获取并校验 provider.apk。不要只交付 Gradle 中间 APK。
最终包名保持 com.motonav.app.fresh。已有签名就沿用，没有就生成并保存我的本机签名；不要上传签名或 Key。
请检查完整安装包、告诉我 APK 路径、包名和签名证书的十六进制 SHA1，并保存 Application-info.txt。
```

用最终包名和证书 SHA1 申请本人高德 Android Key，填入 App。天地图 Key 不再需要。覆盖升级必须沿用原包名和签名；私下备份 `.local/debug.keystore`。

## 开发者：完整构建

在仓库根目录运行 `一键打包-读秒版.bat`，按提示提供工具路径。也可在本目录运行：

```powershell
python build-countdown.py --apktool 'D:\Tools\apktool.jar' --gradle 'D:\Tools\gradle-8.10.2\bin\gradle.bat' --sdk 'D:\Android\Sdk' --java-home 'D:\Tools\jdk-17' --zig 'D:\Tools\zig-0.13.0\zig.exe'
```

入口自动下载 [provider.apk](https://github.com/zyn950929/MotoNav/releases/download/countdown-provider-9.5.13/provider.apk) 到 `inputs/`，按 `provider.json` 校验大小和 SHA256。该文件是约 148 MiB 的第三方构建依赖，**不要直接装它代替成品**；不属于本项目自主代码许可。

下载失败时手动放到 `inputs/provider.apk`，或加 `--provider` 指定位置。只有所有依赖都已缓存才加 `--offline`。工具使用本机新生成的签名；不附带维护者 Key、签名或登录资料。

成品：`output/MotoNav-8.3.26-single.32-fresh.1-release.apk`，身份信息：`output/Application-info.txt`。完整包系统版本名保留兼容组件要求，MotoNav 内显示 single.32-fresh.1；更新编号为 `26000032`。

`app/build/outputs/` 内 APK 仅用于源码编译验证，不能当完整安装包。`inputs/`、`.local/`、集成生成目录和 `output/` 都不提交 Git。

## 首次使用和验证

按提示允许定位、附近设备 / 蓝牙及必要通知，完成额外数据组件首次初始化，再连接圆屏。若没有读秒，先检查初始化、定位、网络、后台权限和当地覆盖。普通模拟导航不保证产生真实读秒。

源码回归：JDK 17 下运行 `python tests/run.py`；下载校验：`python tests/test_provider_download.py`。本次完成完整构建与策略回归；未重新做完整实路、耗电、温度测试。

[本次更新与验证范围](../../docs/RELEASE-2026-11-01.md) · [许可](../../LICENSE.md) · [第三方说明](../../licenses/)
