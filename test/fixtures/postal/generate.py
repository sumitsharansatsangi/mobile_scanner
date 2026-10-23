"""Regenerate synthetic postal image fixtures (requires Pillow).

Digit bars and lengths follow USPS C840 / Postal Bulletin 22083:
https://pe.usps.com/archive/html/dmmarchive20041209/C840.htm
https://about.usps.com/postal-bulletin/2002/html/pb22083/a-d.html
PLANET complements each digit's heights, retaining tall frame bars.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageOps

PATTERNS = ["11000", "00011", "00101", "00110", "01001", "01010",
            "01100", "10001", "10010", "10100"]
CASES = [("postnet5", "12345", False), ("postnet9", "123456789", False),
         ("postnet11", "12345678901", False), ("planet11", "40123456789", True),
         ("planet13", "4012345678901", True)]
ROOT = Path(__file__).parent


def render(text, planet, corrupt=False):
    check = (-sum(map(int, text))) % 10
    digits = text + str((check + int(corrupt)) % 10)
    groups = [PATTERNS[int(d)] for d in digits]
    if planet:
        groups = [g.translate(str.maketrans("01", "10")) for g in groups]
    bars = "1" + "".join(groups) + "1"
    image = Image.new("L", (len(bars) * 8 + 48, 100), 255)
    draw = ImageDraw.Draw(image)
    for i, bar in enumerate(bars):
        draw.rectangle((24 + i * 8, 24 if bar == "1" else 48,
                        26 + i * 8, 71), fill=0)
    return image


if __name__ == "__main__":
    for name, text, planet in CASES:
        image = render(text, planet)
        image.save(ROOT / f"{name}.png")
        image.transpose(Image.Transpose.ROTATE_90).save(ROOT / f"{name}_rotated.png")
        image.transpose(Image.Transpose.ROTATE_180).save(ROOT / f"{name}_upside_down.png")
        ImageOps.invert(image).save(ROOT / f"{name}_inverted.png")
        render(text, planet, corrupt=True).save(ROOT / f"{name}_bad_checksum.png")
    # A valid checksum does not make an unsupported length a postal symbol.
    render("123456", False).save(ROOT / "postnet_bad_length.png")
    render("4012345678", True).save(ROOT / "planet_bad_length.png")
    for name, text, planet in CASES[:1] + CASES[3:4]:
        missing = render(text, planet)
        ImageDraw.Draw(missing).rectangle((24, 24, 26, 71), fill=255)
        missing.save(ROOT / f"{name}_missing_guard.png")
