#!/usr/bin/env python3
"""
feishu_bridge.py — 飞书 ↔ DIGI-CORE 桥接

为什么需要它：
    ESP32 在家里局域网里，飞书云端无法直接访问它。所以由这台常开的电脑做桥：
    飞书开放平台的事件用「长连接」推给本进程（不需要公网 IP），本进程再通过
    局域网 HTTP 调设备的 /api/act 接口，把结果回发到飞书。

职责：
    1. 接收飞书消息 → 解析成设备指令 → POST 设备 → 回消息
    2. 每 3 秒轮询设备状态 → 发现进化/死亡 → 主动播报到群里

依赖：
    pip install lark-oapi requests

配置：
    复制 config.example.json 为 config.json，填入 app_id / app_secret / device。
"""
from __future__ import annotations

import json
import os
import re
import sys
import threading
import time
import traceback

import requests

try:
    import lark_oapi as lark
    from lark_oapi.api.im.v1 import (
        CreateMessageRequest,
        CreateMessageRequestBody,
        P2ImMessageReceiveV1,
    )
except ImportError:  # pragma: no cover
    print("[fatal] 缺少依赖: pip install lark-oapi requests")
    raise

HERE = os.path.dirname(os.path.abspath(__file__))
CONFIG_PATH = os.path.join(HERE, "config.json")

# ------------------------------------------------------------------ 配置 ----
def load_config() -> dict:
    if not os.path.exists(CONFIG_PATH):
        print(f"[fatal] 找不到 {CONFIG_PATH}\n        请先复制 config.example.json 为 config.json")
        sys.exit(1)
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        cfg = json.load(f)
    for k in ("app_id", "app_secret", "device"):
        if not cfg.get(k):
            print(f"[fatal] config.json 缺少字段: {k}")
            sys.exit(1)
    cfg.setdefault("poll_seconds", 3)
    cfg.setdefault("notify_chat_id", "")
    return cfg


CFG = load_config()
DEVICE = CFG["device"].rstrip("/")
POLL_S = float(CFG.get("poll_seconds", 3))

_client = lark.Client.builder().app_id(CFG["app_id"]).app_secret(CFG["app_secret"]).build()

# ------------------------------------------------------------------ 指令表 ----
# (匹配词, 设备指令, 说明)
COMMANDS = [
    (("喂食", "喂饭", "吃饭", "喂"), "feed", "喂食"),
    (("清洁", "清理", "铲屎", "打扫"), "clean", "清洁"),
    (("训练", "练"), "train", "训练"),
    (("战斗", "打架", "对战"), "battle", "战斗"),
    (("睡觉", "睡"), "sleep", "睡觉"),
    (("唤醒", "起床", "醒醒"), "wake", "唤醒"),
    (("进化", "变身"), "evolve", "进化检查"),
]

CARDS = {
    "勇气": "courage", "勇敢": "courage",
    "力量": "power", "攻击": "power",
    "守护": "guard", "防御": "guard",
    "回复": "heal", "治疗": "heal",
    "复活": "revive",
}

HELP_TEXT = (
    "DIGI-CORE 指令：\n"
    "喂食 / 清洁 / 训练 / 战斗 / 睡觉 / 唤醒 / 状态 / 进化\n"
    "卡片 火|水|自然|力量|守护|回复|复活\n"
    "例：卡片 火"
)


# ------------------------------------------------------------------ 设备 ----
def device_state() -> dict | None:
    try:
        r = requests.get(f"{DEVICE}/api/state", timeout=5)
        r.raise_for_status()
        return r.json()
    except Exception as e:  # noqa: BLE001
        print(f"[device] state 失败: {e}")
        return None


def device_action(cmd: str) -> dict | None:
    try:
        r = requests.get(f"{DEVICE}/api/act", params={"c": cmd}, timeout=8)
        r.raise_for_status()
        return r.json()
    except Exception as e:  # noqa: BLE001
        print(f"[device] act({cmd}) 失败: {e}")
        return None


def device_card(card_id: str) -> dict | None:
    try:
        r = requests.get(f"{DEVICE}/api/card", params={"id": card_id}, timeout=8)
        r.raise_for_status()
        return r.json()
    except Exception as e:  # noqa: BLE001
        print(f"[device] card({card_id}) 失败: {e}")
        return None


