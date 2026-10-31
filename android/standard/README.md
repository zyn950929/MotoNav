# MotoNav 普通版 8.3.26

这套是原有不带红绿灯读秒的安卓版本，独立于 `../countdown`。保留高德地图、网页摩托车规划、自行车与驾车导航、巡航、常用地点、轨迹与原有设置。此次只调整仓库位置，并补入新固件主页所需的手机时间同步；没有引入读秒依赖。

## 编译与签名

Windows 在仓库根目录运行 `一键打包-普通版.bat`。首次会准备环境、生成本人的包名和签名，输出目录为根目录 `output/`，安装包 `MotoNav-personal.apk`，包名及十六进制 SHA1 写在 `Application-info.txt`。

已有环境时可指定路径，依赖已经缓存才加 `-Offline`：

```powershell
.\Build-Personal.ps1 -JavaPath 'D:\Tools\jdk-17' -SdkPath 'D:\Android\Sdk' -GradlePath 'D:\Tools\gradle-8.10.2\bin\gradle.bat'
```

Android Studio 请打开 **`android/standard`**，Gradle JVM 选择 JDK 17 或兼容的 JDK 21，不要使用 JDK 25。直接构建 debug 会在此工程 `.local/debug.keystore` 生成新的开发签名。

用**实际输出包名与证书 SHA1**申请自己的高德 Android Key，在 App 的地图 Key 页面填入。地图来源为高德，不需要天地图 Key。`personal.properties`、`keystore/` 与 `.local/` 都只留在自己电脑；以后覆盖升级必须沿用同一包名和签名。

## 适配固件与检查

当前 1.75 通用固件为 `7.12.10-pages.5`，两种安卓版本共用。时间同步在蓝牙认证后发送，后续每分钟校准，不启动额外导航引擎。

JDK 17 配置好后执行 `tests/run.ps1`；本轮公开源码已独立构建并校验签名，未把这次新签名构建覆盖到维护者手机。此前稳定版实测与限制仍按相应历史更新说明理解。
