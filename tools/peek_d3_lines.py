import csv
import glob
import os

D = "d3data"


def load(name):
    p = [f for f in glob.glob(os.path.join(D, "*.csv")) if name in f][0]
    with open(p, encoding="utf-8-sig", errors="replace") as f:
        return list(csv.DictReader(f))


names = {r["string_index"]: r["name"] for r in load("d3_names_original")}
pt = load("d3_partner_table_original")

print("stage | 名字            | 进化要胜场 | jogress partner")
print("-" * 62)
for r in pt:
    nm = names.get(r["string_index"], "?")
    print("{:>5} | {:<15} | {:>10} | {}".format(
        r["stage"], nm[:15], r["win_requirement_for_next_evo"], r["jogress_win_partner_id"]))
