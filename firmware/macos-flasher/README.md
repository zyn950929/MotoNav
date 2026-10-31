# macOS 一键烧录入口源码

这里保存 pages.5 独立包使用的三个 `.command` 入口与官方工具来源清单。入口依赖同目录的固件镜像和双架构 esptool，不要只下载一个脚本运行。

普通用户下载 [完整 macOS ZIP](https://github.com/zyn950929/MotoNav/releases/download/motonav-2026.11.01/MotoNav-1.75-v7.12.10-pages.5-macOS-OneClick.zip)。支持 Apple 芯片与 Intel；完整包自带工具，无需 Python / Arduino / Xcode。

默认升级写应用到 0x10000，保留兼容分区的设置；完整安装要求确认并会清空设置。`检查环境.command` 不写入设备。

工具：Espressif 官方 esptool 5.6.0。来源和 SHA256 见 `工具来源与校验.json`，工具许可随发布 ZIP 提供。原生工具版本 / 环境在真实 Mac 上检查；写入流程只做过模拟，尚未在 Mac 连接实物烧录。
