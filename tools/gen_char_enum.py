"""从 vb_data.h 的 CharAnimSet 自动生成 anim_types.h 枚举 + anim.cpp 的 set_for 分支。"""
import io
import re

ROOT = r"C:\Users\Administrator\deepseekwork\esp32-digimon"
hdr = io.open(ROOT + r"\src\assets\vb_data.h", encoding="utf-8").read()

sets = re.findall(r"static const CharAnimSet (\w+)_SET =", hdr)
print("sets:", len(sets))

enum_ids = ["CH_NONE"] + ["CH_" + s for s in sets] + ["CH_JOKER", "CH_COUNT"]

# ---- anim_types.h ----
p = ROOT + r"\src\gfx\anim_types.h"
s = io.open(p, encoding="utf-8").read()
line = "enum CharId : uint8_t {\n"
for i, e in enumerate(enum_ids):
    comma = "," if i < len(enum_ids) - 1 else ""
    line += f"    {e}{comma}\n"
line += "};\n"
s = re.sub(r"enum CharId : uint8_t \{[\s\S]*?\};\n", line, s, count=1)
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("enum rewritten")

# ---- anim.cpp set_for ----
p = ROOT + r"\src\gfx\anim.cpp"
s = io.open(p, encoding="utf-8").read()
body = "static const CharAnimSet* set_for(CharId c) {\n    switch (c) {\n"
for e in sets:
    body += f"        case CH_{e}: return &{e}_SET;\n"
body += "        case CH_JOKER: return &JOKER_SET;\n"
body += "        default: return nullptr;\n    }\n}"
s = re.sub(r"static const CharAnimSet\* set_for\(CharId c\) \{[\s\S]*?\n\}", body, s, count=1)
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("set_for rewritten")
