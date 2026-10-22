import io

p = r"C:\Users\Administrator\deepseekwork\esp32-digimon\tools\import_vb.py"
s = io.open(p, encoding="utf-8").read()
s = s.replace("CW, CH = 64, 40", "CW, CH = 64, 48")
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("canvas CH=48")
