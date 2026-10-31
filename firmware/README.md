# 圆屏固件：1.75 通用版 7.12.10-pages.5

仅适用 **微雪 ESP32-S3-Touch-AMOLED-1.75**。安卓普通版、读秒版和 iOS 共用基础协议。公开开机为通用 MotoNav / 品牌动画，不含个人 Rebel500 图片。

## 下载与烧录

- [Windows 一键烧录 ZIP](https://github.com/zyn950929/MotoNav/releases/download/motonav-2026.11.01/MotoNav-1.75-v7.12.10-pages.5-Universal-Flasher.zip)：完整解压，双击 `一键烧录.bat`。
- [macOS 一键烧录 ZIP](https://github.com/zyn950929/MotoNav/releases/download/motonav-2026.11.01/MotoNav-1.75-v7.12.10-pages.5-macOS-OneClick.zip)：完整解压，双击 `一键烧录.command`，支持 Apple 芯片和 Intel；工具已包含，不需要安装 Python / Arduino / Xcode。
- [1.75 固件源码 ZIP](https://github.com/zyn950929/MotoNav/releases/download/motonav-2026.11.01/MotoNav-Firmware-Source-1.75-v7.12.10-pages.5.zip)

两个烧录包使用完全相同的 pages.5 镜像。已有同板型、同分区 MotoNav 选默认“应用升级”，保留配对和设置；首次安装或救砖才选完整安装，**会清空设置**。

**禁止把本包刷到 1.85B、1.75C 或 M5Stack。** 1.85B 源码仍是 `boards/185b` 的 7.12.3-185B，本次没有提供其新镜像。

## pages.5 包含什么

固定页面顺序、主页大号 24 小时时间与小号日期、旋转记忆、音乐与天气、空闲关机与插电保护，以及蓝牙启动任务栈修正。

配套 iOS 0.16.0 Build11/12，换基础图时按精确坐标接续重叠区域已知楼块；道路、水面和新楼块优先。每帧绘图代码不变，不增加蓝牙地图字节或建筑大缓冲。未准备的新区域仍需新数据。新楼块保留由 iOS `#BC` 显式启停，安卓沿用原协议。

源码和镜像以 pages.5 构建快照为准，不含随后 PR 的 Deflate / XOR 增量传输，也不包含其他窗口的 pages.6。

## 自行编译

安装 Arduino CLI、Espressif ESP32 Arduino Core **3.3.10**，依赖库在 `libraries/`：

```sh
python3 firmware/build.py
```

Windows：`firmware/build.ps1 -Board 175`。产物在 `firmware/dist/175/panel0`。打包 Windows 工具：`python firmware/package_flasher.py --tools-dir <esptool目录>`。

`windows-flasher/`、`macos-flasher/` 保存对应入口源码；普通用户直接下载上面的完整 ZIP。

[详细更新与构建参数](../docs/RELEASE-FIRMWARE-7.12.10-pages.5.md)

## 验证范围

pages.5 完成完整固件编译、生产解码器桌面回归、镜像格式 / 分区 / 写入偏移和包内哈希检查。Mac 的 Apple 芯片及 Intel 环境已验证原生工具与模拟写入流程，未连接真实圆屏进行 Mac 刷写。新版 App + pages.5 的完整实路、耗电与性能仍需用户验证。
