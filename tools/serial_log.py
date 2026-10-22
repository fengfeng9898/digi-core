"""复位 ESP32 并抓取启动日志。

用法: python tools/serial_log.py [COM12] [秒数] [noreset]
  noreset  不复位，直接监听（用来确认运行是否稳定）
"""
import sys
import time

import serial

PORT = sys.argv[1] if len(sys.argv) > 1 else "COM12"
SECONDS = float(sys.argv[2]) if len(sys.argv) > 2 else 8.0
NO_RESET = len(sys.argv) > 3 and sys.argv[3] == "noreset"

ser = serial.Serial(PORT, 115200, timeout=0.2)
if not NO_RESET:
    # 正常启动（GPIO0 高），用 RTS 拉低 EN 复位
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.12)
    ser.setRTS(False)

buf = bytearray()
end = time.time() + SECONDS
while time.time() < end:
    chunk = ser.read(4096)
    if chunk:
        buf.extend(chunk)

ser.close()
print(f"--- {len(buf)} bytes ---")
print(buf.decode("utf-8", "replace"))
