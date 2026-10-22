# 控制器 App / 网页

## 两种用法

| 方式 | 用法 |
|---|---|
| **网页** | 直接用浏览器打开 `app/index.html`，填设备地址（如 `192.168.0.53`）→ 连接 |
| **APK** | 安装 `docs/DigiCore.apk`，内置同一张页面，打开即用（地址会记住） |

设备必须和手机/电脑在同一个 WiFi 下（家里 `yueyue`）。设备地址可以写 IP，也可以写 `digicore.local`。

## 页面能做什么

- 实时状态：阶段 / 种族、体力、心情、饥饿、战绩、年龄、设备时间、昼夜
- 操作：喂食、清洁、训练、战斗、抚摸、睡觉、醒来、进化
- 升级卡：勇气、**战斗卡**、力量、守护、回复、复活
- 最近事件（含「退化」）
- 设置：亮度滑条、快进 1h / 6h / 24h（会真的推进宠物年龄）

## 接口

页面只用设备这几个接口，都可单独 curl：

```
GET /api/state              设备状态（含 hour/min/scene）
GET /api/act?c=feed         动作：feed clean train battle pet sleep wake evolve
GET /api/card?id=battle     卡片：courage battle power guard heal revive
GET /api/bright?b=120       亮度
GET /api/warp?h=6           快进
```

设备已开启 CORS（`Access-Control-Allow-Origin: *`），所以从 `file://` 打开的页面也能调。

## 重新构建 APK

不需要 Gradle，直接用 Android SDK 的命令行工具链：

```powershell
powershell -ExecutionPolicy Bypass -File tools\build_apk.ps1
```

流程：`javac`（UTF-8）→ `d8` → `aapt2 compile/link` → 塞入 `classes.dex` → `zipalign` → `apksigner`（debug keystore）。

输出 `docs/DigiCore.apk`（约 16 KB，minSdk 23 / targetSdk 34，debug 签名，可直接安装）。

装到手机：

```powershell
adb install -r docs\DigiCore.apk
```
