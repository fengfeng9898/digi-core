# DIGI-CORE

**一台跑在 ESP32 上的数码暴龙风格养成机。64×64 全彩 LED 面板，无实体按键，全程用手机网页 / App 控制。**

蛋 → 幼年期 → 成长期 → 成熟期 → 完全体 → 究极体，5 条进化线、27 只角色、真实北京时间昼夜背景、回合制战斗、6 张升级卡、A/B 双槽存档、网页 OTA 升级。

![界面示意](docs/screen_mock.png)

---

## 特性

| 模块 | 说明 |
|---|---|
| 显示 | HUB75E 64×64 真彩，6bit 色深，112 Hz 刷新，15 FPS 渲染 |
| 渲染 | 单缓冲 + 脏矩形 + HUD 值缓存，单帧绘制 **0.3–0.8 ms**（扫描周期 8.9 ms） |
| 养成 | 6 阶段、5 条进化线、时间闸门 + 胜场门槛 |
| 惩罚 | 饥饿掉血、心情衰减、便便、生病、**退化**（同线回退）、死亡 |
| 战斗 | 回合制，命中率 = `P*100/(P+E)`，24 HP 交换制，前冲突进 + 受击抖动 + 打击特效 |
| 卡片 | 勇气 / 战斗 / 力量 / 守护 / 回复 / 复活 共 6 张 |
| 场景 | 按真实北京时间切换昼夜，云朵横向飘动，左下角时钟 |
| 控制 | 局域网网页 + Android App（含屏幕预览），无任何按键 |
| 存档 | A/B 双槽 + CRC，掉电最多丢最后一次存档 |
| 升级 | HTTP OTA（`/api/ota`），WiFi 直接刷机，不用插线 |

---

## 硬件

| 项 | 型号 |
|---|---|
| 主控 | ESP32-DevKitC-32E（ESP32-WROOM-32E，4 MB Flash，无 PSRAM） |
| 面板 | HUB75E 64×64 P3，1/32 扫描（**必须有 E 线**） |
| 转接 | 自制 ESP32 → HUB75 转接板 |
| 供电 | 5 V（面板单独供电，共地） |

> 无 PSRAM 是硬性要求：HUB75 DMA 缓冲必须位于内部 SRAM，PSRAM 不可用。

### 接线（当前固件实际使用）

采用 `ESP32-HUB75-MatrixPanel-DMA` 的默认引脚，仅覆盖 E 与时钟相位：

| 信号 | GPIO | 信号 | GPIO |
|---|---|---|---|
| R1 | 25 | R2 | 14 |
| G1 | 26 | G2 | 12 |
| B1 | 27 | B2 | 13 |
| A | 23 | LAT | 4 |
| B | 19 | OE | 15 |
| C | 5 | CLK | 16 |
| D | 17 | **E** | **18** |

- `cfg.gpio.e = 18`
- `cfg.clkphase = false`
- `cfg.latch_blanking = 1`（必须 ≥1，0 会让库内部下溢）
- `cfg.double_buff = false`（双缓冲翻页会忙等 EOF 并重连 DMA，反而闪烁）

> `src/config.h` 里还有一组 `PIN_*` 宏，是早期接线记录，**固件并不使用**，接线请以本表为准。

---

## 快速开始

### 环境

```
PlatformIO 6.x
platform  = espressif32@6.8.1
framework = arduino (ESP32 core 2.0.17)
board     = esp32dev
```

依赖库在 `platformio.ini` 中自动拉取：

- `adafruit/Adafruit GFX Library`
- `ESP32-HUB75-MatrixPanel-DMA` 3.0.5

### 编译与烧录

```bash
pio run                                   # 编译
pio run -t upload --upload-port COM3      # 烧录（换成你的串口）
pio device monitor -b 115200              # 看日志
```

正常启动日志：

```
=== DIGI-CORE 1.0.2 ===
[panel] 64x64 depth=6 refresh=112Hz freeheap=243840
[save] load A n=80/80 ok=1 seq=71 | B n=80 ok=1 seq=70
[net] mode=LAN ip=192.168.0.53
[boot] done freeheap=185404
```

### 配网

默认 SSID / 密码在 `src/config.h`：

```c
#define WIFI_STA_SSID_DEFAULT "你的WiFi"
#define WIFI_STA_PASS_DEFAULT "密码"
```

连不上会自建热点 `DIGI-CORE-XXXX`（密码 `digipet1`），手机连上后打开 `192.168.4.1` 配网。

开机横幅会显示当前 IP。

