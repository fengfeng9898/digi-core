// 帧缓冲 ctx：把绘制结果存成 64x64 RGB，用于导出逐帧画面。
// 支持 #rgb / #rrggbb / rgb() / rgba()（rgba 做简单 alpha 混合）。
function makeFB() {
  const buf = new Uint8Array(64 * 64 * 3);
  const ctx = {
    fillStyle: '#000000',
    _color() {
      const s = this.fillStyle;
      if (s === 'black') return [0, 0, 0, 1];
      if (s === 'white') return [255, 255, 255, 1];
      let m = /^#([0-9a-f])([0-9a-f])([0-9a-f])$/i.exec(s);
      if (m) return [parseInt(m[1] + m[1], 16), parseInt(m[2] + m[2], 16), parseInt(m[3] + m[3], 16), 1];
      m = /^#([0-9a-f]{6})$/i.exec(s);
      if (m) {
        const n = parseInt(m[1], 16);
        return [(n >> 16) & 255, (n >> 8) & 255, n & 255, 1];
      }
      m = /^rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([\d.]+)\s*)?\)$/.exec(s);
      if (m) return [+m[1], +m[2], +m[3], m[4] === undefined ? 1 : +m[4]];
      return [255, 0, 255, 1];
    },
    fillRect(x, y, w, h) {
      const c = this._color();
      const [r, g, b, a] = c;
      for (let yy = y; yy < y + h; yy++) {
        for (let xx = x; xx < x + w; xx++) {
          if (xx < 0 || yy < 0 || xx >= 64 || yy >= 64) continue;
          const i = (yy * 64 + xx) * 3;
          buf[i] = Math.round(buf[i] * (1 - a) + r * a);
          buf[i + 1] = Math.round(buf[i + 1] * (1 - a) + g * a);
          buf[i + 2] = Math.round(buf[i + 2] * (1 - a) + b * a);
        }
      }
    },
  };
  return { ctx, buf };
}

module.exports = { makeFB };
