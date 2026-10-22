import io

p = r"C:\Users\Administrator\deepseekwork\esp32-digimon\app\index.html"
s = io.open(p, encoding="utf-8").read()

print("has line ui:", "api/line" in s, "| has canvas:", "previewCanvas" in s)

# 1) 画布改成 64x64（按像素坐标画），CSS 放大显示
s = s.replace('width="256" height="256"', 'width="64" height="64"')
s = s.replace('style="width:192px;height:192px;image-rendering:pixelated;',
              'style="width:min(78vw,300px);height:min(78vw,300px);image-rendering:pixelated;')

# 2) 选线块（如果上次没插入）
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
        '</div>\n\n'
    )
    if anchor in s:
        s = s.replace(anchor, block + anchor, 1)
    else:
        s = s.replace('<div class="toast"', block + '<div class="toast"', 1)
    print("line block inserted")

if "async function pick(" not in s:
    js = (
        "async function pick(n){\n"
        "  try{ const r=await fetch(base+'/api/line?n='+n); const j=await r.json();\n"
        "    const names=j.lines||[];\n"
        "    const el=document.getElementById('linepick');\n"
        "    if(el) el.textContent='当前：'+(n===255?'随机':(names[n]||('线'+n)));\n"
        "    toast(n===255?'已设为随机':'已选 '+names[n]);\n"
        "  }catch(e){ toast('连接失败'); }\n"
        "}\n"
        "async function resetGame(){\n"
        "  if(!confirm('确定重新开始？当前宠物会被清除。')) return;\n"
        "  try{ const r=await fetch(base+'/api/reset?k=1234'); const j=await r.json();\n"
        "    toast(j.ok?'已重置，新蛋':'重置失败');\n"
        "  }catch(e){ toast('连接失败'); }\n"
        "}\n"
    )
    s = s.replace("function apply(s){", js + "\nfunction apply(s){", 1)
    print("js inserted")

io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("line ui now:", "api/line" in s, "| canvas:", 'width="64"' in s)
