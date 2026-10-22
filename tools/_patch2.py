import io

p = r"C:\Users\Administrator\deepseekwork\esp32-digimon\tools\import_vb.py"
s = io.open(p, encoding="utf-8").read()
s = s.replace("TARGET_H = 40", "TARGET_H = 46")
s = s.replace(".enhance(1.30)", ".enhance(1.08)")
io.open(p, "w", encoding="utf-8", newline="\n").write(s)

p2 = r"C:\Users\Administrator\deepseekwork\esp32-digimon\src\ui\ui.cpp"
s2 = io.open(p2, encoding="utf-8").read()
s2 = s2.replace("(int16_t)(29 + dy), dim);", "(int16_t)(31 + dy), dim);")
io.open(p2, "w", encoding="utf-8", newline="\n").write(s2)
print("patched TARGET_H=46, contrast=1.08, sprite cy=31")
