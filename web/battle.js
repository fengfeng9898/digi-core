// 战斗场景：敌方登场 -> 我方攻击命中 -> 胜利 / 敌方反击 -> 我方受伤
// 关键点：攻击动画必须有交互对象，胜利/受伤是对战结果，不是随机播放。
(function (root) {
  'use strict';

  function rgb565(c) {
    const r = ((c >> 11) & 0x1F) * 255 / 31 | 0;
    const g = ((c >> 5) & 0x3F) * 255 / 63 | 0;
    const b = (c & 0x1F) * 255 / 31 | 0;
    return [r, g, b];
  }

  function bbox(frames) {
    let x0 = 1e9, y0 = 1e9, x1 = -1, y1 = -1;
    for (const rows of frames) {
      for (let y = 0; y < rows.length; y++) {
        const row = rows[y];
        for (let x = 0; x < row.length; x++) {
          if (row[x] !== '0') {
            if (x < x0) x0 = x;
            if (y < y0) y0 = y;
            if (x > x1) x1 = x;
            if (y > y1) y1 = y;
          }
        }
      }
    }
    if (x1 < 0) return { x: 0, y: 0, w: 0, h: 0 };
    return { x: x0, y: y0, w: x1 - x0 + 1, h: y1 - y0 + 1 };
  }

  class BattleScene {
    constructor(data, renderer) {
      this.data = data;
      this.r = renderer;
      this.active = false;
      this.t0 = 0;
      this.phases = [];
      this.pi = 0;
      this.bboxCache = {};
    }

    charBox(key) {
      if (!this.bboxCache[key]) {
        const c = this.data.chars[key];
        this.bboxCache[key] = bbox(c.anims.idle.frames);
      }
      return this.bboxCache[key];
    }

    enemyName(key) {
      return { koromon: 'KOROMON', agumon: 'AGUMON', greymon: 'GREYMON' }[key] || 'WILD';
    }

    // opts: { win, myChar, enemyChar, hpBefore, hpAfter }
    start(opts, nowMs) {
      const clipDur = (clip) => clip.dur.reduce((a, b) => a + b, 0);
      const c = this.data.chars[opts.myChar];

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

      const atkDur = clipDur(c.anims.attack);
      const winDur = clipDur(c.anims.win);
      const hurtDur = clipDur(c.anims.hurt);

      this.phases = [
        { name: 'intro', dur: 650 },
        { name: 'attack', dur: atkDur },
      ];
      if (opts.win) {
        this.phases.push({ name: 'down', dur: 420 });
        this.phases.push({ name: 'win', dur: winDur });
        this.phases.push({ name: 'end', dur: 450, last: true });
      } else {
        this.phases.push({ name: 'counter', dur: 380 });
        this.phases.push({ name: 'hurt', dur: hurtDur });
        this.phases.push({ name: 'end', dur: 500, last: true });
      }
    }

    // 取动画在相对时间 t 处的帧
    frameAt(clip, t) {
      let acc = 0;
      for (let i = 0; i < clip.frames.length; i++) {
        acc += clip.dur[i] || 100;
        if (t < acc) return clip.frames[i];
      }
      return clip.frames[clip.frames.length - 1];
    }

    // 按包围盒等比缩放绘制；mirror 用于敌方朝向
    blit(ctx, rows, pal, box, destCx, destBaseY, scale, mirror, tint, flash) {
      if (!box.w || !box.h) return;
      const dw = Math.max(1, Math.round(box.w * scale));
      const dh = Math.max(1, Math.round(box.h * scale));
      const ox = Math.round(destCx - dw / 2);
      const oy = Math.round(destBaseY - dh);

      for (let dy = 0; dy < dh; dy++) {
        const sy = box.y + Math.min(box.h - 1, Math.floor(dy / scale));
        const row = rows[sy];
        if (!row) continue;
        for (let dx = 0; dx < dw; dx++) {
          const sxLocal = Math.min(box.w - 1, Math.floor(dx / scale));
          const sx = box.x + (mirror ? (box.w - 1 - sxLocal) : sxLocal);
          const idx = parseInt(row[sx], 16);
          if (!idx) continue;
          let [r, g, b] = rgb565(pal[idx]);
          if (tint) { r = r * tint[0] | 0; g = g * tint[1] | 0; b = b * tint[2] | 0; }
          if (flash) {
            r = Math.min(255, r + (255 - r) * flash);
            g = Math.min(255, g + (255 - g) * flash);
            b = Math.min(255, b + (255 - b) * flash);
          }
          ctx.fillStyle = `rgb(${r | 0},${g | 0},${b | 0})`;
          ctx.fillRect(ox + dx, oy + dy, 1, 1);
        }
      }
      return { x: ox, y: oy, w: dw, h: dh };
    }

    draw(ctx, nowMs) {
      if (!this.active) return;
      const el = nowMs - this.t0;

      // 阶段推进
      while (this.pi < this.phases.length) {
        const p = this.phases[this.pi];
        const start = this.phaseStart || 0;
        if (el - start < p.dur) break;
        this.phaseStart = start + p.dur;
        this.pi++;
      }
      if (this.pi >= this.phases.length) { this.active = false; return; }

      const phase = this.phases[this.pi];
      const t = el - (this.phaseStart || 0);
      const me = this.data.chars[this.myChar];
      const en = this.data.chars[this.enemyChar];
      const myBox = this.charBox(this.myChar);
      const enBox = this.charBox(this.enemyChar);

      ctx.fillStyle = '#000';
      ctx.fillRect(0, 0, 64, 64);

      // ---- 血条 ----
      const myHp = (phase.name === 'hurt' || phase.name === 'end') ? this.hpAfter : this.hpBefore;
      let enemyHp = 100;
      if (phase.name === 'attack') enemyHp = t > phase.dur * 0.62 ? (this.win ? 0 : 18) : 100;
      if (phase.name === 'down' || phase.name === 'win' || (phase.name === 'end' && this.win)) enemyHp = 0;
      if (phase.name === 'counter' || phase.name === 'hurt' || (phase.name === 'end' && !this.win)) {
        enemyHp = 18;
      }

      this.hpBar(ctx, 1, 1, 30, myHp, '#3fb950');
      this.hpBar(ctx, 33, 1, 30, enemyHp, '#f85149');

      // ---- 我方 ----
      let myRows = me.anims.idle.frames[0];
      if (phase.name === 'attack') myRows = this.frameAt(me.anims.attack, t);
      else if (phase.name === 'win') myRows = this.frameAt(me.anims.win, t);
      else if (phase.name === 'hurt') myRows = this.frameAt(me.anims.hurt, t);
      else myRows = this.frameAt(me.anims.idle, t);

      // 失败时最后一击要闪红
      const myFlash = (phase.name === 'counter' && t > phase.dur * 0.6) ? 0.75 : 0;
      const myTint = myFlash ? null : null;
      this.blit(ctx, myRows, me.pal, myBox, 22, 52, 0.80, false, myTint, myFlash);

      // ---- 敌方 ----
      let enRows = this.frameAt(en.anims.idle, t + 200);
      // 进场：从右侧滑入
      let slide = 0;
      if (phase.name === 'intro') {
        const k = Math.min(1, t / phase.dur);
        slide = Math.round((1 - k) * 46);
      }
      const enFlash = (phase.name === 'attack' && t > phase.dur * 0.62) ? 0.85 : 0;
      const enFade = (phase.name === 'down') ? Math.max(0, 1 - t / phase.dur) : 1;
      const lunge = (phase.name === 'counter' && t < phase.dur * 0.55) ? -7 : 0;
      const enTint = [0.62, 0.42, 0.55];
      const hideEnemy = (this.win && (phase.name === 'end'));
      if (!hideEnemy) this.blit(ctx, enRows, en.pal, enBox, 48 + slide + lunge, 53, 0.68, true,
        enFade >= 1 ? enTint : [enTint[0] * enFade, enTint[1] * enFade, enTint[2] * enFade], enFlash);

      // ---- 文字 ----
      const msg = {
        intro: this.enemyName(this.enemyChar) + '!',
        attack: this.win ? 'GO!' : 'GO!',
        down: 'KO!',
        win: 'YOU WIN!',
        counter: 'ENEMY!',
        hurt: 'YOU LOSE',
        end: this.win ? 'WINNER!' : 'DEFEAT..'
      }[phase.name] || '';
      this.text(ctx, msg, 57, '#fff');
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
