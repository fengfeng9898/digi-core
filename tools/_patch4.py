import io

p = r"C:\Users\Administrator\deepseekwork\esp32-digimon\tools\import_vb.py"
s = io.open(p, encoding="utf-8").read()
s = s.replace("TARGET_H = 46", "TARGET_H = 42")
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("TARGET_H=42")
