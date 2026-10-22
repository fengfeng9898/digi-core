import io
p = r"C:\Users\Administrator\deepseekwork\esp32-digimon\tools\import_vb.py"
s = io.open(p, encoding="utf-8").read()
s = s.replace("from PIL import Image, ImageDraw", "from PIL import Image, ImageDraw, ImageEnhance")
old = 'c = c.convert("RGBa").resize((w, TARGET_H), Image.LANCZOS).convert("RGBA")'
new = (old +
       '\n        # 点阵屏上要更分明：提高饱和度与对比度\n'
       '        _rgb = c.convert("RGB")\n'
       '        _rgb = ImageEnhance.Color(_rgb).enhance(1.55)\n'
       '        _rgb = ImageEnhance.Contrast(_rgb).enhance(1.30)\n'
       '        c = Image.merge("RGBA", _rgb.split() + (c.split()[3],))')
assert old in s
s = s.replace(old, new)
io.open(p, "w", encoding="utf-8", newline="\n").write(s)
print("patched importer")
