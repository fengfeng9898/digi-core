import io

p = r"C:\Users\Administrator\deepseekwork\esp32-digimon\app\index.html"
s = io.open(p, encoding="utf-8").read()

s = s.replace("setInterval(() => { if (S) drawPreview(S); }, 120);",
              "setInterval(function(){ try{ drawPreview(last); }catch(e){} }, 120);")

if "drawPreview(s);" not in s:
    s = s.replace("function apply(s){", "function apply(s){\n  try{ drawPreview(s); }catch(e){}", 1)

io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("fixed interval:", "drawPreview(last)" in s, "| apply hook:", "drawPreview(s);" in s)
