# iPhone / 网易云、QQ 音乐媒体控制与歌曲信息

固件 v7.9.0 为微雪 1.75 英寸圆屏增加 HID over GATT Consumer Control，与原 MotoNav 导航服务运行在同一个 BLE Server 和连接上。原导航 UUID、RX/TX 特征、地图分包、MTU 517、配对码验证保持原有设计。音乐按键不会经过手机 App 转发，不要求 QQ 音乐向 MotoNav 开放接口。

## 安装和使用

Mac 请从[当前下载页](https://github.com/zyn950929/MotoNav/releases/tag/motonav-2026.11.01)获取独立 macOS 一键烧录 ZIP，完整解压后双击包内 `一键烧录.command`。默认升级保留设置；`检查环境.command` 只检查，不烧录。

1. 为 Waveshare ESP32-S3-Touch-AMOLED-1.75 刷入 v7.10.0 或更新固件。已有对应分区的 MotoNav 使用 `app_0x10000` 镜像；首次安装才使用 `full_0x0`。完整镜像可能覆盖原设置，应先备份。
2. 安装仓库 iOS v0.11.0 或更新版。首页“圆屏设置”打开“蓝牙媒体按键（QQ 音乐等）”，默认开启。
3. 在 iPhone“设置 → 蓝牙”中找到同一台 `MotoNav-XXXX`，完成系统 HID 配对；按圆屏显示的六位配对码提示操作。圆屏首次配对窗口仍是开机两分钟内。
4. 如果升级前已经配对，iPhone 可能保留旧 GATT 服务缓存。如果不能识别 HID，先在 iPhone 忽略这台 MotoNav，再重启圆屏并重新配对。重新配对会移除该手机上的旧配对记录。
5. 回到 MotoNav，确认“媒体按键已就绪”。先在 QQ 音乐手动播放一首歌，再滑到圆屏音乐页；左/中/右按钮分别为上一首、播放/暂停、下一首。导航和地图传输继续使用原服务。

按键作用于 iPhone 当前响应系统媒体按键的播放器，并非固定指向 QQ 音乐。切换其他音频 App 后可能控制该 App。暂停后继续播放、锁屏、蓝牙耳机并存和后台导航，需要在实际 iPhone/QQ 音乐版本上验证。

v7.10.0 在 HID 模式增加 Apple Media Service（AMS）客户端，读取 iPhone 当前播放器提供的歌名、歌手、专辑、总时长、播放状态与进度，以及播放器名称。中文由 iOS v0.11.0 App 渲染并发送到圆屏的图片栏 5/6；因此需要配套 App 保持蓝牙连接。信息以播放器实际提供内容为准；未提供 AMS 时显示“AMS UNAVAILABLE”，播放/暂停按钮保留组合图标，媒体按键仍可使用。`HID SENT` 表示 BLE 栈接受了按键报告，不表示 QQ 音乐已执行；未配对、未订阅或被主机挂起时显示 HID 未就绪。

关闭 iOS 的“蓝牙媒体按键”后可切回原 Apple Music App 控制。更新后的安卓版收到 HID 能力通知后明确选择 App 控制，保留原 Android 播放器会话、歌名和回执处理。旧安卓版连接已保存 HID 模式的圆屏时不会自动切回；请使用本仓库更新后的安卓版，或先用 iOS 关闭该设置。

## 协议与实现

- 主广播同时包含原 128 位导航服务 UUID 和 16 位 HID UUID `0x1812`，总计连同 Flags、Generic HID Appearance 为 29 字节；名称保留 `MotoNav-XXXX`，由库放入扫描响应。
- 标准 HID、Device Information、Battery 服务由 ESP32 Core 自带 BLEHIDDevice 创建，不替换原 BLE 栈或 ServerCallbacks。
- Report ID 为 1，Consumer Usage Page `0x0C`；两字节小端 Array 输入报告。Report ID 通过 Report Reference Descriptor 表达，不重复放进 GATT 通知载荷。
- 上一首 `0x00B6`，播放/暂停 `0x00CD`，下一首 `0x00B5`。不添加普通键盘 Collection。
- 按下后约 60 毫秒释放为 `0x0000`，键间至少 40 毫秒间隔；四项队列和 120 毫秒触摸防抖，不使用阻塞 delay。失败时不重复发送不确定的按下事件，只重试释放，避免播放/暂停被切换两次。
- 只有当前连接完成原加密及配对认证、订阅 HID 报告、未 Suspend 且处于 Report Mode 时才发送媒体键。断连/退订丢弃旧队列，恢复就绪先发零报告；切换回 App 模式仍释放在途按键。
- 手机配置 `#HM,1` 选择 HID，`#HM,0` 选择原 App 控制，模式保存到圆屏；不会同时发送 `T,M,P/T/N` 和 HID 媒体键。
- 圆屏在认证通知时重复发送 `CAP,HID,1`；`HID,模式,可发按键` 回报实际订阅状态。iOS 等待能力通知后才下发新命令，并在收到认证通知后重同步设置。旧固件不会收到不认识的模式命令。

HID over GATT 的设备模型见 [Bluetooth SIG 规范](https://www.bluetooth.com/specifications/specs/hid-over-gatt-profile/)。固件采用 [Espressif BLEHIDDevice](https://github.com/espressif/arduino-esp32/blob/3.3.10/libraries/BLE/src/BLEHIDDevice.cpp)，没有使用 iOS 私有媒体 API。

## 构建和验证

ESP32 Core 3.3.10，FQBN `esp32:esp32:esp32s3:FlashSize=16M,PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,CDCOnBoot=cdc`。Windows 可使用原 `firmware/build.ps1 -Board 175`；Mac 已有 Arduino CLI/Core 时可在根目录执行：

```sh
python3 firmware/build.py
```

按键报告及调度测试：

```sh
clang++ -std=c++11 -Wall -Wextra -Werror firmware/tests/HidMediaKeysTest.cpp -o /tmp/motonav-hid-tests
/tmp/motonav-hid-tests
```

2026-10-17：v7.10.0 固件完整编译通过；46 项 HID 报告和调度检查、33 项 AMS 数据检查及 33 项 Swift 分包重组检查通过。iOS 主 App/分享扩展无签名 arm64 编译通过。生成应用镜像和完整镜像时校验 merged 文件中 `0x10000` 处与应用镜像逐字节一致，并输出 SHA-256 清单。

用户已确认网易云的 HID 播放/暂停与切歌可用。新增 AMS 的歌曲显示仍需手机与圆屏联调确认。实测必须包括：首次配对、已有设备升级后重新配对、QQ 播放/暂停及切歌、锁屏、连续点击、导航地图传输时操作、断连重连、主机挂起恢复、切回 Apple Music App 控制、安卓版原音乐控制。缺少 HID 订阅时应显示未就绪，不能把构建成功视为 QQ 音乐已响应。

## AMS 歌曲信息（v7.10.0）

使用 [Apple AMS 官方协议](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleMediaService_Reference/Specification/Specification.html)，在现有已认证、加密的连接上执行异步 GATT 客户端操作，不新建蓝牙连接。订阅 Entity Update 后分别注册 Player 的名称/PlaybackInfo，以及 Track 的歌手/专辑/标题/时长。截断字段串行执行 Entity Attribute 选择与长读取，最多保留 512 字节且不切断 UTF-8 字符。过时读取通过字段修订号丢弃。空属性不会影响 HID；状态错误或失效句柄触发重新发现。

订阅 iPhone GATT Service Changed；收到服务变化后清理旧数据并重新发现。AMS 暂不存在时每 20 秒重试，断连、认证丢失或切回 App 控制时清空歌曲和进度。所有主机回调通过有界 FreeRTOS 队列交给主循环，读取和订阅操作不阻塞导航，也不发送 AMS Remote Command。

固件向 iOS 的导航 TX 通知使用二进制帧：`A6 | generation LE32 | revision LE16 | field | part | count | flags | UTF8 bytes`。字段 0–6 为标题、歌手、专辑、时长、PlaybackInfo、播放器名称、服务状态。标志 bit 0 表示仍为部分内容。分片大小跟随 MTU，每片最多 128 字节，每 20 毫秒至多发送一片。iOS 只在全部分片收到后解码 UTF-8，并拒绝旧连接、旧修订和无效字段；`#AMS?` 请求当前快照。进度按 AMS 提供的时间和速率推算，不轮询播放器。

AMS 不提供封面或歌词。新版本未改变导航、地图或天气协议。请实测网易云切歌后的中文更新、暂停状态、长标题、锁屏、断连重连和导航同时运行；HID SENT 仍仅代表按键报告已发送。

2026-10-17：已将 v7.10.0 应用镜像烧录到连接的 ESP32-S3 圆屏（`/dev/cu.usbmodem1101`）。读回分区确认 app0 为 `0x10000 / 3 MiB`，16 MB 闪存匹配；esptool 完成写入和 Hash 校验并自动重启。未修改引导程序、分区表或 NVS，新增 AMS 显示尚未完成实机联调。

v7.10.1 额外修复 iOS 圆盘旋转命令的精确角度解析，保留 v7.10.0 的 AMS、HID、导航和天气功能。旋转命令的 0–359° 全角度及兼容/无效输入共 394 项检查通过；旧版手机 App 的现有命令即可生效。


## 逐句歌词（固件 v7.11.0 / iOS v0.12.0）

“显示逐句歌词”位于 iOS 媒体控制设置，首次安装和已有版本升级默认关闭。关闭时圆屏上方为歌名、下方为歌手，歌手栏不再拼接专辑。开启后歌手下方增加当前一句歌词，不改动媒体按键。该开关及歌手排版需要配套 iOS v0.12.0；仅升级固件不会给已安装的旧 App 增加开关，也不能改变旧 App 已渲染的文字图片。

AMS 不包含歌词，手机按歌名、歌手、专辑、时长从 [LRCLIB 公开接口](https://lrclib.net/docs) 获取 LRC。默认关闭时不请求歌词服务。匹配时校验歌曲和歌手、专辑及约 2 秒的时长差，避免混用现场版等版本；没有同步歌词、纯音乐、网络错误时保持歌名/歌手布局，设置页说明原因。不是所有歌曲都能匹配。

解析 LRC 多时间标签、毫秒、offset、空白间奏和同时间的译文，根据 AMS PlaybackInfo 的时间与速率，每 250 毫秒检查当前行，只在行改变时发送图片。暂停固定位置，拖进度或切歌使用更新后的时钟；切歌防抖 0.8 秒，取消旧请求并以请求令牌丢弃迟到结果。缓存最多 20 首，网络失败有短时缓存，不持续重试。断线、AMS 服务变化、切回 App 控制或关闭开关时清理歌词。

新增图片栏 12（共 13 栏），`CAP,LYR,1` 回报能力，`#ML,0/1` 控制显示，`#LY,generation,titleRevision,artistRevision` 为歌词图片绑定当前歌曲。圆屏拒绝与 AMS 当前歌曲不匹配的歌词图片，并在歌曲或连接代次改变时清空，避免残留旧歌词。导航、天气图片栏 0–11 保持兼容。

验证：旋转协议 394 项、HID 46 项、AMS 数据 33 项、Swift AMS 分包 33 项、LRC/播放时钟/歌曲匹配 32 项检查通过。LRCLIB 官方文档示例接口请求成功并返回同步歌词；未对用户当前网易云歌曲实测歌词匹配与同步。用户已确认 v7.10.0 的歌曲信息可显示。按用户最新要求，本次只烧录圆屏，未安装手机端更新。

2026-10-17：v7.11.0 已烧录到连接圆屏的 `0x10000` 应用区，esptool Hash 校验通过并自动重启；iOS v0.12.0 通用设备构建通过，按用户要求未执行安装。


## 媒体恢复（固件 v7.12.0 / iOS v0.12.1）

用户反馈关闭歌词后第二行仍有旧文字，同时媒体键显示 HID NOT READY。第二行是图片栏 6（歌手栏），独立歌词使用图片栏 12。固件处理 `#ML,0` 时清理歌词显示与接收缓存、使旧歌词绑定失效，并在实际关闭或每次连接的首次 OFF 时清空图片栏 6、发送 `T,M,Q` 请求手机重新渲染歌名/歌手。OFF 回复 `ML,0`；ON 回复 `ML,1`。每次连接变化先默认关闭歌词，再接受手机当前设置，避免重连时沿用旧启用状态。

iOS v0.12.1 将歌词开关与 `#HM` 媒体模式同步拆开，关闭只发送 `#ML,0`，取消歌词请求、计时器、待发送图片和 `#LY`，替换重连缓存。即使尚未收到 HID 能力或正在断线也缓存最新 OFF；收到 ML 回执才确认，未确认时每 2 秒重发。迟到的 ON 回执不能修改用户的 OFF 设置。本次遵循用户此前只更新固件的选择，不安装手机端。

HID 的原加密、订阅、Suspend、Report Mode 与释放报告条件继续生效。初始化零报告重试限制为每 50 ms 一次，不假装 HID 已就绪。若点按时 HID 不可用、但当前已认证的 iPhone AMS Remote Command 可写，则使用 [Apple 官方媒体命令](https://developer.apple.com/library/archive/documentation/CoreBluetooth/Reference/AppleMediaService_Reference/Appendix/Appendix.html)作为备用：上一首 4、播放/暂停 2、下一首 3。订阅支持的命令列表；收到列表后仅启用相应按键，未收到列表时可尝试这三个标准命令。四项队列，和元数据读取串行执行；断连/服务变化清空，发送前再检查支持列表。不确定的写入失败不自动重试，不会将同一次点按同时发给 HID 和 AMS。

圆屏反馈 `AMS QUEUED`、`AMS SENT` 或 `AMS SEND FAILED`，`AMC,按键掩码` 回报 AMS 控制通道。AMS SENT 仅表示 GATT 写响应成功，不等于播放器已执行。串口 MEDIA 日志记录 HID 就绪条件、AMS 控制掩码与歌词开关，不输出歌词内容。保留原导航服务及地图/天气/旋转功能。

验证：31 项 AMS 命令队列检查、12 项歌词设置回执/重试/断线/迟到 ON 检查，及原 HID 46 项、AMS 数据 33 项、Swift AMS 分包 33 项通过。固件及 iOS 通用设备构建通过。歌词残留的实际来源与 HID 未就绪的具体条件尚未获得设备日志确认，需烧录后复测，不能把构建成功视为网易云已响应。

2026-10-17：v7.12.0 已烧录到连接圆屏，Hash 校验通过，串口确认启动版本 v7.12.0。启动时记录 HID 未认证/未订阅、AMS 控制为 0；尚需观察 iPhone 重连后的状态。
