// 背景层：白天/黑夜场景 + 缓慢飘动的云（除云以外全部静止）
(function (root) {
  'use strict';

  const B36 = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ';

  function rgb565(c) {
    const r = ((c >> 11) & 0x1F) * 255 / 31 | 0;
    const g = ((c >> 5) & 0x3F) * 255 / 63 | 0;
    const b = (c & 0x1F) * 255 / 31 | 0;
    return `rgb(${r},${g},${b})`;
  }

  // 云只有两种尺寸，够用
  const CLOUD_W = [
    ['...1111...', '..111111..', '.11111111.'],
    ['....111111.....', '..1111111111...', '1111111111111.'],
  ];

  class Background {
    constructor(data) {
      this.data = data;
      this.scene = null;
      this.cache = {};
      this.last = 0;
      this.clouds = [
        { i: 1, x: 6, y: 3, speed: 2.2 },
        { i: 0, x: 34, y: 9, speed: 1.4 },
        { i: 1, x: 52, y: 15, speed: 1.0 },
      ];
    }

    // mode: 'auto' | 'day' | 'night'
    sceneFor(nowSec, mode) {
      if (mode === 'day' || mode === 'night') return mode;
      // 北京时间（UTC+8），6:00-18:00 为白天
      const h = (((nowSec % 86400) + 8 * 3600) % 86400) / 3600;
      return (h >= 6 && h < 18) ? 'day' : 'night';
    }

    // 预渲染成离屏画布，避免每帧重画 3584 个像素
    build(key) {
      if (this.cache[key]) return this.cache[key];
      const s = this.data[key];
      if (!s) return null;
      const off = document.createElement('canvas');
      off.width = s.w;
      off.height = s.h;
      const c = off.getContext('2d');
      for (let y = 0; y < s.h; y++) {
        const row = s.rows[y];
        for (let x = 0; x < s.w; x++) {
          const idx = B36.indexOf(row[x]);
          c.fillStyle = rgb565(s.pal[idx]);
          c.fillRect(x, y, 1, 1);
        }
      }
      this.cache[key] = off;
      return off;
    }

    draw(ctx, nowSec, mode) {
      const key = this.sceneFor(nowSec, mode);
      this.scene = key;
      const s = this.data[key];
      if (!s) return;

      // 顶部 8px 留给状态栏，背景从 y=8 开始
      if (typeof document !== 'undefined') {
        const off = this.build(key);
        ctx.drawImage(off, 0, 8);
      } else {
        for (let y = 0; y < s.h; y++) {
          const row = s.rows[y];
          for (let x = 0; x < s.w; x++) {
            const idx = B36.indexOf(row[x]);
            ctx.fillStyle = rgb565(s.pal[idx]);
            ctx.fillRect(x, 8 + y, 1, 1);
          }
        }
      }

      this.drawClouds(ctx, key);
    }

    // 只推进云的位置（其余画面全部静止）
    advance(dtSec) {
      for (const cl of this.clouds) {
        cl.x += cl.speed * dtSec;
        const w = CLOUD_W[cl.i][0].length;
        if (cl.x > 64 + w) cl.x = -w - Math.random() * 20;
      }
    }

    drawClouds(ctx, key) {
      const now = (typeof performance !== 'undefined') ? performance.now() : Date.now();
      const dt = this.last ? Math.min(0.1, (now - this.last) / 1000) : 0;
      this.last = now;
      this.advance(dt);

      const main = key === 'day' ? '#ffffff' : '#26385f';
      const shade = key === 'day' ? '#d6e6f8' : '#1c2b4a';

      for (const cl of this.clouds) {
        const w = CLOUD_W[cl.i][0].length;
        const ox = Math.round(cl.x);
        const oy = 8 + cl.y;
        const rows = CLOUD_W[cl.i];
        for (let yy = 0; yy < rows.length; yy++) {
          const row = rows[yy];
          for (let xx = 0; xx < row.length; xx++) {
            const ch = row[xx];
            if (ch === '.') continue;
            ctx.fillStyle = yy === rows.length - 1 ? shade : main;
            const px = ox + xx;
            if (px < 0 || px >= 64) continue;
            ctx.fillRect(px, oy + yy, 1, 1);
          }
        }
      }
    }
  }

  root.DigiBackground = Background;
})(window);
