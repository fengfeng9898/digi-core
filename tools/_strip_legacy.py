import io

p = r"C:\Users\Administrator\deepseekwork\esp32-digimon\src\game\pet.cpp"
s = io.open(p, encoding="utf-8").read()
start = s.index("// 旧的单线进化体")
end = s.index("// ---------------------------------------------------------------- tick ----")
s = s[:start] + s[end:]
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("removed legacy block, new length", len(s))
