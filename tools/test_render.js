// 无头冒烟测试：用假 canvas 跑 render.js，确认能画、能播动画
const fs = require('fs');
const path = require('path');

global.window = {};
require(path.join(__dirname, '..', 'web', 'render.js'));

const raw = fs.readFileSync(path.join(__dirname, '..', 'web', 'data.js'), 'utf8');
const json = raw.slice(raw.indexOf('=') + 1).trim().replace(/;$/, '');
const DATA = JSON.parse(json);

let pixels = 0;
const ctx = {
  fillStyle: '',
  font: '',
  imageSmoothingEnabled: true,
  fillRect(x, y, w, h) {
    if (x < 0 || y < 0 || x + w > 64 || y + h > 64) throw new Error(`越界绘制 ${x},${y},${w},${h}`);
    pixels += w * h;
  },
  fillText() {},
};
const canvas = { getContext: () => ctx };

const R = new window.DigiRenderer(canvas, DATA);
const { Pet, ST } = require(path.join(__dirname, '..', 'web', 'game.js'));

const pet = new Pet(1700000000);
let now = 1700000000;

function frame(t, label) {
  pixels = 0;
  R.draw(pet, t, now);
  if (pixels < 500) throw new Error(`${label}: 画面几乎空白 (${pixels}px)`);
  return pixels;
}

console.log('初始帧像素:', frame(0, 'egg'));

// 快进到亚古兽
pet.act('evolve', now);
now += 100;
pet.stage = ST.CHILD; pet.species = 2; pet.alive = 1;   // 直接摆到成长期
frame(0, 'agumon');
console.log('亚古兽帧像素 OK');

// 动画推进：待机循环若干帧
const f0 = R.anim.frame;
for (let i = 1; i <= 10; i++) frame(i * 150, 'idle' + i);
const f1 = R.anim.frame;
if (f0 === f1 && R.anim.clip.frames.length > 1) throw new Error('待机动画没有推进');
console.log(`待机动画推进: ${f0} -> ${f1}，共 ${R.anim.clip.frames.length} 帧`);

// 战斗序列：攻击 -> 胜利 -> 待机
R.playBattle(null, true);
const seen = new Set();
for (let i = 1; i <= 80; i++) {
  frame(i * 60, 'battle' + i);
  seen.add(R.anim.name + ':' + R.anim.frame);
}
const names = new Set([...seen].map((s) => s.split(':')[0]));
if (!names.has('attack') || !names.has('win')) throw new Error('战斗序列没播完: ' + [...names]);
console.log('战斗序列播放:', [...names].join(' -> '));

// 程序化兜底（机械暴龙兽无动画）
pet.stage = ST.ADULT; pet.species = 4;
pixels = 0;
R.draw(pet, 0, now);
if (pixels < 500) throw new Error('静态兜底空白');
console.log('静态兜底像素:', pixels, 'OK');

console.log('\n全部通过');