def fmt_state(s: dict) -> str:
    hearts = "♥" * int(s.get("hunger", 0)) + "♡" * (4 - int(s.get("hunger", 0)))
    status = "死亡" if not s.get("alive") else ("睡眠中" if s.get("sleeping") else "活动中")
    return (
        f"{s.get('species')} · {s.get('stageName')}\n"
        f"饥饿 {hearts}　心情 {s.get('mood')}　体力 {s.get('hp')}\n"
        f"攻 {s.get('atk')} / 防 {s.get('def')}　体重 {s.get('weight')}g\n"
        f"年龄 {int(s.get('age', 0)) // 60} 分钟　战绩 {s.get('wins')} 胜 / {s.get('losses')} 负\n"
        f"状态：{status}"
    )


# ------------------------------------------------------------------ 飞书 ----
def send_text(chat_id: str, text: str) -> None:
    body = (
        CreateMessageRequestBody.builder()
        .receive_id(chat_id)
        .msg_type("text")
        .content(json.dumps({"text": text}, ensure_ascii=False))
        .build()
    )
    req = (
        CreateMessageRequest.builder()
        .receive_id_type("chat_id")
        .request_body(body)
        .build()
    )
    resp = _client.im.v1.message.create(req)
    if not resp.success():
        print(f"[feishu] 发送失败 code={resp.code} msg={resp.msg}")


def reply(data: P2ImMessageReceiveV1, text: str) -> None:
    msg = data.event.message
    send_text(msg.chat_id, text)


def parse_command(text: str) -> tuple[str, str] | None:
    """返回 (类型, 值)。类型: 'act' / 'card' / 'state' / 'help'"""
    t = text.strip()
    t = re.sub(r"@\S+", "", t).strip()          # 去掉 @机器人
    if not t:
        return None
    if t in ("帮助", "help", "?", "？"):
        return ("help", "")

    if t.startswith("卡片") or t.startswith("卡"):
        rest = t[2:] if t.startswith("卡片") else t[1:]
        rest = rest.strip()
        for name, cid in CARDS.items():
            if name in rest:
                return ("card", cid)
        return ("help", "")

    if t in ("状态", "看看", "情况", "status"):
        return ("state", "")

    for words, cmd, _ in COMMANDS:
        if any(w == t or w in t for w in words):
            return ("act", cmd)
    return None


def on_message(data: P2ImMessageReceiveV1) -> None:
    try:
        msg = data.event.message
        if msg.message_type != "text":
            reply(data, "目前只支持文字指令哦，发「帮助」看菜单。")
            return
        content = json.loads(msg.content)
        text = content.get("text", "")
        parsed = parse_command(text)
        if not parsed:
            reply(data, "没看懂这条指令。\n" + HELP_TEXT)
            return

        kind, value = parsed
        if kind == "help":
            reply(data, HELP_TEXT)
            return
        if kind == "state":
            s = device_state()
            reply(data, fmt_state(s) if s else "设备没有响应，检查一下是否在线。")
            return
        if kind == "card":
            res = device_card(value)
            reply(data, f"注入卡片：{res.get('msg')}" if res else "设备没有响应。")
            return
        if kind == "act":
            res = device_action(value)
            if not res:
                reply(data, "设备没有响应，检查一下是否在线。")
                return
            msg_txt = res.get("msg", "")
            st = res.get("state") or {}
            tail = f"\n{st.get('species')} · {st.get('stageName')}" if st else ""
            reply(data, f"{msg_txt}{tail}")
            return
    except Exception:  # noqa: BLE001
        traceback.print_exc()


def build_ws_client():
    handler = (
        lark.EventDispatcherHandler.builder("", "")
        .register_p2_im_message_receive_v1(on_message)
        .build()
    )
    return lark.ws.Client(
        CFG["app_id"],
        CFG["app_secret"],
        event_handler=handler,
        log_level=lark.LogLevel.INFO,
    )


# -------------------------------------------------------------- 状态轮询 ----
def poll_loop() -> None:
    """设备自己不会推消息，靠桥轮询发现进化/死亡。"""
    last_stage = None
    while True:
        s = device_state()
        if s:
            key = (s.get("stage"), s.get("alive"))
            if last_stage is not None and key != last_stage:
                chat = CFG.get("notify_chat_id")
                if chat:
                    if not s.get("alive"):
                        send_text(chat, f"💀 {s.get('species')} 倒下了…")
                    else:
                        send_text(chat, f"✨ 进化！{s.get('species')} · {s.get('stageName')}")
            last_stage = key
        time.sleep(POLL_S)


def main() -> None:
    print(f"[bridge] device = {DEVICE}")
    s = device_state()
    print("[bridge] 设备状态:", "在线" if s else "离线（稍后自动重试）")

    threading.Thread(target=poll_loop, daemon=True).start()

    print("[bridge] 正在建立飞书长连接… (Ctrl+C 退出)")
    build_ws_client().start()


if __name__ == "__main__":
    main()
