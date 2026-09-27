"""Create the repository-owned bitmap artwork used by the WP2 table sample.

This is an offline authoring step. The Rocket application only loads the
checked-in PNG files and draws them as textures.
"""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont


ROOT = Path(__file__).parent / "assets"
ROOT.mkdir(exist_ok=True)


def font(size: int, bold: bool = False):
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    try:
        return ImageFont.truetype(name, size)
    except OSError:
        return ImageFont.load_default()


felt = Image.new("RGBA", (800, 450), (9, 48, 42, 255))
d = ImageDraw.Draw(felt)
d.rounded_rectangle((22, 18, 778, 432), radius=180, fill=(25, 86, 71),
                    outline=(219, 179, 93), width=8)
d.rounded_rectangle((47, 43, 753, 407), radius=160, outline=(249, 220, 139),
                    width=2)
d.ellipse((185, 105, 615, 395), outline=(177, 218, 183), width=2)
d.ellipse((225, 135, 575, 370), outline=(89, 156, 124), width=2)
d.text((303, 47), "ROCKET ROYALE", font=font(24, True), fill=(247, 218, 146))
d.text((320, 391), "PLACE YOUR BETS", font=font(16, True), fill=(231, 205, 132))
felt.convert("RGB").save(ROOT / "felt.png", optimize=True)

cards = Image.new("RGBA", (320, 112), (0, 0, 0, 0))
d = ImageDraw.Draw(cards)
for index, (rank, suit, ink) in enumerate((
        ("A", "♠", (24, 32, 39)),
        ("K", "♥", (173, 36, 53)),
        ("Q", "♦", (173, 36, 53)),
        ("J", "♣", (24, 32, 39)))):
    x = index * 80
    d.rounded_rectangle((x + 2, 2, x + 78, 110), radius=8,
                        fill=(250, 248, 237), outline=(186, 166, 127), width=2)
    d.text((x + 11, 9), rank, font=font(26, True), fill=ink)
    d.text((x + 28, 37), suit, font=font(37), fill=ink)
    d.text((x + 55, 78), rank, font=font(18, True), fill=ink)
cards.save(ROOT / "card-atlas.png", optimize=True)

chips = Image.new("RGBA", (216, 72), (0, 0, 0, 0))
d = ImageDraw.Draw(chips)
for index, (body, label) in enumerate((
        ((158, 45, 51), "25"),
        ((38, 71, 137), "50"),
        ((34, 39, 47), "100"))):
    x = index * 72
    d.ellipse((x + 3, 3, x + 69, 69), fill=body,
              outline=(241, 221, 171), width=4)
    d.ellipse((x + 13, 13, x + 59, 59), outline=(248, 239, 211), width=3)
    d.text((x + (25 if len(label) == 2 else 20), 28), label,
           font=font(17, True), fill=(255, 246, 218))
chips.save(ROOT / "chip-atlas.png", optimize=True)
