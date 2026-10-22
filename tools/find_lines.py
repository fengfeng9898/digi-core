import glob
import json
import os

ROOT = r"C:\Users\Administrator\deepseekwork\vb-assets\assets\data"

db = {}
for dj in glob.glob(os.path.join(ROOT, "*", "*", "dim.json")):
    d = json.load(open(dj, encoding="utf-8"))
    base = os.path.dirname(dj)
    for m in d.get("digimon", []):
        sp = m.get("sprites", {})
        if sp.get("frame1") and m["id"] not in db:
            db[m["id"]] = (m.get("name", ""), m.get("stage", ""))

LINES = {
    "V-mon": ["Chibimon", "V-mon", "Veemon", "Fladramon", "Lighdramon", "Magnamon",
              "XV-mon", "Paildramon", "Imperialdramon"],
    "Patamon": ["Tokomon", "Patamon", "Pegasmon", "Angemon", "Shakkoumon",
                "Holy Angemon", "Seraphimon"],
    "Tailmon": ["Plotmon", "Salamon", "Tailmon", "Gatomon", "Nefertimon",
                "Silphymon", "Angewomon", "Holydramon"],
    "Hawkmon": ["Poromon", "Hawkmon", "Holsmon", "Shurimon", "Aquilamon"],
    "Armadimon": ["Upamon", "Armadimon", "Digmon", "Sabmarimon", "Ankylomon"],
    "Wormmon": ["Minomon", "Wormmon", "Stingmon"],
    "Terriermon": ["Gummymon", "Terriermon", "Galgomon", "Rapidmon"],
}

for line, wants in LINES.items():
    print(f"=== {line} ===")
    for w in wants:
        hit = [(k, v) for k, v in db.items() if w.lower() in v[0].lower()]
        if hit:
            for k, v in hit[:3]:
                print(f"   {w:<14} -> id={k:<16} {v[0]} ({v[1]})")
        else:
            print(f"   {w:<14} -> 没找到")
