// Node 对拍：用网页里的 game.js 跑 500 次养成，和 Python 模拟器比较
const path = require('path');
const { Pet, ST } = require(path.join(__dirname, '..', 'web', 'game.js'));

function runOnce() {
  let now = 1700000000;
  const pet = new Pet(now);
  let battlesToday = 0;
  let dayMark = now;
  let lastCare = now;

  for (let i = 0; i < 45 * 86400; i += 60) {
    now += 60;

    if (now - lastCare >= 7200) {
      lastCare = now;
      if (now - dayMark >= 86400) { dayMark = now; battlesToday = 0; }
      while (pet.hunger < 3) pet.act('feed', now);
      if (pet.poops > 0) pet.act('clean', now);
      if (pet.pending === 2) pet.act('pet', now);
      if (pet.stage === ST.ADULT && !(pet.items & 1)) pet.card('courage', now);
      if (pet.hp > 35 && pet.stage >= ST.CHILD && battlesToday < 2 && pet.hunger > 0) {
        battlesToday++;
        pet.act('battle', now);
      }
    }

    pet.tick(now);
    if (pet.stage === ST.PERFECT) return { done: true, days: (now - pet.birth) / 86400, stage: 5 };
    if (!pet.alive) return { done: false, dead: true, days: (now - pet.birth) / 86400, stage: pet.stage };
  }
  return { done: false, days: 45, stage: pet.stage };
}

const runs = [];
for (let i = 0; i < 500; i++) runs.push(runOnce());

const done = runs.filter(r => r.done);
const dead = runs.filter(r => r.dead);
console.log('样本', runs.length);
console.log('到达究极体', (done.length / runs.length * 100).toFixed(1) + '%');
console.log('死亡', (dead.length / runs.length * 100).toFixed(1) + '%');
if (done.length) {
  const d = done.map(r => r.days).sort((a, b) => a - b);
  const med = d[Math.floor(d.length / 2)];
  console.log(`到达天数 min=${d[0].toFixed(1)} 中位=${med.toFixed(1)} max=${d[d.length - 1].toFixed(1)}`);
}
