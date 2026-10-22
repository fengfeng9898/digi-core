# DIGI-CORE

ESP32-DevKitC-32E + 独立 HUB75E 64×64 RGB 点阵屏，做成一台数码暴龙机风格的养成机。
**无按键**：所有操作来自局域网网页；飞书通过本地桥接服务转发指令。

---

## 当前进度

| 模块 | 状态 |
|---|---|
| 工具链（PlatformIO + espressif32 6.8.1 + Arduino core 2.0.17） | ✅ 编译通过 |
| HUB75 64×64 驱动封装（6bit 色深 / 双缓冲 / 15 MHz） | ✅ 已接入 |
| 精灵管线（程序化生成 → 索引色 ASCII → 逐像素绘制） | ✅ 5 个形象 |
| 养成状态机（蛋→幼年→成长→成熟→完全体、饥饿/心情/体力/便便/睡眠/死亡） | ✅ |
| NVS 存档（CRC 校验、30 秒心跳、动作即存） | ✅ |
| 局域网网页控制台（状态 / 动作 / 卡片 / 亮度） | ✅ |
| WiFi 配网（连不上自动回退热点） | ✅ |
| 飞书桥接（长连接收指令 + 进化播报） | ✅ 代码完成，待填 app 凭证 |
| 音效（蜂鸣器） | ⬜ 未接线，未实现 |
| 战斗动画 / 回合制交互 | ⬜ 目前是即时结算 |
| 外壳 / 锂电 | ⬜ |

---

## 硬件接线

| 面板 | GPIO | | 面板 | GPIO |
|---|---|---|---|---|
| R1 | 25 | | A | 23 |
| G1 | 26 | | B | 19 |
| B1 | 27 | | C | 5 |
| R2 | 14 | | D | 17 |
| G2 | 13 | | E | **16** |
| B2 | 4 | | LAT | 21 |
| CLK | 18 | | OE | 22 |

- `E` 脚必须接，否则 64×64 只有一半画面。
- 面板 5 V 单独供电（≥4 A），与 ESP32 共地；面板电源端并 1000 µF。
- 强烈建议加 74HCT245 做 3.3 V→5 V 电平转换。
- 目前**不需要接任何按键**；板载 `BOOT` 键保留为「长按 3 秒清空 WiFi 配置」的恢复手段。

---

## 编译 / 烧录

```powershell
cd C:\Users\Administrator\deepseekwork\esp32-digimon
python -m platformio run                 # 编译
python -m platformio run -t upload       # 烧录（USB 连板子）
python -m platformio device monitor      # 串口日志 115200
```

串口应能看到：

```
=== DIGI-CORE 0.1.0 ===
[panel] 64x64 chain=1 depth=6 refresh=... freeheap=...
[save] new  stage=EGG species=EGG age=0s
[net] mode=AP ip=192.168.4.1
```

`refresh` 应 ≥ 120 Hz。若 `begin() FAILED` 或画面错位，先查供电与 E 脚。

---

## 首次使用

1. 上电后若没配过网，屏幕显示 `192.168.4` / `.1`，设备开热点 `DIGI-CORE-XXXX`，密码 `digipet1`。
2. 手机连该热点 → 打开 `http://192.168.4.1` → 填家里 2.4 GHz WiFi 名称和密码 → 保存。
3. 设备重启后屏幕显示局域网 IP，之后手机/电脑连同一 WiFi 打开该 IP 即可控制。
4. 也可以试 `http://digicore.local`（mDNS，部分安卓不支持）。

网页能力：喂食 / 清洁 / 训练 / 战斗 / 睡觉 / 唤醒 / 7 种升级卡 / 亮度调节 / 实时状态。

> 换路由器或忘记密码：长按板载 BOOT 键 3 秒，设备清空 WiFi 配置并回到配网模式。

---

## 飞书接入

```powershell
pip install lark-oapi requests
cd bridge
copy config.example.json config.json   # 填 app_id / app_secret / device 地址
python feishu_bridge.py
```

飞书开放平台侧需要：
1. 创建「企业自建应用」，开启**机器人**能力；
2. 事件订阅选**长连接**方式，订阅 `im.message.receive_v1`；
3. 权限：`im:message`、`im:message:send_as_bot`；
4. 发布版本，把机器人拉进群。

机器人指令：`喂食` `清洁` `训练` `战斗` `睡觉` `唤醒` `状态` `进化` `卡片 火`（水/自然/力量/守护/回复/复活）。

---

## 已知限制 / 待办

1. **WiFi 与点阵共存的干扰**：库文档明确警告高频 DMA 可能影响 Wi-Fi。实测若网页卡顿或画面闪，把 `PANEL_I2S` 降到 10–15 MHz 或色深降到 5bit。
2. 战斗目前是「立即结算 + 屏幕提示」，没有逐回合画面。
3. 音效未实现（`PIN_BUZZER` 已预留 GPIO2）。
4. 精灵是程序化生成的占位美术，替换时只需覆盖 `src/assets/sprites.h`（保持同样的 ASCII 索引格式）。
5. 网页是同步 WebServer，单用户操作没问题，多人同时访问会排队。

---

## 目录

```
src/
  main.cpp            启动 / 主循环
  config.h            引脚、屏幕、玩法参数
  panel.h             面板全局对象
  assets/sprites.h    生成的精灵数据
  gfx/sprite.*        索引色精灵绘制
  game/pet.*          养成状态机 / 进化 / 战斗 / 卡片
  game/save.*         NVS 存档 + WiFi 配置
  ui/ui.*             64×64 界面渲染
  net/net.*           WiFi + HTTP API
  net/webui.h         内嵌网页
tools/
  gen_sprites.py      精灵生成器
  preview.py          终端预览精灵
bridge/
  feishu_bridge.py    飞书 ↔ 设备桥接
```
