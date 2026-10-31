# MotoNav

手机配合 ESP32-S3 圆屏使用的导航工具。安卓分普通版和读秒版，按需选择；iPhone 使用独立 iOS App。

## 下载与版本

**[打开下载页](https://github.com/zyn950929/MotoNav/releases/tag/motonav-2026.11.01)**，在 Assets 中选择对应文件。

| 需要什么 | 当前版本 | 下载 / 说明 |
| --- | --- | --- |
| 安卓普通版，不带读秒 | **8.3.26**，保持原版 | [普通版源码与打包](android/standard/README.md) |
| 安卓读秒版 | **8.3.26-single.32-fresh.1**，五栏精简 UI | [源码 ZIP](https://github.com/zyn950929/MotoNav/releases/download/motonav-2026.11.01/MotoNav-Android-Countdown-8.3.26-single.32-fresh.1-Source.zip) · [自行打包](android/countdown/README.md) |
| 1.75 圆屏，Windows 烧录 | **7.12.10-pages.5** | [Windows 一键烧录 ZIP](https://github.com/zyn950929/MotoNav/releases/download/motonav-2026.11.01/MotoNav-1.75-v7.12.10-pages.5-Universal-Flasher.zip) |
| 1.75 圆屏，Mac 烧录 | **7.12.10-pages.5** | [macOS 一键烧录 ZIP](https://github.com/zyn950929/MotoNav/releases/download/motonav-2026.11.01/MotoNav-1.75-v7.12.10-pages.5-macOS-OneClick.zip) |
| iPhone App | **0.16.0 Build12**，不带读秒 | [未签名 IPA](https://github.com/zyn950929/MotoNav/releases/download/motonav-2026.11.01/MotoNav-0.16.0-build12-unsigned.ipa) · [iOS 说明](ios/README.md) |

圆屏包**只适用微雪 ESP32-S3-Touch-AMOLED-1.75**。1.85B 仍是 7.12.3-185B，不能刷这些镜像；M5Stack 也不能混刷。

## 这次更新了什么

- 读秒版以维护者指定的 **single.32-fresh.1 五栏精简版**为准：保持五栏（首页搜索、圆屏显示、导航语音、圆屏设置、地图 Key），精简调试入口与状态信息，统一卡片与选路布局。
- 保留网页摩托车 / 驾车 / 自行车、选路与模拟、途经点 / 加油站、收藏 / 历史、导航中修改设置，以及 single.32 的后台和停车读秒修正。
- 不再在读秒端叠加近期 PR 的传输 / 压缩 / 媒体图片实验；普通版目录保持本次整理前的内容，Git 历史保留。
- 1.75 固件回到准确的 **pages.5** 源码和镜像，通用 MotoNav 开机，**不含个人 Rebel500 画面**。Windows 和 Mac 包使用同一镜像。
- 补齐 iPhone Build12 IPA 下载，iOS 功能代码不在这次改版。

[详细更新与验证范围](docs/RELEASE-2026-11-01.md)

## 安卓：怎么安装或自己打包

**普通版**：让 WorkBuddy 获取仓库，构建 `android/standard`，或双击根目录 `一键打包-普通版.bat`。普通版不需要额外读秒组件。

**读秒版只发布源码，不提供预签名 APK**。下载源码后，让 WorkBuddy 构建 `android/countdown`，运行根目录 `一键打包-读秒版.bat`。入口自动下载并校验兼容组件，**不要把 Gradle 中间 APK 当成完整读秒版**。

可直接复制给 WorkBuddy：

```text
从 https://github.com/zyn950929/MotoNav 的 main 分支获取最新源码。
我要打包读秒版 8.3.26-single.32-fresh.1，工程是 android/countdown。
请准备构建环境，下载慢时使用可信国内镜像；使用 Build-Countdown.ps1 完成完整构建，自动获取并校验 provider.apk。
已有签名就沿用，没有就生成并保存我的本机签名。不要修改功能或上传我的签名、Key。
请告诉我完整 APK 路径、最终包名和签名证书的十六进制 SHA1，并保存 Application-info.txt。
```

要普通版时改说“打包普通版 8.3.26，使用 android/standard 和 Build-Personal.ps1”，不要使用读秒构建入口。

**高德 Key**：用实际 APK 的**包名 + 签名证书 SHA1**申请自己的高德 Android Key，在 App 的 Key 页面填写。不再需要天地图 Key。读秒完整包包名固定 `com.motonav.app.fresh`；签名由打包者决定。编译后实际身份信息由工具保存到本机 `Application-info.txt`，不要照抄他人的 SHA1。

**覆盖升级**必须沿用原包名和签名。自行打包请私下备份签名；新生成的签名与旧版不同就不能直接覆盖，升级时沿用自己的签名。不要把证书 SHA1 当成 APK 文件 SHA256。

首次使用按提示授予定位、附近设备 / 蓝牙、通知权限，完成额外数据组件的首次初始化，再连接圆屏。读秒没有数据时先检查初始化、定位、网络与后台权限。

## 圆屏：怎么烧录

1. 按电脑系统下载上表 ZIP，**完整解压**，不要在压缩包内运行。
2. USB 数据线连接 1.75 圆屏，关闭占用串口的程序。
3. Windows 双击 `一键烧录.bat`；Mac 双击 `一键烧录.command`，支持 Apple 芯片和 Intel。
4. 已有同板型、同分区 MotoNav 选默认“应用升级”，保留配对和设置。**首次安装 / 救砖选完整安装，会清空设置**，请按工具提示确认。

Mac 包自带工具，不用另外安装 Arduino、Xcode 或 Python。系统拦截时按“隐私与安全性”提示允许当前文件，不要全局关闭安全功能。

[固件源码、兼容性与构建](firmware/README.md)

## iPhone：怎么安装

IPA 是**未签名安装包**，不能直接点击安装。下载后用 Sideloadly 等工具，以自己的 Apple 账号签名安装；免费账号通常需要每 7 天重新签名。按系统要求信任开发者或开启开发者模式。覆盖升级沿用签名和 Bundle ID，分享扩展也需签名。

App 内使用自己的高德 iOS Key，申请时填写最终实际 Bundle ID。iOS **不含红绿灯读秒**。

[安装与源码构建](ios/README.md)

## 使用范围与致谢

**红绿灯读秒是实验功能，不保证一直有效，也不保证每个路口、每个方向都有数据。** 地区、数据源、定位、网络和系统后台限制都可能影响结果；缺少的数据不会补造，必须以现场信号为准。

自主功能代码全程借助 AI 开发与迭代，维护者提出需求并参与设备和路测验证。第三方 SDK、库、兼容组件不属于项目原创；代码与第三方资源的权利归属以 [许可](LICENSE.md)、[第三方说明](licenses) 为准。

感谢 [nongxl](https://github.com/nongxl) 的省电优化贡献；single.32 保留其 PR #9 基础及兼容修正。iOS UI 来源为 [coolcoolmanman-design](https://github.com/coolcoolmanman-design/MotoNav)，见 [来源与致谢](ios/UI-SOURCE-NOTICE.md)。近期其他 PR 的来源和历史提交仍保留，但不把它们当作本次读秒版的实际实现。

**使用本套程序请遵守交通法规，使用即表示自愿承担可能带来的交通事故及其他风险。** 行驶中不要操作设备，路线、图标和读秒不能代替现场信号与标志；本提示不排除依法不能免除的责任。项目仍可能存在错误，测试回放不等于实际道路验证。

当前帮助见[文档目录](docs/README.md)，旧更新说明归入[历史资料](docs/archive/README.md)。历史安装与烧录包保留在 [Releases](https://github.com/zyn950929/MotoNav/releases)，版本以本页顶部为准。
