> 历史资料：只对应当时的版本。当前安装和构建请看[项目主页](../../README.md)。

# MotoNav iPhone 安装与续签

适用于 GitHub Release 提供的 `MotoNav-0.13.8-unsigned.ipa`。支持 iOS 15.0 及以上的 iPhone。此文件是 arm64 真机 Release 构建，包含主应用及分享扩展，需用自己的 Apple 账号重新签名后安装。

## 安装步骤

1. 在 [MotoNav Releases](https://github.com/zyn950929/MotoNav/releases) 下载 IPA；需要时使用同版本 `SHA256SUMS.txt` 校验下载文件。
2. 从 [Sideloadly 官网](https://sideloadly.io/) 安装 Windows 或 macOS 版本。Windows 所需 Apple 驱动、iTunes/iCloud 按官网说明安装。
3. 用 USB 连接并解锁 iPhone，在手机上选择信任电脑，确认 Sideloadly 已识别手机。
4. 把 IPA 拖入 Sideloadly，选择自己的 iPhone，填写自己的 Apple 账号，点击 Start，按提示完成账号验证。账号信息只输入 Sideloadly/Apple 验证流程，不需要交给项目作者。
5. iOS 16 及以上如提示需要开发者模式，在“设置 → 隐私与安全性 → 开发者模式”启用，按系统提示重启并确认。
6. 如提示“不受信任的开发者”，在“设置 → 通用 → VPN 与设备管理”找到自己账号对应的开发者并信任。
7. 打开 MotoNav，按需要授权定位、蓝牙和音乐；后台导航需按系统提示允许后台定位。在 App 的 Key 设置页查看、复制**实际 Bundle ID**，申请并填写自己的高德 iOS SDK Key；天气填写自己的和风 API Host 与 Key，天地图按需配置。
8. 圆屏使用 Release 中的通用固件（iOS、Android 共用），刷写说明见 [固件说明](../../firmware/README.md)。在 iPhone 蓝牙设置中配对圆屏，再到 App 连接导航通道。媒体按键及歌曲信息配置见 [BLE 使用说明](../BLE-HID.md)。

高德 Key 必须绑定重签安装后的实际 Bundle ID。Sideloadly 可能调整包名；请以手机 Key 设置页显示值为准。重签更换包名后，旧包名的 Key 可能不可用。

## 续签与更新

免费 Apple 账号通常每次签名有效 7 天，到期需重新签名；也有同时安装 App 数量及 App ID 配额限制。付费开发者账号签名有效期可长至一年，以实际签名配置为准。

续签或更新时使用同一个 Apple 账号及相同 Bundle ID 覆盖安装，尽量保留本地设置；先自行保存 Key 和重要设置，避免先卸载。可在 Sideloadly 启用自动刷新，但电脑需要运行相应服务，并能通过 USB 或已配置的 Wi-Fi 同步连接手机。仅下载 IPA 到手机无法完成这些步骤。

分享扩展随主应用一起打包；若签名工具移除了扩展，系统分享菜单不会显示 MotoNav，此时仍可在 App 内粘贴高德分享链接。

## 常见问题

- 找不到手机：检查 USB 数据线、手机解锁与“信任电脑”，Windows 检查官网要求的 Apple 驱动。
- 提示签名到期或无法验证：连接网络，使用原账号及原 Bundle ID 重新签名覆盖安装。
- 提示 App/App ID 配额不足：按 Apple/Sideloadly 提示处理；分享扩展也可能占用 App ID 配额。
- App 能打开但地图搜索失败：检查手机显示的实际 Bundle ID、高德 Key 绑定及服务权限。
- 分享菜单无 MotoNav：检查是否保留分享扩展，并在系统分享菜单“更多”中启用。

安装规则参考 [Sideloadly 官方 FAQ](https://sideloadly.io/faq)。发布包已进行编译与结构检查；自签安装、分享扩展、后台定位和蓝牙行为仍需在具体账号及设备上验证。项目采用仓库内的非商业使用条款。

## 从源码打包（Mac）

安装 Xcode 与 CocoaPods，在 `ios` 目录运行 `pod install`，再从仓库根目录运行 `./scripts/build-ios-ipa.sh`。产物在 `output/ios/`，无需作者开发者 Team 或证书。自行用 Xcode 签名时，为两个 target 选择自己的 Team，并让扩展 Bundle ID 使用主应用 ID 加 `.share`。
