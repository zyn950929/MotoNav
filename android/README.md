# 安卓版本选择

| 版本 | 源码 | 入口 |
| --- | --- | --- |
| 普通版 8.3.26，不带读秒 | [standard](standard/README.md) | 根目录 `一键打包-普通版.bat` |
| 读秒版 8.3.26-single.32-fresh.1，五栏精简 UI | [countdown](countdown/README.md) | 根目录 `一键打包-读秒版.bat` |

普通版保持本次整理前源码。两版分开编译和维护签名，不要互相覆盖目录。读秒完整入口自动获取并校验额外兼容组件；Gradle APK 只是中间产物。

[下载、申请 Key 和首次使用](../README.md) · [本次更新](../docs/RELEASE-2026-11-01.md)
