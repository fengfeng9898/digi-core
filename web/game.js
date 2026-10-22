// DIGI-CORE 游戏逻辑（浏览器预览用，逐条对齐 src/game/pet.cpp）
// 同一个文件也能被 Node require，用于和 Python 模拟器对拍。
(function (root, factory) {
  if (typeof module === 'object' && module.exports) module.exports = factory();
  else root.DigiGame = factory();
})(this, function () {
  'use strict';

  const D = 86400, H = 3600, MN = 60;
  const ST = { EGG: 0, BABY: 1, CHILD: 2, TEEN: 3, ADULT: 4, PERFECT: 5, DEAD: 6 };
  const SP = { EGG: 0, KOROMON: 1, AGUMON: 2, GREYMON: 3, METALGREYMON: 4, WARGREYMON: 5 };

  const STAGE_CN = ['数码蛋', '幼年期', '成长期', '成熟期', '完全体', '究极体', '已死亡'];
  const STAGE_EN = ['EGG', 'BABY', 'CHILD', 'TEEN', 'ADULT', 'ULTIMATE', 'DEAD'];
  const SP_CN = ['数码蛋', '滚球兽', '亚古兽', '暴龙兽', '机械暴龙兽', '战斗暴龙兽'];
  const SP_EN = ['EGG', 'KOROMON', 'AGUMON', 'GREYMON', 'METALGREYMON', 'WARGREYMON'];

  const EV = {
    HATCH: 1, EVOLVE: 2, FEED: 3, CLEAN: 4, TRAIN: 5, WIN: 6, LOSE: 7,
    POOP: 8, SICK: 9, FOUND: 10, CALL: 11, SLEEP: 12, WAKE: 13, CARD: 14,
    DEATH: 15, IGNORED: 16, HEAL: 17, PET: 18, WILD: 19
  };

  const ITEM = { COURAGE: 1, STRENGTH: 8, GUARD: 16, REVIVE: 32 };

  const CFG = {
    HUNGER_DROP_S: 2 * H,
    MOOD_DECAY_S: 10 * MN,
    HP_REGEN_S: 30 * MN,
    HP_SICK_S: 10 * MN,
    eventGap: [20 * MN, 180 * MN],
    stageDur: {
      0: [12 * H, 24 * H],
      1: [2 * D, 4 * D],
      2: [3 * D, 6 * D],
      3: [3.5 * D, 7 * D],
      4: [6 * D, 12 * D]
    },
    winsNeed: { 2: 1, 3: 2, 4: 5 }
  };

  const rr = (lo, hi) => Math.floor(lo + Math.random() * (hi - lo));
  const clampi = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  const T = (now) => Math.floor(now);

  class Pet {
    constructor(now) { this.newGame(now); }

    newGame(now) {
      const t = T(now);
      this.stage = ST.EGG;
      this.species = SP.EGG;
      this.alive = 1;
      this.sleeping = 0;
      this.hunger = 4;
      this.mood = 80;
      this.hp = 100;
      this.atk = 5; this.def = 5;
      this.weight = 10;
      this.wins = 0; this.losses = 0; this.mistakes = 0;
      this.discipline = 50;
      this.items = 0;
      this.poops = 0;
      this.sick = 0;
      this.pending = 0;           // 0 无 / 1 野遭遇 / 2 呼唤
      this.birth = t;
      this.stageStart = t;
      this.evolveAt = t + this.stageDuration(ST.EGG);
      this.lastSettle = t;
      this.nextEvent = t + this.eventGap();
      this.pendingUntil = 0;
      this.accHunger = 0; this.accMood = 0; this.accHp = 0;
      this.log = [];
      this.lastEventSeq = 0;
      this.lastHeal = 0;
      return this;
    }

    stageDuration(st) {
      const r = CFG.stageDur[st] || [H, 2 * H];
      return rr(r[0], r[1]);
    }
    eventGap() { return rr(CFG.eventGap[0], CFG.eventGap[1]); }

    logEvent(k, now) {
      this.log.unshift({ t: T(now), k });
      if (this.log.length > 16) this.log.pop();
    }

    stageName() { return STAGE_CN[this.stage]; }
    speciesName() { return SP_CN[this.species]; }

    settleOnline(dt) {
      if (this.sleeping) {
        this.accMood += dt;
        while (this.accMood >= 1800) {
          this.accMood -= 1800;
          this.mood = clampi(this.mood + 1, 0, 100);
          this.hp = clampi(this.hp + 1, 0, 100);
        }
        return;
      }
      this.accHunger += dt;
      while (this.accHunger >= CFG.HUNGER_DROP_S) {
        this.accHunger -= CFG.HUNGER_DROP_S;
        if (this.hunger > 0) this.hunger--;
        else this.mood = clampi(this.mood - 4, 0, 100);
      }
      if (this.hunger === 0 || this.poops >= 2) {
        this.accMood += dt;
        while (this.accMood >= CFG.MOOD_DECAY_S) {
          this.accMood -= CFG.MOOD_DECAY_S;
          this.mood = clampi(this.mood - 1, 0, 100);
        }
      } else this.accMood = 0;

      this.accHp += dt;
      if (this.sick) {
        while (this.accHp >= CFG.HP_SICK_S) {
          this.accHp -= CFG.HP_SICK_S;
          this.hp = clampi(this.hp - 1, 0, 100);
        }
      } else if (this.hunger > 0) {
        while (this.accHp >= CFG.HP_REGEN_S) {
          this.accHp -= CFG.HP_REGEN_S;
          this.hp = clampi(this.hp + 1, 0, 100);
        }
      }
    }

    settleOffline(dt) {
      const hungry = Math.floor(dt / CFG.HUNGER_DROP_S);
      this.hunger = hungry >= this.hunger ? 0 : this.hunger - hungry;
      this.mood = clampi(this.mood - Math.floor(dt / 1800), 0, 100);
      this.poops = Math.min(3, this.poops + Math.floor(dt / (4 * H)));
      this.mistakes += Math.floor(dt / (6 * H));
      if (this.hunger === 0) this.hp = clampi(this.hp - Math.floor(dt / (2 * H)), 0, 100);
      this.accHunger = this.accMood = this.accHp = 0;
    }

    fireEvent(now) {
      this.nextEvent = now + this.eventGap();
      if (this.sleeping) return;

      const wPoop = this.poops < 3 ? 30 : 0;
      const wWild = (this.hp > 25 && this.stage >= ST.CHILD) ? 22 : 0;
      const wSick = this.sick ? 0 : (this.poops >= 2 ? 16 : 6);
      const wFound = 12;
      const wCall = this.mood < 70 ? 20 : 8;
      const total = wPoop + wWild + wSick + wFound + wCall;
      if (total <= 0) return;

      let r = Math.floor(Math.random() * total);
      if ((r -= wPoop) < 0) { this.poops++; this.logEvent(EV.POOP, now); return; }
      if ((r -= wWild) < 0) {
        this.pending = 1; this.pendingUntil = now + 30 * MN;
        this.logEvent(EV.WILD, now); return;
      }
      if ((r -= wSick) < 0) { this.sick = 1; this.logEvent(EV.SICK, now); return; }
      if ((r -= wFound) < 0) {
        this.mood = clampi(this.mood + 10, 0, 100);
        this.weight += 2;
        this.logEvent(EV.FOUND, now); return;
      }
      if ((r -= wCall) < 0) {
        this.pending = 2; this.pendingUntil = now + 20 * MN;
        this.logEvent(EV.CALL, now); return;
      }
    }

    tryEvolve(now) {
      if (now < this.evolveAt) return false;
      if (this.wins < (CFG.winsNeed[this.stage] || 0)) return false;

      let ns, nsp;
      switch (this.stage) {
        case ST.EGG: ns = ST.BABY; nsp = SP.KOROMON; break;
        case ST.BABY: ns = ST.CHILD; nsp = SP.AGUMON; break;
        case ST.CHILD: ns = ST.TEEN; nsp = SP.GREYMON; break;
        case ST.TEEN: ns = ST.ADULT; nsp = SP.METALGREYMON; break;
        case ST.ADULT:
          if (!(this.items & ITEM.COURAGE)) return false;
          ns = ST.PERFECT; nsp = SP.WARGREYMON; break;
        default: return false;
      }

      this.stage = ns; this.species = nsp;
      this.stageStart = now;
      this.evolveAt = now + this.stageDuration(ns);
      this.pending = 0;

      if (ns === ST.BABY) { this.hp = 100; this.atk = 8; this.def = 6; this.weight = 14; }
      if (ns === ST.CHILD) {
        this.atk += 4; this.def += 3; this.weight += 10;
        if (this.mistakes > 4) { this.atk -= 2; this.mood -= 10; }
      }
      if (ns === ST.TEEN) { this.atk += 6; this.def += 5; this.weight += 16; }
      if (ns === ST.ADULT) { this.atk += 10; this.def += 8; this.weight += 30; }
      if (ns === ST.PERFECT) { this.atk += 15; this.def += 12; }

      this.hp = clampi(this.hp + 20, 0, 100);
      this.logEvent(EV.EVOLVE, now);
      return true;
    }

    tick(now) {
      now = T(now);
      if (!this.alive || this.stage === ST.DEAD) { this.lastSettle = now; return; }
      if (!this.lastSettle) { this.lastSettle = now; return; }
      if (now + 60 < this.lastSettle) { this.rebase(this.lastSettle - now); return; }

      const dt = now - this.lastSettle;
      if (dt === 0) return;
      if (dt > 30 * D) { this.lastSettle = now; return; }

      if (dt > 2 * H) this.settleOffline(dt);
      else this.settleOnline(dt);
      this.lastSettle = now;

      if (this.pending && now > this.pendingUntil) {
        this.pending = 0;
        this.mood = clampi(this.mood - 8, 0, 100);
        this.mistakes++;
        this.logEvent(EV.IGNORED, now);
      }

      let guard = 0;
      while (this.alive && now >= this.nextEvent && guard++ < 8) this.fireEvent(now);

      if (now - this.lastHeal >= 60) {
        this.lastHeal = now;
        if (this.sick && this.poops === 0 && this.hunger > 0 && Math.random() < 0.02) this.sick = 0;
      }

      if (this.tryEvolve(now)) this.nextEvent = now + this.eventGap();
      if (this.hp === 0) this.die(now);
    }

    rebase(delta) {
      this.birth += delta; this.stageStart += delta; this.evolveAt += delta;
      this.lastSettle += delta; this.nextEvent += delta;
      if (this.pendingUntil) this.pendingUntil += delta;
    }

    die(now) {
      if (!this.alive) return;
      this.stage = ST.DEAD; this.alive = 0; this.species = SP.EGG; this.pending = 0;
      this.logEvent(EV.DEATH, now);
    }

    act(cmd, now) {
      now = T(now);
      if (!this.alive) return { ok: false, msg: 'GONE...' };

      switch (cmd) {
        case 'feed':
          if (this.sleeping) return { ok: false, msg: 'ZZZ...' };
          if (this.hunger >= 4) return { ok: false, msg: 'NOT HUNGRY' };
          this.hunger++;
          this.weight = clampi(this.weight + 1, 0, 999);
          this.mood = clampi(this.mood + 3, 0, 100);
          this.logEvent(EV.FEED, now);
          return { ok: true, msg: 'YUM!' };

        case 'clean':
          if (this.poops === 0) return { ok: false, msg: 'ITS CLEAN' };
          this.poops = 0;
          this.mood = clampi(this.mood + 5, 0, 100);
          this.logEvent(EV.CLEAN, now);
          return { ok: true, msg: 'CLEAN!' };

        case 'pet':
          if (this.pending === 2) this.pending = 0;
          this.mood = clampi(this.mood + 8, 0, 100);
          this.logEvent(EV.PET, now);
          return { ok: true, msg: 'HAPPY!' };

        case 'train':
          if (this.sleeping) return { ok: false, msg: 'ZZZ...' };
          if (this.hunger === 0) { this.mistakes++; return { ok: false, msg: 'TOO HUNGRY' }; }
          if (this.hp < 10) return { ok: false, msg: 'TOO TIRED' };
          this.atk = clampi(this.atk + 1, 1, 99);
          this.def = clampi(this.def + Math.floor(Math.random() * 2), 1, 99);
          this.weight = clampi(this.weight - 1, 1, 999);
          this.hp = clampi(this.hp - 4, 0, 100);
          this.mood = clampi(this.mood - 2, 0, 100);
          this.logEvent(EV.TRAIN, now);
          return { ok: true, msg: 'TRAINED!' };

        case 'battle': {
          if (this.sleeping) return { ok: false, msg: 'ZZZ...' };
          if (this.hp < 15) return { ok: false, msg: 'TOO TIRED' };
          if (this.hunger === 0) return { ok: false, msg: 'TOO HUNGRY' };

          const ambush = this.pending === 1;
          if (ambush) this.pending = 0;

          const base = this.atk * 2 + this.def;
          const power = base + rr(-6, 7) + (ambush ? 4 : 0);
          const enemy = base + rr(-10, 12);
          const win = power >= enemy;

          if (win) {
            this.wins++;
            this.mood = clampi(this.mood + 10, 0, 100);
            this.atk = clampi(this.atk + 1, 1, 99);
            this.hp = clampi(this.hp - rr(4, 10), 0, 100);
            this.logEvent(EV.WIN, now);
          } else {
            this.losses++;
            this.mood = clampi(this.mood - 8, 0, 100);
            this.hp = clampi(this.hp - rr(10, 18), 0, 100);
            this.mistakes++;
            this.logEvent(EV.LOSE, now);
          }
          const dead = this.hp === 0;
          if (dead) this.die(now);
          return { ok: true, msg: dead ? 'KO...' : (win ? (ambush ? 'AMBUSH WIN!' : 'YOU WIN!') : 'YOU LOSE..'), win, dead };
        }

        case 'sleep':
          if (this.sleeping) return { ok: false, msg: 'ASLEEP' };
          this.sleeping = 1; this.pending = 0;
          this.logEvent(EV.SLEEP, now);
          return { ok: true, msg: 'GOOD NIGHT' };

        case 'wake':
          if (!this.sleeping) return { ok: false, msg: 'AWAKE' };
          this.sleeping = 0; this.accHunger = 0;
          this.logEvent(EV.WAKE, now);
          return { ok: true, msg: 'GOOD DAY' };

        case 'evolve':
          if (this.tryEvolve(now)) return { ok: true, msg: 'EVOLVED!' };
          if (now < this.evolveAt) return { ok: false, msg: 'TOO EARLY' };
          if (this.wins < (CFG.winsNeed[this.stage] || 0)) return { ok: false, msg: 'NEED WINS' };
          return { ok: false, msg: 'NEED CARD' };

        default:
          return { ok: false, msg: 'UNKNOWN' };
      }
    }

    card(id, now) {
      now = T(now);
      switch (id) {
        case 'courage': this.items |= ITEM.COURAGE; this.logEvent(EV.CARD, now); return { ok: true, msg: 'COURAGE!' };
        case 'power':
          if (this.items & ITEM.STRENGTH) return { ok: false, msg: 'USED' };
          this.items |= ITEM.STRENGTH; this.atk = clampi(this.atk + 5, 1, 99);
          this.logEvent(EV.CARD, now); return { ok: true, msg: 'POWER UP!' };
        case 'guard':
          if (this.items & ITEM.GUARD) return { ok: false, msg: 'USED' };
          this.items |= ITEM.GUARD; this.def = clampi(this.def + 5, 1, 99);
          this.logEvent(EV.CARD, now); return { ok: true, msg: 'GUARD UP!' };
        case 'heal':
          this.hp = 100; this.mood = 100; this.sick = 0;
          this.logEvent(EV.HEAL, now); return { ok: true, msg: 'FULL HEAL' };
        case 'revive':
          if (this.alive) return { ok: false, msg: 'NO NEED' };
          this.alive = 1; this.stage = ST.BABY; this.species = SP.KOROMON;
          this.stageStart = now; this.evolveAt = now + this.stageDuration(ST.BABY);
          this.lastSettle = now; this.nextEvent = now + this.eventGap();
          this.hp = 60; this.mood = 60; this.hunger = 3; this.poops = 0;
          this.sick = 0; this.pending = 0; this.items |= ITEM.REVIVE;
          this.logEvent(EV.HEAL, now); return { ok: true, msg: 'REVIVED!' };
        default: return { ok: false, msg: 'NO CARD' };
      }
    }

    age(now) { return Math.max(0, T(now) - this.birth); }
    evolveIn(now) { return Math.max(0, this.evolveAt - T(now)); }

    snapshot(now) {
      return {
        stage: this.stage, stageName: STAGE_CN[this.stage], stageEn: STAGE_EN[this.stage],
        species: this.species, speciesName: SP_CN[this.species], speciesEn: SP_EN[this.species],
        hunger: this.hunger, mood: this.mood, hp: this.hp,
        atk: this.atk, def: this.def, weight: this.weight,
        age: this.age(now), evolveIn: this.evolveIn(now),
        wins: this.wins, losses: this.losses, mistakes: this.mistakes,
        poops: this.poops, sick: this.sick, pending: this.pending,
        pendingIn: Math.max(0, this.pendingUntil - T(now)),
        sleeping: this.sleeping, alive: this.alive, items: this.items,
        winsNeed: CFG.winsNeed[this.stage] || 0,
        log: this.log
      };
    }
  }

  return { Pet, ST, SP, EV, ITEM, CFG, STAGE_CN, SP_CN };
});