---

## 手机控制

### 网页

手机浏览器直接打开 `http://<设备IP>/`，包含：

- 状态（阶段 / 体力 / 心情 / 饥饿 / 年龄 / 时钟）
- 操作：喂食 · 清洁 · 训练 · 战斗 · 安抚 · 睡觉 · 唤醒 · 进化检查
- 卡片：6 张升级卡
- **选择进化线**：5 条线 + 随机（孵化时生效）
- **重置游戏**
- 调试：+6 小时 / +1 天 / +3 天 快进

每个操作在面板上都有 0.7 秒对应小动画（爱心 / 泡泡 / 火花 / Zzz）。

### Android App

`docs/DigiCore.apk`（约 108 KB，无第三方依赖，WebView 外壳），填入设备 IP 即可。额外包含 **64×64 屏幕预览**，不用一直盯着面板。

App 源码在 `app/index.html`，用 `tools/build_apk.ps1` 打包（aapt2 + d8 + apksigner，不需要 Gradle）。

---

## OTA 升级

双 OTA 分区（`partitions_ota.csv`，各 1.875 MB），通过 WiFi 刷写：

```bash
pio run                                   # 先编译
curl "http://<设备IP>/api/ota/arm?k=1234"  # 授权 60 秒
curl -F "firmware=@.pio/build/esp32dev/firmware.bin" http://<设备IP>/api/ota
```

也可以在 App / 网页的「固件更新（OTA）」里选文件上传。

> 口令在 `src/config.h` 的 `OTA_KEY`。因为开了 CORS，**仅限可信局域网使用**。

---

## 玩法

### 五条进化线

| 线 | 进化链（括号内为进化所需胜场） |
|---|---|
| 亚古兽 | 滚球兽 → 亚古兽(5) → 暴龙兽(10) → 机械暴龙兽(20) → 战斗暴龙兽 |
| V 仔兽 | 芝比兽 → V仔兽(5) → XV兽(10) → 机甲龙兽(20) → 帝皇龙甲兽 |
| 大耳兽 | 软糖兽 → 大耳兽(5) → 加尔哥兽(10) → 拉比兽(20) → 撒多格杜兽 |
| 巴达兽 | 巴达兽(5) → 天使兽(10) → 神圣天使兽(20) → 炽天使兽 |
| 迪路兽 | 迪路兽(5) → 尼菲迪兽(10) → 人面战鹰兽(20) → 圣龙兽 |

孵化时随机选线，也可以在网页里指定。

### 进化与退化

```
进化条件 = 时间到（本阶段时长）  或  胜场达标
究极体额外需要：勇气卡
退化     = 随机回退到同一条线内更低的阶段，胜场清零，攻防 ×0.7
```

各阶段时长（随机区间）：蛋 12–24 h，幼年期 2–4 天，成长期 3–6 天，成熟期 3.5–7 天，完全体 6–12 天，**到究极体约 15–30 天**。

### 战斗

```
P = atk*2 + def                                   我方力量
E = P + (遭遇战 ? 16 : 0) + rand(-14 .. 15)       敌方力量
命中率 = P*100 / (P+E)                            敌方 = 100 - 命中率
伤害   = 3 .. 7（心情 ≥80 时 +1）
HP     = 24，最多 40 次交换，打满按剩余血量判定
战后宠物 HP 损失 = (24 - 剩余) * 45 / 24
```

演出顺序：敌方登场 → `GO!` → 突进攻击 → `HIT!`（白核 + 十字环 + 火花 + 抖动）→ 结果。敌人都不是角色线成员（恶魔兽 / 艾特兽 / 吸血鬼兽，BOSS 为堕天地狱兽与小丑王）。

---

## 素材管线

仓库里的精灵数据是由原始素材**转换生成**的，脚本都在 `tools/`：

```bash
# 1. 获取素材（第三方仓库，需自行 clone）
git clone https://github.com/kongtaehun/digimon-assets-VB.git ../vb-assets

# 2. 生成固件用的精灵头 + 网页用的数据
python tools/import_vb.py \
  --src ../vb-assets/assets/data \
  --ids koro,agu,grey,metalgrey_va,wargrey,chibi,v,xv,paildra,imperialdra_f,pata,ange,holyange,seraphi,gummy,terrier,galgo,rapid,saintgalgo,tail_c,neferti,silphy,holydra,devi,ete,vamde,belphe_r,bota

# 3. 重新生成角色枚举与动画表
python tools/gen_char_enum.py
```

其它工具：

