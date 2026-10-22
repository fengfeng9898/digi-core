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
button.d{background:#21262d;color:#f85149;border-color:#f8514944}
.cards{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.cb{text-align:left;padding:10px 11px;line-height:1.35}
.cb b{display:block;font-size:13px;margin-bottom:2px}
.cb span{font-size:11px;color:#8b949e}
#toast{position:fixed;left:50%;bottom:26px;transform:translate(-50%,20px);background:#1f6feb;color:#fff;padding:9px 18px;border-radius:99px;font-size:13px;opacity:0;transition:.25s;pointer-events:none;white-space:nowrap;max-width:92vw;overflow:hidden;text-overflow:ellipsis}
#toast.on{opacity:1;transform:translate(-50%,0)}
.foot{text-align:center;color:#484f58;font-size:11px;margin-top:14px}
</style></head><body>

<div class="hd">
  <h1>DIGI-CORE</h1>
  <span class="muted"><i class="dot"></i><span id="ip">连接中</span></span>
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
</div>

<div class="card">
  <div class="grid" style="grid-template-columns:1fr 1fr;gap:8px">
    <button class="p" onclick="act('feed')">🍖 喂食</button>
    <button onclick="act('clean')">🧹 清洁</button>
    <button onclick="act('train')">💪 训练</button>
    <button onclick="act('battle')">⚔️ 战斗</button>
    <button onclick="act('sleep')">🌙 睡觉</button>
    <button onclick="act('wake')">☀️ 唤醒</button>
  </div>
</div>

<div class="card">
  <div class="muted" style="margin-bottom:8px">升级卡 · 点击注入</div>
  <div class="cards">
    <button class="cb" onclick="card('fire')"><b>🔥 进化卡·火</b><span>究极体·炽焰龙</span></button>
    <button class="cb" onclick="card('water')"><b>💧 进化卡·水</b><span>究极体·深潮兽</span></button>
    <button class="cb" onclick="card('nature')"><b>🌿 进化卡·自然</b><span>究极体·森罗兽</span></button>
    <button class="cb" onclick="card('power')"><b>⚡ 力量卡</b><span>攻击 +5</span></button>
    <button class="cb" onclick="card('guard')"><b>🛡 守护卡</b><span>防御 +5</span></button>
    <button class="cb" onclick="card('heal')"><b>❤️ 回复卡</b><span>体力与心情全满</span></button>
    <button class="cb" onclick="card('revive')"><b>✨ 复活卡</b><span>死亡后再生</span></button>
    <button class="cb" onclick="act('evolve')"><b>🧬 进化检查</b><span>立即结算一次</span></button>
  </div>
</div>

<div class="card">
  <div class="row"><span class="muted">屏幕亮度</span><span class="num" id="bv">—</span></div>
  <input type="range" id="bright" min="10" max="180" value="70" style="width:100%" oninput="setB(this.value)">
  <div class="row" style="margin-top:12px"><span class="muted">战绩</span><span class="num" id="wl">—</span></div>
  <div class="row"><span class="muted">已获得卡片</span><span class="num" id="items">—</span></div>
</div>

<div class="foot">DIGI-CORE · 局域网控制台</div>
<div id="toast"></div>

<script>
let t = null;
function toast(m){const e=document.getElementById('toast');e.textContent=m;e.classList.add('on');clearTimeout(t);t=setTimeout(()=>e.classList.remove('on'),1600);}
function pct(v){return Math.max(0,Math.min(100,v));}
const ITEM_NAMES={1:'火',2:'水',4:'自然',8:'力量',16:'守护',32:'复活'};
function apply(s){
  document.getElementById('pname').textContent=s.speciesCn||s.species;
  document.getElementById('pstage').textContent=(s.stageCn||s.stageName)+(s.alive?'':' · 已死亡');
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
  document.getElementById('age').textContent=Math.floor(s.age/60)+'m';
  document.getElementById('wl').textContent=s.wins+' 胜 / '+s.losses+' 负';
  document.getElementById('bv').textContent=s.bright;
  document.getElementById('bright').value=s.bright;
  document.getElementById('ip').textContent=s.ip;
  const list=Object.keys(ITEM_NAMES).filter(k=>(s.items & k)).map(k=>ITEM_NAMES[k]);
  document.getElementById('items').textContent=list.length?list.join(' · '):'无';
}
async function refresh(){try{const r=await fetch('/api/state',{cache:'no-store'});apply(await r.json());}catch(e){}}
async function act(c){try{const r=await fetch('/api/act?c='+c);const j=await r.json();toast(j.msg||'ok');apply(j.state||{});}catch(e){toast('连接失败');}refresh();}
async function card(id){try{const r=await fetch('/api/card?id='+id);const j=await r.json();toast(j.msg||'ok');apply(j.state||{});}catch(e){toast('连接失败');}refresh();}
async function setB(v){try{await fetch('/api/bright?v='+v);}catch(e){}}
setInterval(refresh,1500);refresh();
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
