import io

p = r"C:\Users\Administrator\deepseekwork\esp32-digimon\app\index.html"
s = io.open(p, encoding="utf-8").read()

if "previewCanvas" not in s:
    anchor = '<div class="sec-t">进化线 / 重置</div>'
    block = (
        '<div class="sec-t">屏幕预览（和面板同步）</div>\n'
        '<div class="card" style="text-align:center">\n'
        '  <canvas id="previewCanvas" width="256" height="256"'
        ' style="width:192px;height:192px;image-rendering:pixelated;border-radius:12px;background:#0b0f14"></canvas>\n'
        '  <div class="row" style="margin-top:8px"><span class="muted" id="pvLabel">—</span></div>\n'
        '</div>\n\n' + anchor
    )
    s = s.replace(anchor, block, 1)
    s = s.replace('<script>', '<script src="vb_data.js"></script>\n<script>', 1)

if "function drawPreview" not in s:
    js = r"""
// ---------------- 屏幕预览 ----------------
const PV = { key: null, t0: 0, frame: 0 };
function pvPickChar(name) {
  const db = window.DIGI_VB || {};
  const q = (name || '').toUpperCase().replace(/[^A-Z]/g, '');
  let best = null;
  for (const k in db) {
    const n = (db[k].name || '').toUpperCase().replace(/[^A-Z]/g, '');
    if (!n) continue;
    if (n === q) return k;
    if (!best && q && (n.includes(q) || q.includes(n))) best = k;
  }
  return best;
}
function drawPreview(state) {
  const c = document.getElementById('previewCanvas'); if (!c) return;
  const g = c.getContext('2d');
  const night = state && state.scene === 1;
  g.fillStyle = night ? '#0a1030' : '#5fb6f0';
  g.fillRect(0, 0, 64, 64);
  g.fillStyle = night ? '#1b2a6b' : '#3f8fd0';
  g.fillRect(0, 40, 64, 24);

  const key = pvPickChar(state && state.species);
  if (key && window.DIGI_VB && DIGI_VB[key]) {
    const ch = DIGI_VB[key];
    const frames = ch.anims.idle.frames;
    const now = Date.now();
    if (PV.key !== key) { PV.key = key; PV.t0 = now; PV.frame = 0; }
    PV.frame = Math.floor((now - PV.t0) / 400) % frames.length;
    const rows = frames[PV.frame] || [];
    const h = rows.length, w = (rows[0] || '').length;
    const ox = Math.floor((64 - w) / 2), oy = 64 - h - 4;
    for (let y = 0; y < h; y++) {
      const line = rows[y];
      for (let x = 0; x < w; x++) {
        const ch2 = line[x];
        if (ch2 === '0' || ch2 === undefined) continue;
        const idx = parseInt(ch2, 16);
        const col = ch.pal[idx];
        g.fillStyle = 'rgb(' + ((col >> 11) * 8) + ',' + (((col >> 5) & 63) * 4) + ',' + ((col & 31) * 8) + ')';
        g.fillRect(ox + x, oy + y, 1, 1);
      }
    }
    const lb = document.getElementById('pvLabel');
    if (lb) lb.textContent = (state.stageName || '') + ' · ' + ch.name;
  }
  // 顶部/底部 HUD
  g.fillStyle = '#2a2f45'; g.fillRect(0, 0, 64, 7); g.fillRect(0, 56, 64, 8);
  g.fillStyle = '#ffd166'; g.font = '5px monospace';
  if (state) g.fillText((state.hunger||0) + ' ♥' + (state.mood||0), 2, 5);
}
setInterval(() => { if (S) drawPreview(S); }, 120);
"""
    s = s.replace("async function pick(n){", js + "\nasync function pick(n){", 1)

io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("preview added:", "previewCanvas" in s, "drawPreview" in s)
