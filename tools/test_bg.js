// 背景层验证 + 导出白天/黑夜（云移动前后）画面
const fs = require('fs');
const path = require('path');

global.window = {};
require(path.join(__dirname, '..', 'web', 'bg.js'));
require(path.join(__dirname, '..', 'web', 'render.js'));
const { makeFB } = require('./framebuf.js');

const raw = fs.readFileSync(path.join(__dirname, '..', 'web', 'bg_data.js'), 'utf8');
const win = {};
new Function('window', raw)(win);
const BG = win.DIGI_BG;

const fb = makeFB();
const bg = new window.DigiBackground(BG);

function shot(mode, nowSec) {
  fb.buf.fill(0);
  // 顶部状态栏条
  fb.ctx.fillStyle = '#000000';
  fb.ctx.fillRect(0, 0, 64, 8);
  bg.draw(fb.ctx, nowSec, mode);
  return Array.from(fb.buf);
}

const BASE = 1700000000 - (1700000000 % 86400);   // UTC 零点
const T_DAY = BASE + 4 * 3600;      // 北京时间 12:00
const T_NIGHT = BASE + 14 * 3600;   // 北京时间 22:00


const shots = [];
shots.push(shot('day', T_DAY));
bg.advance(10);
shots.push(shot('day', T_DAY));
shots.push(shot('night', T_NIGHT));
bg.advance(10);
shots.push(shot('night', T_NIGHT));

// 云确实动了：比较两张白天图上半部分
let diff = 0;
for (let y = 8; y < 30; y++) {
  for (let x = 0; x < 64; x++) {
    const i = (y * 64 + x) * 3;
    if (Math.abs(shots[0][i] - shots[1][i]) > 20) diff++;
  }
}
if (diff < 10) throw new Error('云没有移动: diff=' + diff);
console.log('云移动像素:', diff);

// 除云以外的区域必须完全静止（下半部分草地/屋顶）
let staticDiff = 0;
for (let y = 40; y < 64; y++) {
  for (let x = 0; x < 64; x++) {
    const i = (y * 64 + x) * 3;
    if (shots[0][i] !== shots[1][i] || shots[2][i] !== shots[3][i]) staticDiff++;
  }
}
if (staticDiff > 0) throw new Error('静态区域发生了移动: ' + staticDiff);
console.log('静态区域差异: 0 ✓');

fs.writeFileSync(path.join(__dirname, '..', 'docs', 'bg_frames.json'), JSON.stringify(shots));
console.log('已导出 docs/bg_frames.json');

// ---------- 完整画面（背景 + 状态栏 + 角色）----------
const rawData = fs.readFileSync(path.join(__dirname, '..', 'web', 'data.js'), 'utf8');
const DATA = JSON.parse(rawData.slice(rawData.indexOf('=') + 1).trim().replace(/;$/, ''));
const { Pet } = require(path.join(__dirname, '..', 'web', 'game.js'));

const R = new window.DigiRenderer({ getContext: () => fb.ctx }, DATA);
R.bg = bg;
R.sceneMode = 'auto';

const pet = new Pet(T_DAY);
pet.stage = 2; pet.species = 2; pet.alive = 1;
pet.hunger = 3; pet.mood = 78; pet.hp = 92;
pet.stageStart = T_DAY; pet.evolveAt = T_DAY + 3 * 86400;

function full(nowSec, label) {
  fb.buf.fill(0);
  R.draw(pet, 0, nowSec);
  return { label, px: Array.from(fb.buf) };
}

const scenes = [];
R.nowSource = () => T_DAY;
scenes.push(full(T_DAY, '白天 12:00'));
bg.advance(12);
scenes.push(full(T_DAY, '白天（12 秒后）'));
R.nowSource = () => T_NIGHT;
scenes.push(full(T_NIGHT, '黑夜 22:00'));
bg.advance(12);
scenes.push(full(T_NIGHT, '黑夜（12 秒后）'));

fs.writeFileSync(path.join(__dirname, '..', 'docs', 'scene_frames.json'), JSON.stringify(scenes));
console.log('已导出 docs/scene_frames.json');
