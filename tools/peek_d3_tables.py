import csv
import glob
import os


def load(name):
    p = [f for f in glob.glob(os.path.join("d3data", "*.csv")) if name in f][0]
    with open(p, encoding="utf-8-sig", errors="replace") as f:
        return list(csv.DictReader(f)), os.path.basename(p)


pt, pf = load("d3_partner_table_original")
print(f"=== {pf} ({len(pt)} 条) 进化/伙伴表 ===")
print(" stage  id   nextEvoWins  jogress  sprite  string")
for r in pt:
    print("  {stage:>4} {id:>4} {w:>10} {j:>8} {sp:>7} {st:>7}".format(
        stage=r.get("stage", ""), id=r.get("digimon_id", ""),
        w=r.get("win_requirement_for_next_evo", ""), j=r.get("jogress_win_partner_id", ""),
        sp=r.get("sprite_index", ""), st=r.get("string_index", "")))

bt, bf = load("d3_battle_type_map")
print(f"\n=== {bf} ===")
for r in bt:
    print("  ", r)

am, af = load("d3_battle_type_map")
mm, mf = load("d3_map_areas.csv")
print(f"\n=== {mf} ({len(mm)} 条) 地图推进（前 12）===")
print("  area region map steps enc friend")
for r in mm[:12]:
    print("  {a:>4} {g:>6} {m:>3} {s:>5} {e:>3} {fr:>6}".format(
        a=r.get("area_id", ""), g=r.get("region_id", ""), m=r.get("map_id", ""),
        s=r.get("steps", ""), e=r.get("encounter_type", ""), fr=r.get("friend_digimon_id", "")))

lb, lf = load("d3_link_battle_table")
print(f"\n=== {lf} ({len(lb)} 行) 战斗表字段 ===")
print("  列:", list(lb[0].keys())[:12])
for r in lb[:5]:
    print("   ", {k: r[k] for k in list(r.keys())[:8]})
