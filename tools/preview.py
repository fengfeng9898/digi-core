import re
import sys

sys.stdout.reconfigure(encoding="utf-8")
txt = open("src/assets/sprites.h", encoding="utf-8").read()
for name in ("SPR_EGG", "SPR_BABY", "SPR_CHILD", "SPR_TEEN", "SPR_DEAD"):
    m = re.search(name + r"\[\] = \{(.*?)\};", txt, re.S)
    rows = re.findall(r'"([0-9A-F]+)"', m.group(1))
    print(f"--- {name}  {len(rows[0])}x{len(rows)}")
    for r in rows:
        print(r.replace("0", "."))
    print()
