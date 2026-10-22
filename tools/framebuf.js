// 帧缓冲 ctx：把绘制结果存成 64x64 RGB，用于导出战斗逐帧画面
function makeFB() {
  const buf = new Uint8Array(64 * 64 * 3);
  const ctx = {
    fillStyle: '#000000',
    _color() {
      const s = this.fillStyle;
      if (s === 'black') return [0, 0, 0];
      if (s === 'white') return [255, 255, 255];
      let m = /^#([0-9a-f])([0-9a-f])([0-9a-f])$/i.exec(s);
      if (m) return [parseInt(m[1] + m[1], 16), parseInt(m[2] + m[2], 16), parseInt(m[3] + m[3], 16)];
      m = /^#([0-9a-f]{6})$/i.exec(s);
      if (m) {
        const n = parseInt(m[1], 16);
        return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
      }
      m = /^rgb\((\d+),\s*(\d+),\s*(\d+)\)$/.exec(s);
      if (m) return [+m[1], +m[2], +m[3]];
      return [255, 0, 255];
    },
    fillRect(x, y, w, h) {
      const [r, g, b] = this._color();
      for (let yy = y; yy < y + h; yy++) {
        for (let xx = x; xx < x + w; xx++) {
          if (xx < 0 || yy < 0 || xx >= 64 || yy >= 64) continue;
          const i = (yy * 64 + xx) * 3;
          buf[i] = r; buf[i + 1] = g; buf[i + 2] = b;
        }
      }
    },
  };
  return { ctx, buf };
}

module.exports = { makeFB };
