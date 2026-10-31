> 历史资料：只对应当时的版本。当前安装和构建请看[项目主页](../../README.md)。

# 1.75 通用固件 7.12.9-junction.11

## 下载与升级

[打开 v7.12.9-junction.11 Release](https://github.com/zyn950929/MotoNav/releases/tag/firmware-v7.12.9-junction.11)。

- Windows 推荐下载 `MotoNav-1.75-v7.12.9-junction.11-Universal-Flasher.zip`，完整解压后双击“一键烧录.bat”。已有同板型、同分区 MotoNav 选择默认“升级”，保留设置与配对；首次完整安装会整片擦除，请先备份。
- macOS／Linux 下载 `MotoNav-universal-firmware-7.12.9-junction.11.zip`，包含镜像、校验清单和烧录入口；先按包内说明安装 esptool，再运行 `python3 firmware/flash_macos.py --check` 校验。
- 也可单独下载 `app_0x10000.bin` 或 `full_0x0.bin`，文件名中的地址是写入偏移；不要混用。
- `MotoNav-Firmware-Source-1.75-v7.12.9-junction.11.zip` 是 1.75 固件源码及依赖库，不含 Android／iOS App。
- 用 `SHA256SUMS.txt` 核对下载附件。Windows 包已包含烧录工具，不需要 Arduino、Python 或编译环境。



适用 Waveshare ESP32-S3-Touch-AMOLED-1.75（466×466），禁止刷入 1.85B 或 M5Stack StopWatch。Android 原版、读秒实验版和 iOS 沿用现有导航协议；只有读秒实验版有上游数据时发送实验灯包。iOS 不提供读秒。

本版整理主程序、图标、绘图和通信；保留精确计算缓存和绘图优化，车道提醒与转弯倒计时同步；右侧竖排多灯、过期清理、灰色未知右转，以及网页导航转弯环按距离更新间隔连续填充。保留电池五分钟无导航关机和插电不自动关机。

**公开版使用通用 MotoNav／品牌开机动画。** 默认品牌为 MotoNav；App 仍可选已有品牌，保存的品牌编号继续有效。

Windows：安装 Arduino CLI、Espressif ESP32 Core 3.3.10 后，在仓库根目录执行：

```powershell
.\firmware\build.ps1 -Board 175
```

Mac／Linux：`python3 firmware/build.py`。生成在 `firmware/dist/175/panel0`，升级程序分区为 `app_0x10000.bin`；首次完整镜像为 `full_0x0.bin`，需先核对板型与分区，完整刷写可能影响设置。此版本已发布独立 Release，旧版本下载保留，不能当作本版本镜像。

先前个人启动图版本已在实物虚拟导航确认“圆环连续了，其他正常”；通用启动图公共副本另行编译，尚未重刷硬件。其构建结果见 [验证说明](VERIFY-2026-10-25.md)。真实道路与长时间后台运行仍待验证，不承诺固定帧率、读秒覆盖或无任何闪烁。

1.85B 本轮仍为 7.12.3-185B，未声称同步新灯卡片与渲染改动。


## Release 附加检查

成品镜像与此前已校验的公开通用版本 SHA256 一致；再次确认 ESP32-S3 标识、分区表、0x10000 应用偏移和运行版本字符串。Windows 包校验、损坏镜像拒绝、越界路径拒绝及还原检查通过；独立镜像包的版本／哈希校验和六项 Mac 烧录策略检查通过。没有将私人启动图版本作为成品上传，也未为此重新刷写当前个人设备。

安卓读秒属于实验功能，不保证一直有效，不保证每个路口或每个方向均有数据；iOS 不提供该功能。真实道路验证仍待完成，始终遵守交通法规并以现场信号为准。
