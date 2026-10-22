// 64x64 屏幕渲染（和固件 ui.cpp 布局一致）
(function (root) {
  'use strict';

  const PALETTE = { red: '#f80000', amber: '#fdaf00', cyan: '#07ffff', gray: '#7b7b7b', dim: '#3a3a3a' };

  function rgb565(c) {
    const r = ((c >> 11) & 0x1F) * 255 / 31 | 0;
    const g = ((c >> 5) & 0x3F) * 255 / 63 | 0;
    const b = (c & 0x1F) * 255 / 31 | 0;
    return `rgb(${r},${g},${b})`;
  }

  const HEART = [0b01010, 0b11111, 0b11111, 0b01110, 0b00100];

  class Renderer {
    constructor(canvas, data) {
      this.cv = canvas;
      this.ctx = canvas.getContext('2d');
      this.ctx.imageSmoothingEnabled = false;
      this.data = data;
      this.font = data.font && data.font.length ? data.font : null;
      this.byName = {};
      data.statics.forEach((s) => { this.byName[s.name] = s; });
      this.anim = { key: null, name: null, clip: null, frame: 0, start: 0, loop: true, battle: false, result: null };
    }

    // ---------- 基础绘制 ----------
    px(x, y, color) {
      if (x < 0 || y < 0 || x >= 64 || y >= 64) return;
      this.ctx.fillStyle = color;
      this.ctx.fillRect(x, y, 1, 1);
    }

    rect(x, y, w, h, color) {
      this.ctx.fillStyle = color;
      this.ctx.fillRect(x, y, w, h);
    }

    // 5x7 字库（与真机 glcdfont 相同），带 1px 黑色描边，压在背景上也看得清
    text(str, x, y, color) {
      if (this.font) {
        const put = (dx, dy, col) => {
          this.ctx.fillStyle = col;
          for (let i = 0; i < str.length; i++) {
            let code = str.charCodeAt(i);
            if (code < 32 || code > 126) code = 63;
            const g = (code - 32) * 5;
            for (let ci = 0; ci < 5; ci++) {
              const bits = this.font[g + ci] || 0;
              for (let row = 0; row < 8; row++) {
                if (bits & (1 << row)) {
                  this.ctx.fillRect(x + i * 6 + ci + dx, y + row + dy, 1, 1);
                }
              }
            }
          }
        };
        put(1, 1, '#000');
        put(0, 0, color);
        return;
      }
      // 没有字库时退化为浏览器字体
      this.ctx.fillStyle = color;
      this.ctx.font = '7px monospace';
      this.ctx.fillText(str, x, y + 7);
    }

    textCentered(str, y, color) {
      if (str.length > 10) str = str.slice(0, 10);
      const x = Math.max(0, Math.floor((64 - str.length * 6) / 2));
      this.text(str, x, y, color);
    }

    sprite(s, ox, oy, dim) {
      const pal = s.pal.map((c) => (dim >= 255 ? rgb565(c) : this.dimColor(c, dim)));
      for (let y = 0; y < s.h; y++) {
        const row = s.rows[y];
        for (let x = 0; x < s.w; x++) {
          const idx = parseInt(row[x], 16);
          if (!idx) continue;
          this.ctx.fillStyle = pal[idx];
          this.ctx.fillRect(ox + x, oy + y, 1, 1);
        }
      }
    }

    dimColor(c, dim) {
      const r = (((c >> 11) & 0x1F) * dim) >> 8;
      const g = (((c >> 5) & 0x3F) * dim) >> 8;
      const b = ((c & 0x1F) * dim) >> 8;
      return rgb565((r << 11) | (g << 5) | b);
    }

    // ---------- 动画 ----------
    charForSpecies(sp) {
      switch (sp) {
        case 1: return 'koromon';
        case 2: return 'agumon';
        case 3: return 'greymon';
        default: return null;
      }
    }

    setChar(key) {
      if (this.anim.key === key) return;
      this.anim.key = key;
      if (key) this.play('idle', true);
    }

    play(name, loop) {
      const c = this.data.chars[this.anim.key];
      if (!c || !c.anims[name]) { this.anim.clip = null; return; }
      this.anim.name = name;
      this.anim.clip = c.anims[name];
      this.anim.frame = 0;
      this.anim.start = performance.now();
      this.anim.loop = loop;
      this.anim.battle = false;
      this.anim.result = null;
    }

    playBattle(viaChar, win) {
      const key = viaChar || this.anim.key;
      if (!key || !this.data.chars[key]) return;
      this.anim.key = key;
      this.play('attack', false);
      this.anim.battle = true;
      this.anim.result = win ? 'win' : 'hurt';
    }

    update(ms) {
      const a = this.anim;
      if (!a.clip) return;
      const dur = a.clip.dur[a.frame] || 100;
      if (ms - a.start < dur) return;
      a.start = ms;
      a.frame++;
      if (a.frame < a.clip.frames.length) return;

      if (a.loop) { a.frame = 0; return; }
      if (a.battle) {
        a.battle = false;
        this.play(a.result, false);
        return;
      }
      a.frame = a.clip.frames.length - 1;
      this.play('idle', true);
    }

    // ---------- 整屏 ----------
    draw(pet, nowMs, nowSec) {
      const snap = pet.snapshot(nowSec);
      // 背景与时钟都跟真实时间走（北京时间），不吃游戏内加速
      const realSec = this.nowSource ? this.nowSource() : Date.now() / 1000;

      // 背景（白天/黑夜 + 飘动的云）；顶部 8px 保持黑色给状态栏
      this.ctx.fillStyle = '#000';
      this.ctx.fillRect(0, 0, 64, 8);
      if (this.bg) {
        this.bg.draw(this.ctx, realSec, this.sceneMode || 'auto');
      } else {
        this.ctx.fillRect(0, 8, 64, 56);
      }

      // 状态栏
      for (let i = 0; i < 4; i++) {
        const filled = i < snap.hunger;
        for (let row = 0; row < 5; row++) {
          for (let col = 0; col < 5; col++) {
            if (HEART[row] & (0x10 >> col)) this.px(1 + i * 6 + col, 1 + row, filled ? '#f80000' : '#600000');
          }
        }
      }
      // 心情条
      this.ctx.fillStyle = '#3a3a3a';
      this.ctx.fillRect(28, 1, 35, 5);
      this.ctx.fillStyle = '#000';
      this.ctx.fillRect(29, 2, 33, 3);
      const mw = Math.round(33 * Math.max(0, Math.min(100, snap.mood)) / 100);
      if (mw > 0) {
        this.ctx.fillStyle = snap.mood > 60 ? '#3fb950' : (snap.mood > 30 ? '#d29922' : '#f85149');
        this.ctx.fillRect(29, 2, mw, 3);
      }

      // 待响应事件
      if (snap.pending && ((nowMs / 400) & 1)) this.text('!', 0, 8, '#fdaf00');

      // 主体
      const dim = snap.sleeping ? 96 : (snap.alive ? 255 : 150);
      const chKey = this.charForSpecies(snap.species);
      if (chKey && this.data.chars[chKey]) {
        this.setChar(chKey);
        this.update(nowMs);
        const c = this.data.chars[chKey];
        const clip = this.anim.clip;
        if (clip) this.spriteFrame(clip.frames[this.anim.frame], c.pal, dim);
      } else {
        this.anim.key = null; this.anim.clip = null;
        const name = snap.alive ? (snap.stage === 0 ? 'EGG' : snap.speciesEn) : 'DEAD';
        const s = this.byName[name] || this.byName['EGG'];
        if (s) this.sprite(s, 32 - (s.w >> 1), 34 - (s.h >> 1), dim);
      }

      // 便便
      for (let i = 0; i < snap.poops && i < 3; i++) this.rect(56 + i * 3, 58, 2, 2, '#820000');

      // 底部：左边真实北京时间，右边阶段
      this.text(this.clockStr(realSec), 1, 56, '#00e5ff');

      let msg;
      if (snap.sleeping) msg = 'ZZZ';
      else if (!snap.alive) msg = 'RIP';
      else if (snap.stage === 0) msg = 'EGG';
      else msg = this.shortStage(snap.stageEn);
      const col = snap.alive ? '#ffffff' : '#7b7b7b';
      this.text(msg, 63 - msg.length * 6 + 1, 56, col);
    }

    // 北京时间 HH:MM
    clockStr(sec) {
      const t = (((sec % 86400) + 86400 + 8 * 3600) % 86400);
      const h = Math.floor(t / 3600), m = Math.floor((t % 3600) / 60);
      return String(h).padStart(2, '0') + ':' + String(m).padStart(2, '0');
    }

    shortStage(name) {
      return { ULTIMATE: 'ULT', PERFECT: 'PERF' }[name] || name;
    }

    spriteFrame(rows, pal, dim) {
      const colors = pal.map((c) => (dim >= 255 ? rgb565(c) : this.dimColor(c, dim)));
      // 64x40 画布，左上角 (0,9)
      for (let y = 0; y < rows.length; y++) {
        const row = rows[y];
        for (let x = 0; x < row.length; x++) {
          const idx = parseInt(row[x], 16);
          if (!idx) continue;
          this.ctx.fillStyle = colors[idx];
          this.ctx.fillRect(x, 9 + y, 1, 1);
        }
      }
    }

    // 满屏单角色绘制（战斗切镜用）：tint 压暗敌方，flash 受击闪白
    drawFrame(rows, pal, y0, tint, flash) {
      const cols = new Array(16);
      for (let i = 0; i < 16; i++) {
        let r = ((pal[i] >> 11) & 0x1F) * 255 / 31 | 0;
        let g = ((pal[i] >> 5) & 0x3F) * 255 / 63 | 0;
        let b = (pal[i] & 0x1F) * 255 / 31 | 0;
        if (tint) { r = r * tint[0] | 0; g = g * tint[1] | 0; b = b * tint[2] | 0; }
        if (flash) {
          r = Math.min(255, r + (255 - r) * flash) | 0;
          g = Math.min(255, g + (255 - g) * flash) | 0;
          b = Math.min(255, b + (255 - b) * flash) | 0;
        }
        cols[i] = `rgb(${r},${g},${b})`;
      }
      for (let y = 0; y < rows.length; y++) {
        const row = rows[y];
        for (let x = 0; x < row.length; x++) {
          const idx = parseInt(row[x], 16);
          if (!idx) continue;
          this.ctx.fillStyle = cols[idx];
          this.ctx.fillRect(x, y0 + y, 1, 1);
        }
      }
    }
  }

  root.DigiRenderer = Renderer;
})(window);
