"""用和固件相同的常数/规则，模拟 500 次养成，检查 15~30 天节奏下能不能活到究极体。

策略模拟「正常玩家」：每 2 小时看一次，饿了喂、脏了清、体力够就打两场/天。
"""
import random
import statistics

MIN = 60
HOUR = 3600
DAY = 86400

HUNGER_DROP = 2 * HOUR
MOOD_DECAY = 10 * MIN
HP_REGEN = 30 * MIN
HP_SICK = 10 * MIN

RANGES = {0: (12 * HOUR, 24 * HOUR), 1: (2 * DAY, 4 * DAY), 2: (3 * DAY, 6 * DAY),
          3: (3.5 * DAY, 7 * DAY), 4: (6 * DAY, 12 * DAY)}
WINS_NEED = {2: 1, 3: 2, 4: 5}


def run(seed):
    random.seed(seed)
    now = 0.0
    stage = 0
    hunger, mood, hp = 4, 80, 100
    atk, def_, weight = 5, 5, 10
    wins = losses = 0
    ev_dt = 0
    next_evolve = now + random.uniform(*RANGES[0])
    next_event = now + random.uniform(20 * MIN, 180 * MIN)
    last_check = 0
    pending = None
    battles_today = 0
    day_mark = 0
    trans = []

    while now < 45 * DAY:
        now += 60

        # ---- 玩家每 2 小时巡视 ----
        if now - last_check >= 2 * HOUR:
            last_check = now
            if now - day_mark >= DAY:
                day_mark = now
                battles_today = 0
            # 喂到 3 颗心（现实里就是点 2~3 下）
            while hunger < 3:
                hunger += 1
            if pending:
                pending = None
            if hp > 35 and stage >= 2 and battles_today < 2:
                battles_today += 1
                base = atk * 2 + def_
                power = base + random.randint(-6, 6)
                enemy = base + random.randint(-10, 12)
                if power >= enemy:
                    wins += 1
                    atk += 1
                    hp = max(0, hp - random.randint(4, 10))
                else:
                    losses += 1
                    hp = max(0, hp - random.randint(10, 18))

        # ---- 自然衰减 ----
        if int(now) % HUNGER_DROP == 0:
            hunger = max(0, hunger - 1)
        if hunger == 0 and int(now) % MOOD_DECAY == 0:
            mood = max(0, mood - 1)
        if int(now) % HP_REGEN == 0 and hunger > 0:
            hp = min(100, hp + 1)

        # ---- 随机事件 ----
        if now >= next_event:
            next_event = now + random.uniform(20 * MIN, 180 * MIN)
            r = random.randint(0, 99)
            if r < 30:
                pass          # 便便（忽略，玩家会清）
            elif r < 52 and stage >= 2 and hp > 25:
                pass          # 野生遭遇（玩家 2 小时内会迎战）
            elif r < 66:
                pass          # 生病（简化：不计）
            elif r < 82:
                mood = min(100, mood + 10)
            else:
                mood = min(100, mood + 8 if False else mood)

        # ---- 死亡 ----
        if hp <= 0:
            return dict(died=True, days=now / DAY, stage=stage, wins=wins, trans=trans)

        # ---- 进化 ----
        if stage < 5 and now >= next_evolve and wins >= WINS_NEED.get(stage, 0):
            if stage == 4:
                return dict(died=False, days=now / DAY, stage=5, wins=wins, trans=trans)  # 需要勇气卡，这里假设有
            stage += 1
            trans.append((round(now / DAY, 2), stage, wins))
            next_evolve = now + random.uniform(*RANGES.get(stage, (HOUR, 2 * HOUR)))
            if stage == 2:
                atk, def_ = atk + 4, def_ + 3
            elif stage == 3:
                atk, def_ = atk + 6, def_ + 5
            elif stage == 4:
                atk, def_ = atk + 10, def_ + 8

    return dict(died=False, days=45, stage=stage, wins=wins, trans=trans)


runs = [run(s) for s in range(500)]
done = [r for r in runs if r["stage"] == 5]
dead = [r for r in runs if r["died"]]
stuck = [r for r in runs if not r["died"] and r["stage"] < 5]

print(f"样本 {len(runs)}")
print(f"到达究极体      : {len(done)/len(runs)*100:5.1f}%")
print(f"死亡            : {len(dead)/len(runs)*100:5.1f}%")
print(f"45 天内未完成   : {len(stuck)/len(runs)*100:5.1f}%")
if done:
    d = sorted(r["days"] for r in done)
    print(f"到达天数        : min={d[0]:.1f} 中位={statistics.median(d):.1f} max={d[-1]:.1f}")
if stuck:
    print("卡住阶段分布    :", {s: sum(1 for r in stuck if r["stage"] == s) for s in range(6)})

for r in runs:
    if r['stage'] < 5:
        print('STUCK stage=%d days=%.1f' % (r['stage'], r['days']), r['trans'], 'wins', r['wins'])
