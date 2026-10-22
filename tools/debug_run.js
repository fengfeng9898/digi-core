const path = require('path');
const { Pet, ST } = require(path.join(__dirname, '..', 'web', 'game.js'));

let now = 1700000000;
const pet = new Pet(now);
let lastStage = pet.stage;
let lastCare = now;
let battlesToday = 0, dayMark = now;

for (let i = 0; i < 20 * 86400; i += 60) {
  now += 60;
  if (now - lastCare >= 7200) {
    lastCare = now;
    if (now - dayMark >= 86400) { dayMark = now; battlesToday = 0; }
    while (pet.hunger < 3) pet.act('feed', now);
    if (pet.poops > 0) pet.act('clean', now);
    if (pet.hp > 35 && pet.stage >= ST.CHILD && battlesToday < 2) {
      battlesToday++;
      pet.act('battle', now);
    }
  }
  pet.tick(now);
  if (pet.stage !== lastStage) {
    console.log(`day ${((now - pet.birth) / 86400).toFixed(2)}  stage ${lastStage} -> ${pet.stage}  wins=${pet.wins} hp=${pet.hp} hunger=${pet.hunger}`);
    lastStage = pet.stage;
  }
  if (!pet.alive) { console.log('DEAD day', ((now - pet.birth) / 86400).toFixed(2)); break; }
}
console.log('final', pet.stage, 'wins', pet.wins, 'hp', pet.hp, 'hunger', pet.hunger,
  'mood', pet.mood, 'evolveIn', ((pet.evolveAt - now) / 86400).toFixed(2), 'days');