| 脚本 | 作用 |
|---|---|
| `tools/import_backgrounds.py` | 生成昼夜背景 `bg_data.h` |
| `tools/make_joker.py` | 原创 BOSS「小丑王」动画 + `joker_data.h` |
| `tools/make_app_icon.py` | APK 图标 |
| `tools/build_apk.ps1` | 打包 APK |
| `tools/serial_log.py` | 串口抓日志 |

精灵格式：16 色索引，每像素 1 个十六进制字符，`0` 表示透明，`64×48` 画布，ASCII 行数组。

---

## 目录结构

```
src/
├── main.cpp              主循环、面板初始化、15fps 渲染闸门
├── config.h              WiFi / 面板参数 / OTA 口令
├── panel.h, panel_rot.h  面板实例 + 90° 旋转坐标映射
├── game/                 宠物状态机、结算、进化、战斗结算、存档
├── gfx/                  精灵绘制、动画播放、角色枚举
├── ui/                   主界面、战斗演出、屏幕绘制与脏矩形
├── net/                  HTTP 服务、API、内置网页
└── assets/               生成的角色/背景数据
app/index.html            手机控制页（APK 内嵌）
web/                      浏览器预览（走 JS 复刻同一套逻辑）
tools/                    素材导入、APK 打包、串口工具
docs/                     APK、示意图
partitions_ota.csv        双 OTA 分区表
```

---

## HTTP API

| 方法 | 路径 | 说明 |
|---|---|---|
| GET | `/` | 控制页面 |
| GET | `/api/state` | 完整状态 JSON |
| GET | `/api/act?c=feed\|clean\|train\|battle\|pet\|sleep\|wake\|evolve` | 执行操作 |
| GET | `/api/card?id=courage\|battle\|power\|guard\|heal\|revive` | 用卡 |
| GET | `/api/bright?v=10..180` | 调亮度 |
| GET | `/api/rot?d=cw\|ccw` | 屏幕旋转方向 |
| GET | `/api/line?n=0..4\|255` | 选进化线（255=随机） |
| GET | `/api/warp?h=N` | **调试**：快进 N 小时 |
| GET | `/api/reset?k=1234` | **调试**：重置成新蛋（保留选线） |
| GET | `/api/ota/arm?k=1234` | OTA 授权 60 秒 |
| POST | `/api/ota` | 上传 firmware.bin |
| POST | `/api/wifi` | 保存 WiFi |

---

## 架构要点

- **单缓冲 + 脏矩形**：库的 `flipDMABuffer()` 会忙等 EOF 并重连 DMA，双缓冲在 15 FPS 下反而闪；改为单缓冲后靠像素差量更新，单帧 0.3–0.8 ms。
- **相邻帧不做像素差分**：面板一旦被 HUD / 战斗 / 特效覆写，差分会漏画并留下残影，因此除「完全同一帧」外一律整块擦除重画。
- **渲染器切换强制重绘**：主界面 / 战斗 / 进化特效三套渲染器切换时清屏，避免战斗横幅、血条残留。
- **旋转**：该库的 `setRotation()` 未实现，改为 `RotPanel` 子类重写 `drawPixel` 等做坐标映射。
- **角色表驱动**：物种表 `SpeciesRow{stage,line,next,wins,ch,name}` 支持多进化线，退化限定在同线内。

---

## 已知限制 / Roadmap

- 巴达兽线与迪路兽线**没有幼年期**（源头素材缺 Tokomon / Plotmon），直接从成长期起步
- Flash 占用 ~79%（1.56 MB / 1.875 MB），继续加角色需要改 4bpp 打包
- D-3 的 **Mash 连点战斗** 与 **步数地图推进** 尚未实现
- 音效未接（面板无喇叭，可在 App 端补）

---

## 致谢

- [ESP32-HUB75-MatrixPanel-DMA](https://github.com/mrcodetastic/ESP32-HUB75-MatrixPanel-DMA) —— 面板驱动
- [kongtaehun/digimon-assets-VB](https://github.com/kongtaehun/digimon-assets-VB) —— 精灵素材来源
- [itsTylerIRL/Digiflip](https://github.com/itsTylerIRL/Digiflip) —— 战斗交换制与双槽存档的机制参考（未使用其代码/素材）

---

## 许可

代码采用 **MIT**（见 `LICENSE`）。

角色美术数据的权利归属与使用限制见 **`NOTICE.md`**：这些数据版权属于 Bandai / 本乡昭由 / 东映动画，本项目为非商业的个人项目。
