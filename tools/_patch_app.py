import io

p = r"C:\Users\Administrator\deepseekwork\esp32-digimon\app\index.html"
s = io.open(p, encoding="utf-8").read()

if "api/line" not in s:
    anchor = '<div class="sec-t">固件更新（OTA）</div>'
    block = (
        '<div class="sec-t">进化线 / 重置</div>\n'
        '<div class="card">\n'
        '  <div class="row" style="flex-wrap:wrap;gap:6px">\n'
        '    <button onclick="pick(0)">🦖 亚古兽</button>\n'
        '    <button onclick="pick(1)">🦕 V仔兽</button>\n'
        '    <button onclick="pick(2)">🐰 大耳兽</button>\n'
        '    <button onclick="pick(3)">🐹 巴达兽</button>\n'
        '    <button onclick="pick(4)">🐱 迪路兽</button>\n'
        '    <button onclick="pick(255)">🎲 随机</button>\n'
        '  </div>\n'
        '  <div class="row" style="margin-top:8px"><span class="muted" id="linepick">当前：—</span></div>\n'
        '  <div class="row" style="margin-top:8px">\n'
        '    <button onclick="resetGame()" style="background:#7a1f1f">♻️ 重置游戏</button>\n'
        '  </div>\n'
        '</div>\n\n' + anchor
    )
    s = s.replace(anchor, block, 1)

if "async function pick(" not in s:
    js_anchor = "async function ota(){"
    js = (
        "async function pick(n){\n"
        "  try{\n"
        "    const r = await fetch(base + '/api/line?n=' + n);\n"
        "    const j = await r.json();\n"
        "    const names = j.lines || [];\n"
        "    $('linepick').textContent = '当前：' + (n === 255 ? '随机' : (names[n] || ('线' + n)));\n"
        "    toast(n === 255 ? '已设为随机' : '已选 ' + names[n]);\n"
        "  }catch(e){ toast('连接失败'); }\n"
        "}\n\n"
        "async function resetGame(){\n"
        "  if(!confirm('确定重新开始？当前宠物会被清除。')) return;\n"
        "  try{\n"
        "    const r = await fetch(base + '/api/reset?k=1234');\n"
        "    const j = await r.json();\n"
        "    toast(j.ok ? '已重置，新蛋' : '重置失败');\n"
        "    setTimeout(()=>{ poll(); }, 600);\n"
        "  }catch(e){ toast('连接失败'); }\n"
        "}\n\n" + js_anchor
    )
    s = s.replace(js_anchor, js, 1)

io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("app page patched:", "api/line" in s, "async function pick(" in s)
