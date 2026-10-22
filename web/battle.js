// 回合制战斗演出（切镜版）
//   我方攻击(单) -> 敌方受伤(单) -> 敌方攻击(单) -> 我方受伤(单) -> 胜利/失败
// 每个镜头只有一个角色占满屏，不再左右分屏。
(function (root) {
  'use strict';

  const FULL_Y = 9;        // 64x40 动画帧的绘制起点（与主界面一致）

  function clipTotal(clip) {
    return clip.dur.reduce((a, b) => a + b, 0);
  }

  class BattleScene {
    constructor(data, renderer) {
      this.data = data;
      this.r = renderer;
      this.active = false;
    }

    enemyName(key) {
      return { koromon: 'KOROMON', agumon: 'AGUMON', greymon: 'GREYMON', joker: 'JOKER' }[key] || 'WILD';
    }

    start(opts, nowMs) {
      const me = this.data.chars[opts.myChar];
      const en = this.data.chars[opts.enemyChar];
      const cap = (c, ms) => Math.min(ms, clipTotal(c));

      this.active = true;
      this.t0 = nowMs;
      this.pi = 0;
      this.phaseStart = 0;
      this.win = opts.win;
      this.myChar = opts.myChar;
      this.enemyChar = opts.enemyChar;
      this.hpBefore = opts.hpBefore;
      this.hpAfter = opts.hpAfter;
      this.enemyHp = 100;
      this.revert = !!opts.revert;

      this.phases = [
        { name: 'intro', dur: 450, shot: 'enemy' },
        { name: 'myAttack', dur: cap(me.anims.attack, 1400), shot: 'me' },
        { name: 'enemyHurt', dur: cap(en.anims.hurt, 1000), shot: 'enemy' }
      ];
      if (this.revert) this.phases.unshift({ name: 'formUp', dur: 520, shot: 'me' });
      if (opts.win) {
        this.phases.push({ name: 'enemyDown', dur: 420, shot: 'enemy' });
        this.phases.push({ name: 'myWin', dur: cap(me.anims.win, 1500), shot: 'me' });
        this.phases.push({ name: 'end', dur: 500, shot: 'me' });
      } else {
        this.phases.push({ name: 'enemyAttack', dur: cap(en.anims.attack, 1300), shot: 'enemy' });
        this.phases.push({ name: 'myHurt', dur: cap(me.anims.hurt, 1200), shot: 'me' });
        this.phases.push({ name: 'end', dur: 550, shot: 'me' });
      }
    }

    // 受伤动画里「倒下那一下」大约在 55% 处；用它做定格，避免停到恢复站起帧
    collapseFrame(clip) {
      return this.atTime(clip, clipTotal(clip) * 0.55);
    }

    atTime(clip, tt) {
      let acc = 0;
      for (let i = 0; i < clip.frames.length; i++) {
        acc += clip.dur[i] || 100;
        if (tt < acc) return clip.frames[i];
      }
      return clip.frames[clip.frames.length - 1];
    }

    // 把镜头时间映射到动画帧（超出 cap 就停在最后一帧）
    frameAt(clip, t, cap) {
      const total = clipTotal(clip);
      const tt = Math.max(0, Math.min(total - 1, t * total / Math.max(1, cap)));
      let acc = 0;
      for (let i = 0; i < clip.frames.length; i++) {
        acc += clip.dur[i] || 100;
        if (tt < acc) return clip.frames[i];
      }
      return clip.frames[clip.frames.length - 1];
    }

    draw(ctx, nowMs, nowSec) {
      if (!this.active) return;
      const el = nowMs - this.t0;

      while (this.pi < this.phases.length) {
        const p = this.phases[this.pi];
        if (el - this.phaseStart < p.dur) break;
        this.phaseStart += p.dur;
        this.pi++;
      }
      if (this.pi >= this.phases.length) { this.active = false; return; }

      const phase = this.phases[this.pi];
      const t = el - this.phaseStart;
      const me = this.data.chars[this.myChar];
      const en = this.data.chars[this.enemyChar];

      // ---- 背景 + 顶部黑条 ----
      ctx.fillStyle = '#000';
      ctx.fillRect(0, 0, 64, 8);
      const realSec = (this.r && this.r.nowSource) ? this.r.nowSource() : Date.now() / 1000;
      if (this.r && this.r.bg) this.r.bg.draw(ctx, realSec, this.r.sceneMode || 'auto');
      else ctx.fillRect(0, 8, 64, 56);

      // ---- 血条 ----
      const hurtDone = (phase.name === 'myHurt' || phase.name === 'end');
      const myHp = hurtDone ? this.hpAfter : this.hpBefore;
      let enemyHp = 100;
      if (phase.name === 'enemyHurt') enemyHp = 40;
      if (phase.name === 'enemyDown' || phase.name === 'myWin') enemyHp = 0;
      if (phase.name === 'enemyAttack' || phase.name === 'myHurt' || phase.name === 'end') enemyHp = 40;
      if (this.win && phase.name === 'end') enemyHp = 0;

      this.hpBar(ctx, 1, 1, 30, myHp, '#3fb950');
      this.hpBar(ctx, 33, 1, 30, enemyHp, '#f85149');

      // ---- 镜头 ----
      const shot = phase.shot;
      const who = shot === 'me' ? me : en;

      let frame = null;
      let flash = 0;
      switch (phase.name) {
        case 'formUp':      frame = this.frameAt(me.anims.idle, t, phase.dur);
                            flash = t < 220 ? 0.6 : 0; break;
        case 'intro':       frame = this.frameAt(en.anims.idle, t, phase.dur); break;
        case 'myAttack':    frame = this.frameAt(me.anims.attack, t, phase.dur); break;
        case 'enemyHurt':   frame = this.frameAt(en.anims.hurt, t, phase.dur);
                            flash = t < 140 ? 0.85 : 0; break;
        case 'enemyDown':   frame = this.collapseFrame(en.anims.hurt); break;
        case 'myWin':       frame = this.frameAt(me.anims.win, t, phase.dur); break;
        case 'enemyAttack': frame = this.frameAt(en.anims.attack, t, phase.dur); break;
        case 'myHurt':      frame = this.frameAt(me.anims.hurt, t, phase.dur);
                            flash = t < 140 ? 0.8 : 0; break;
        case 'end':         frame = this.win ? this.collapseFrame(me.anims.win)
                                             : this.collapseFrame(me.anims.hurt); break;
        default: break;
      }

      if (frame) {
        // 野生兽用暗色区分；小丑王本身配色就够跳，保持原色
        const tint = (shot === 'enemy' && this.enemyChar !== 'joker') ? [0.72, 0.55, 0.62] : null;
        if (this.r.drawFrame) this.r.drawFrame(frame, who.pal, FULL_Y, tint, flash);
        else this.plainFrame(ctx, frame, who.pal);
      }

      // 敌方倒下时向上淡出
      if (phase.name === 'enemyDown') {
        const k = Math.min(1, t / phase.dur);
        ctx.fillStyle = 'rgba(0,0,0,' + (k * 0.85).toFixed(2) + ')';
        ctx.fillRect(0, 8, 64, 56);
      }

      // ---- 文字 ----
      const msg = {
        formUp: 'CARD!',
        intro: this.enemyName(this.enemyChar) + '!',
        myAttack: 'GO!',
        enemyHurt: 'HIT!',
        enemyDown: 'KO!',
        myWin: 'YOU WIN!',
        enemyAttack: 'ENEMY!',
        myHurt: 'OUCH!',
        end: this.revert ? 'REVERT..' : (this.win ? 'WINNER!' : 'DEFEAT..')
      }[phase.name] || '';
      this.r.textCentered(msg, 56, '#fff');
    }

    plainFrame(ctx, rows, pal) {
      for (let y = 0; y < rows.length; y++) {
        const row = rows[y];
        for (let x = 0; x < row.length; x++) {
          const idx = parseInt(row[x], 16);
          if (!idx) continue;
          const [r, g, b] = ((c) => [((c >> 11) & 31) * 255 / 31 | 0,
                                      ((c >> 5) & 63) * 255 / 63 | 0,
                                      (c & 31) * 255 / 31 | 0])(pal[idx]);
          ctx.fillStyle = `rgb(${r},${g},${b})`;
          ctx.fillRect(x, FULL_Y + y, 1, 1);
        }
      }
    }

    hpBar(ctx, x, y, w, pct, color) {
      ctx.fillStyle = '#20242c';
      ctx.fillRect(x, y, w, 5);
      ctx.fillStyle = '#000';
      ctx.fillRect(x + 1, y + 1, w - 2, 3);
      const fw = Math.round((w - 2) * Math.max(0, Math.min(100, pct)) / 100);
      if (fw > 0) {
        ctx.fillStyle = color;
        ctx.fillRect(x + 1, y + 1, fw, 3);
      }
    }

    text(ctx, str, y, color) {
      this.r.textCentered(str, y, color);
    }
  }

  root.DigiBattle = BattleScene;
})(window);
