"""校验养成周期：把 stage_duration 的区间抽样一万次，看落在几天。

区间必须和 src/game/pet.cpp 的 stage_duration() 保持一致。
"""
import random

D = 86400
H = 3600

STAGES = [
    ("蛋",     12 * H, 24 * H),
    ("幼年期",  2 * D, 4 * D),
    ("成长期",  3 * D, 6 * D),
    ("成熟期",  3.5 * D, 7 * D),
    ("完全体",  6 * D, 12 * D),
]

random.seed(1)
totals = []
for _ in range(10000):
    totals.append(sum(random.uniform(lo, hi) for _, lo, hi in STAGES) / D)

totals.sort()
n = len(totals)
print("阶段区间:")
for name, lo, hi in STAGES:
    print(f"  {name:6s} {lo/D:5.2f} ~ {hi/D:5.2f} 天")
print()
print(f"到达究极体(只算时间闸门): min={totals[0]:.2f}d  p5={totals[int(n*0.05)]:.2f}d  "
      f"中位={totals[n//2]:.2f}d  p95={totals[int(n*0.95)]:.2f}d  max={totals[-1]:.2f}d")
inside = sum(1 for t in totals if 15 <= t <= 30) / n * 100
print(f"落在 15~30 天区间内: {inside:.1f}%")
