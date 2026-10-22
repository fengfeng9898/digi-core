// 战斗演出验证 + 导出逐帧画面
const fs = require('fs');
const path = require('path');

global.window = {};
require(path.join(__dirname, '..', 'web', 'render.js'));
require(path.join(__dirname, '..', 'web', 'battle.js'));
const { makeFB } = require('./framebuf.js');

const raw = fs.readFileSync(path.join(__dirname, '..', 'web', 'data.js'), 'utf8');
const DATA = JSON.parse(raw.slice(raw.indexOf('=') + 1).trim().replace(/;$/, ''));

const fb = makeFB();
require(path.join(__dirname, '..', 'web', 'bg.js'));
const rawBg = fs.readFileSync(path.join(__dirname, '..', 'web', 'bg_data.js'), 'utf8');
const winObj = {};
new Function('window', rawBg)(winObj);

const R = new window.DigiRenderer({ getContext: () => fb.ctx }, DATA);
const bg = new window.DigiBackground(winObj.DIGI_BG);
R.bg = bg;
R.sceneMode = 'day';
const BASE = 1700000000 - (1700000000 % 86400);
R.nowSec = BASE + 4 * 3600;
const B = new window.DigiBattle(DATA, R);

function run(name, opts) {
  B.start(opts, 0);
  const total = B.phases.reduce((a, p) => a + p.dur, 0);
  const shots = [];
  const seen = new Set();
  for (let t = 0; t <= total + 200; t += 50) {
    fb.buf.fill(0);
    B.draw(fb.ctx, t);
    seen.add(B.phases[Math.min(B.pi, B.phases.length - 1)].name);
  }
  if (B.active) throw new Error(name + ': 演出没有结束');
  console.log(`${name}: 阶段 ${[...seen].join(' -> ')}  时长 ${total}ms`);

  // 每个镜头取一帧（回合制切镜，按阶段取样最能说明问题）
  B.start(opts, 0);
  let acc = 0;
  for (const p of B.phases) {
    const mid = acc + p.dur * 0.5;
    fb.buf.fill(0);
    B.draw(fb.ctx, mid);
    shots.push(Array.from(fb.buf));
    acc += p.dur;
  }
  return shots;
}

const winShots = run('胜', { win: true, myChar: 'agumon', enemyChar: 'greymon', hpBefore: 90, hpAfter: 72 });
const loseShots = run('负', { win: false, myChar: 'agumon', enemyChar: 'greymon', hpBefore: 90, hpAfter: 40 });

// 检查敌方是否真的出现在右半边（在入场帧上统计）
const rightPixels = (() => {
  B.start({ win: true, myChar: 'agumon', enemyChar: 'greymon', hpBefore: 90, hpAfter: 72 }, 0);
  fb.buf.fill(0);
  B.draw(fb.ctx, 600);
  let n = 0;
  for (let y = 20; y < 60; y++) for (let x = 32; x < 64; x++) {
    const i = (y * 64 + x) * 3;
    if (fb.buf[i] + fb.buf[i + 1] + fb.buf[i + 2] > 30) n++;
  }
  return n;
})();
if (rightPixels < 40) throw new Error('敌方没有出现在右半屏: ' + rightPixels);
console.log(`敌方入场像素: ${rightPixels}`);

fs.writeFileSync(path.join(__dirname, '..', 'docs', 'battle_frames.json'),
  JSON.stringify({ win: winShots, lose: loseShots }));
console.log('已导出 docs/battle_frames.json');
