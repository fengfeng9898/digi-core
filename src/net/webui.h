#pragma once
#include <Arduino.h>

// 主控制页（连上家里 WiFi 后打开 http://<device-ip>/ ）
static const char INDEX_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-CN"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>DIGI-CORE</title>
<style>
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;background:#0b0f14;color:#e6edf3;font:15px/1.5 -apple-system,"PingFang SC","Microsoft YaHei",sans-serif;padding:14px 14px 28px}
h1{font-size:17px;margin:0;letter-spacing:.14em;color:#7ee787}
.hd{display:flex;align-items:center;justify-content:space-between;margin-bottom:14px}
.dot{width:8px;height:8px;border-radius:50%;background:#3fb950;box-shadow:0 0 8px #3fb950;display:inline-block;margin-right:6px;vertical-align:middle}
.muted{color:#8b949e;font-size:12px}
.card{background:#151b23;border:1px solid #21262d;border-radius:14px;padding:14px;margin-bottom:12px}
.row{display:flex;align-items:baseline;justify-content:space-between;margin:6px 0}
.name{font-size:20px;font-weight:700;letter-spacing:.06em}
.badge{font-size:11px;padding:2px 8px;border-radius:99px;background:#1f6feb22;color:#79c0ff;border:1px solid #1f6feb55}
.bar{height:8px;background:#0d1117;border-radius:99px;overflow:hidden;margin:4px 0 10px}
.bar>i{display:block;height:100%;border-radius:99px;transition:width .3s}
.num{font-variant-numeric:tabular-nums;font-weight:600}
.grid{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px}
button{font:inherit;border:1px solid #30363d;background:#21262d;color:#e6edf3;border-radius:11px;padding:11px 6px;cursor:pointer;transition:.15s}
button:active{transform:scale(.96);background:#30363d}
button.p{background:#238636;border-color:#2ea043;color:#fff}
.cards{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.cb{text-align:left;padding:10px 11px;line-height:1.35}
.cb b{display:block;font-size:13px;margin-bottom:2px}
.cb span{font-size:11px;color:#8b949e}
#alert{border-color:#d29922;display:none}
#alert .t{font-weight:700;color:#d29922;margin-bottom:4px}
.feed{font-size:12px;color:#8b949e;line-height:1.9}
.feed b{color:#e6edf3;font-weight:600}
.feed .tm{color:#484f58;font-size:11px;margin-right:6px}
#toast{position:fixed;left:50%;bottom:26px;transform:translate(-50%,20px);background:#1f6feb;color:#fff;padding:9px 18px;border-radius:99px;font-size:13px;opacity:0;transition:.25s;pointer-events:none;white-space:nowrap;max-width:92vw;overflow:hidden;text-overflow:ellipsis}
#toast.on{opacity:1;transform:translate(-50%,0)}
.foot{text-align:center;color:#484f58;font-size:11px;margin-top:14px}
.dbg{display:flex;gap:8px;margin-top:8px}
.dbg button{flex:1;font-size:12px;padding:8px}
</style></head><body>

<div class="hd">
  <h1>DIGI-CORE</h1>
  <span class="muted"><i class="dot"></i><span id="ip">连接中</span></span>
</div>

<div class="card" id="alert">
  <div class="t" id="alertT">—</div>
  <div class="muted" id="alertD">—</div>
  <div class="grid" style="grid-template-columns:1fr 1fr;margin-top:10px">
    <button class="p" id="alertBtn" onclick="alertAct()">立即响应</button>
    <button onclick="hideAlert()">先不管</button>
  </div>
</div>

<div class="card">
  <div class="row">
    <div>
      <div class="name" id="pname">—</div>
      <div class="muted" id="pstage">—</div>
    </div>
    <span class="badge" id="pmode">—</span>
  </div>
  <div class="row"><span class="muted">心情</span><span class="num" id="moodv">—</span></div>
  <div class="bar"><i id="moodb" style="width:0;background:#3fb950"></i></div>
  <div class="row"><span class="muted">体力</span><span class="num" id="hpv">—</span></div>
  <div class="bar"><i id="hpb" style="width:0;background:#58a6ff"></i></div>
  <div class="grid" style="grid-template-columns:1fr 1fr 1fr 1fr;margin-top:8px">
    <div><div class="muted">饥饿</div><div class="num" id="hunger">—</div></div>
    <div><div class="muted">攻/防</div><div class="num" id="ad">—</div></div>
    <div><div class="muted">体重</div><div class="num" id="wt">—</div></div>
    <div><div class="muted">年龄</div><div class="num" id="age">—</div></div>
  </div>
  <div class="row" style="margin-top:10px">
    <span class="muted">下一阶段</span><span class="num" id="eta">—</span>
  </div>
</div>

<div class="card">
  <div class="grid" style="grid-template-columns:1fr 1fr;gap:8px">
    <button class="p" onclick="act('feed')">🍖 喂食</button>
    <button onclick="act('clean')">🧹 清洁</button>
    <button onclick="act('train')">💪 训练</button>
    <button onclick="act('battle')">⚔️ 战斗</button>
    <button onclick="act('pet')">🤚 安抚</button>
    <button onclick="act('sleep')">🌙 睡觉</button>
    <button onclick="act('wake')">☀️ 唤醒</button>
    <button onclick="act('evolve')">🧬 进化检查</button>
  </div>
</div>

<div class="card">
  <div class="muted" style="margin-bottom:8px">升级卡 · 点击注入</div>
  <div class="cards">
    <button class="cb" onclick="card('courage')"><b>🦁 勇气卡</b><span>解锁究极体·战斗暴龙兽</span></button>
    <button class="cb" onclick="card('battle')"><b>⚔️ 战斗卡</b><span>下一战临时进化一级</span></button>
    <button class="cb" onclick="card('power')"><b>⚡ 力量卡</b><span>攻击 +5</span></button>
    <button class="cb" onclick="card('guard')"><b>🛡 守护卡</b><span>防御 +5</span></button>
    <button class="cb" onclick="card('heal')"><b>❤️ 回复卡</b><span>体力与心情全满，治病</span></button>
    <button class="cb" onclick="card('revive')"><b>✨ 复活卡</b><span>死亡后再生</span></button>
  </div>
</div>

<div class="card">
  <div class="row"><span class="muted">最近事件</span><span class="muted" id="synced"></span></div>
  <div class="feed" id="feed">—</div>
</div>

<div class="card">
  <div class="row"><span class="muted">选择进化线（孵化时生效）</span></div>
  <div class="dbg" style="flex-wrap:wrap">
    <button onclick="pick(0)">🦖 亚古兽</button>
    <button onclick="pick(1)">🦕 V仔兽</button>
    <button onclick="pick(2)">🐰 大耳兽</button>
    <button onclick="pick(3)">🐹 巴达兽</button>
    <button onclick="pick(4)">🐱 迪路兽</button>
    <button onclick="pick(255)">🎲 随机</button>
  </div>
  <div class="muted" id="linepick" style="margin-top:8px">当前：—</div>
</div>

<div class="card">
  <div class="row"><span class="muted">重置游戏（回到一颗新蛋）</span></div>
  <div class="dbg"><a href="/api/reset?k=1234&h=1" onclick="return confirm('确定重新开始？当前宠物会被清除。')" style="display:inline-block;padding:10px 16px;background:#7a1f1f;color:#fff;border-radius:10px;text-decoration:none">♻️ 重新开始</a></div>
  <div class="muted" style="margin-top:8px">会清空当前宠物，选好的进化线保留</div>
</div>

<div class="card">
  <div class="row"><span class="muted">屏幕亮度</span><span class="num" id="bv">—</span></div>
  <input type="range" id="bright" min="10" max="180" value="70" style="width:100%" oninput="setB(this.value)">
  <div class="row" style="margin-top:12px"><span class="muted">战绩</span><span class="num" id="wl">—</span></div>
  <div class="row"><span class="muted">已获得卡片</span><span class="num" id="items">—</span></div>
  <div class="muted" style="margin-top:12px">调试：快进时间（验证 15~30 天节奏）</div>
  <div class="dbg">
    <button onclick="warp(6)">+6 小时</button>
    <button onclick="warp(24)">+1 天</button>
    <button onclick="warp(72)">+3 天</button>
  </div>
</div>

<div class="foot">DIGI-CORE · 局域网控制台</div>
<div id="toast"></div>

<script>
let t=null, S=null, alertKind=0;
const EV={1:'孵化',2:'进化',3:'喂食',4:'清洁',5:'训练',6:'战斗胜利',7:'战斗失败',8:'便便',9:'生病',
          10:'捡到东西',11:'呼唤',12:'睡觉',13:'醒来',14:'使用卡片',15:'死亡',16:'被无视',17:'治疗',18:'安抚',19:'野生遭遇',20:'退化'};
const ITEM_NAMES={1:'勇气',8:'力量',16:'守护',32:'复活',64:'战斗卡'};

function toast(m){const e=document.getElementById('toast');e.textContent=m;e.classList.add('on');clearTimeout(t);t=setTimeout(()=>e.classList.remove('on'),1600);}
function pct(v){return Math.max(0,Math.min(100,v));}
function fmtLeft(s){
  if(s<=0) return '可进化（等条件）';
  const d=Math.floor(s/86400), h=Math.floor(s%86400/3600), m=Math.floor(s%3600/60);
  if(d>0) return d+' 天 '+h+' 小时';
  if(h>0) return h+' 小时 '+m+' 分';
  return m+' 分钟';
}
function fmtAge(s){
  const d=Math.floor(s/86400), h=Math.floor(s%86400/3600);
  return d>0 ? (d+' 天 '+h+' 时') : (h+' 小时');
}
function apply(s){
  S=s;
  document.getElementById('pname').textContent=s.speciesCn||s.species;
  document.getElementById('pstage').textContent=(s.stageCn||s.stageName)+(s.alive?'':' · 已死亡')+(s.sick?' · 生病':'');
  document.getElementById('pmode').textContent=s.sleeping?'睡眠':((s.stageName==='EGG')?'孵化中':'活动中');
  const mood=pct(s.mood), hp=pct(s.hp);
  document.getElementById('moodv').textContent=mood;
  document.getElementById('hpv').textContent=hp;
  document.getElementById('moodb').style.width=mood+'%';
  document.getElementById('moodb').style.background=mood>60?'#3fb950':(mood>30?'#d29922':'#f85149');
  document.getElementById('hpb').style.width=hp+'%';
  document.getElementById('hunger').textContent=s.hunger+'/4';
  document.getElementById('ad').textContent=s.atk+'/'+s.def;
  document.getElementById('wt').textContent=s.weight+'g';
  document.getElementById('age').textContent=fmtAge(s.age);
  document.getElementById('eta').textContent=s.winsNeed>0&&s.wins<s.winsNeed
      ? ('还需 '+s.winsNeed+' 胜').replace(s.winsNeed,s.winsNeed-s.wins)
      : fmtLeft(s.evolveIn);
  document.getElementById('wl').textContent=s.wins+' 胜 / '+s.losses+' 负';
  document.getElementById('bv').textContent=s.bright;
  document.getElementById('bright').value=s.bright;
  document.getElementById('ip').textContent=s.ip;
  document.getElementById('synced').textContent=s.synced?'时间已同步':'时间未同步';
  const list=Object.keys(ITEM_NAMES).filter(k=>(s.items & k)).map(k=>ITEM_NAMES[k]);
  document.getElementById('items').textContent=list.length?list.join(' · '):'无';

  // 事件流
  const now=Math.floor(Date.now()/1000);
  document.getElementById('feed').innerHTML=(s.ev||[]).slice(0,8).map(([ts,k])=>{
    const ago=Math.max(0,now-ts);
    const a=ago<60?'刚刚':(ago<3600?Math.floor(ago/60)+' 分钟前':(ago<86400?Math.floor(ago/3600)+' 小时前':Math.floor(ago/86400)+' 天前'));
    return '<div><span class="tm">'+a+'</span><b>'+(EV[k]||('事件'+k))+'</b></div>';
  }).join('')||'<div>还没有事件</div>';

  // 待响应
  if(s.pending){
    alertKind=s.pending;
    document.getElementById('alert').style.display='block';
    document.getElementById('alertT').textContent=s.pending===1?'⚠️ 野生数码兽出现':'⚠️ 它在叫你';
    document.getElementById('alertD').textContent=s.pending===1
      ? ('还有 '+Math.max(0,Math.floor(s.pendingIn/60))+' 分钟，不理会会掉心情')
      : ('还有 '+Math.max(0,Math.floor(s.pendingIn/60))+' 分钟，不理会记一次照顾失误');
    document.getElementById('alertBtn').textContent=s.pending===1?'迎战':'安抚';
  } else {
    document.getElementById('alert').style.display='none';
  }
}
function hideAlert(){document.getElementById('alert').style.display='none';}
function alertAct(){
  if(alertKind===1) act('battle'); else act('pet');
  hideAlert();
}
async function refresh(){try{const r=await fetch('/api/state',{cache:'no-store'});apply(await r.json());}catch(e){}}
async function act(c){try{const r=await fetch('/api/act?c='+c);const j=await r.json();toast(j.msg||'ok');if(j.state)apply(j.state);}catch(e){toast('连接失败');}refresh();}
async function pick(n){try{const r=await fetch('/api/line?n='+n);const j=await r.json();
  const names=j.lines||[];
  document.getElementById('linepick').textContent='当前：'+(n===255?'随机':(names[n]||('线'+n)));
  toast(n===255?'已设为随机':'已选 '+names[n]);}catch(e){toast('连接失败');}}
async function resetGame(){
  if(!confirm('确定重新开始？当前宠物会被清除。')) return;
  try{const r=await fetch('/api/reset?k=1234');const j=await r.json();
    toast(j.ok?'已重置，新蛋':'重置失败');refresh();}catch(e){toast('连接失败');}
}
async function card(id){try{const r=await fetch('/api/card?id='+id);const j=await r.json();toast(j.msg||'ok');if(j.state)apply(j.state);}catch(e){toast('连接失败');}refresh();}
async function setB(v){try{await fetch('/api/bright?v='+v);}catch(e){}}
async function warp(h){try{const r=await fetch('/api/warp?h='+h);const j=await r.json();toast('快进 '+h+' 小时');if(j.state)apply(j.state);}catch(e){toast('连接失败');}refresh();}
setInterval(refresh,2000);refresh();
</script></body></html>)HTML";

// 首次配网页（设备 AP 模式时提供）
static const char SETUP_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="zh-CN"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>DIGI-CORE 配网</title>
<style>
body{margin:0;background:#0b0f14;color:#e6edf3;font:15px/1.6 -apple-system,"PingFang SC",sans-serif;padding:22px}
h1{font-size:18px;color:#7ee787;letter-spacing:.14em}
p{color:#8b949e;font-size:13px}
label{display:block;margin:16px 0 6px;font-size:13px;color:#8b949e}
input{width:100%;padding:12px;border-radius:10px;border:1px solid #30363d;background:#0d1117;color:#e6edf3;font-size:15px}
button{margin-top:20px;width:100%;padding:13px;border-radius:11px;border:0;background:#238636;color:#fff;font-size:15px}
.tip{margin-top:22px;font-size:12px;color:#484f58}
</style></head><body>
<h1>DIGI-CORE 配网</h1>
<p>设备还没连上家里的 WiFi。输入 2.4GHz 网络名称和密码，保存后会自动重启并连接。</p>
<form method="POST" action="/api/wifi">
  <label>WiFi 名称 (SSID)</label>
  <input name="ssid" autocomplete="off" required>
  <label>密码</label>
  <input name="pass" type="password" autocomplete="off">
  <button type="submit">保存并连接</button>
</form>
<div class="tip">提示：ESP32 只支持 2.4GHz。若忘记密码或换了路由器，长按板载 BOOT 键 3 秒可重新配网。</div>
</body></html>)HTML";
