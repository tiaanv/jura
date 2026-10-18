"""Generate documentation illustrations (not live screenshots). Requires Pillow.

Run from any directory. The user-provided native-controls screenshot and original
connector reference are separate, unmodified assets and are not regenerated.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "assets"
OUT.mkdir(parents=True, exist_ok=True)
SCALE = 2


def font(size, bold=False):
    # Segoe UI on Windows; DejaVu Sans on common Linux development systems.
    for name in (("segoeuib.ttf", "DejaVuSans-Bold.ttf") if bold else
                 ("segoeui.ttf", "DejaVuSans.ttf")):
        try:
            return ImageFont.truetype(name, size * SCALE)
        except OSError:
            pass
    raise RuntimeError("Install Segoe UI or DejaVu Sans to render these diagrams")


def canvas(width, height, background):
    image = Image.new("RGB", (width * SCALE, height * SCALE), background)
    draw = ImageDraw.Draw(image)

    def text(x, y, value, size=18, color="#243247", bold=False):
        draw.text((x * SCALE, y * SCALE), value, font=font(size, bold), fill=color)

    def line(points, color, width=3):
        draw.line([(x * SCALE, y * SCALE) for x, y in points], fill=color,
                  width=width * SCALE)

    def box(bounds, fill, outline=None, radius=12):
        draw.rounded_rectangle(tuple(v * SCALE for v in bounds), radius * SCALE,
                               fill=fill, outline=outline, width=SCALE)

    return image, text, line, box


image, text, line, box = canvas(1040, 570, "#f4f7fb")
text(36, 24, "Jura ↔ ESP32-C3", 30, bold=True)
text(36, 70, "9600 baud · 8N1 · reference pins: TX GPIO4 / RX GPIO3", 18)
box((30, 122, 242, 470), "#ffffff", "#d5dfe9")
box((798, 122, 1010, 470), "#ffffff", "#d5dfe9")
text(54, 142, "JURA", 24, bold=True)
text(820, 142, "ESP32-C3", 24, bold=True)
text(54, 198, "TX · 5 V signal", 18)
text(820, 198, "RX · GPIO3", 18)
line([(242, 230), (345, 230)], "#c45c24")
box((345, 211, 435, 249), "#fff3e8", "#c45c24", 3)
text(364, 214, "1 kΩ", 18, "#8b4019", True)
line([(435, 230), (555, 230), (798, 230)], "#007f99")
box((550, 225, 560, 235), "#007f99", radius=5)
text(477, 176, "≈ 3.33 V", 21, "#007f99", True)
line([(555, 230), (555, 272)], "#007f99")
box((538, 272, 572, 338), "#e3f5f9", "#007f99", 3)
text(590, 290, "2 kΩ", 18, "#007f99", True)
line([(555, 338), (555, 437)], "#657184")
text(54, 344, "RX", 18)
text(820, 344, "TX · GPIO4", 18)
line([(242, 375), (500, 375)], "#6355b9")
line([(500, 375), (610, 375), (798, 375)], "#6355b9")
# The vertical resistor-to-ground wire crosses TX without connecting to it.
box((545, 366, 565, 384), "#f4f7fb", radius=0)
line([(555, 366), (555, 384)], "#657184")
text(331, 392, "Direct 3.3 V logic → Jura RX", 16, "#6355b9")
text(54, 423, "GND", 18)
text(820, 423, "GND", 18)
line([(242, 437), (798, 437)], "#657184")
box((550, 432, 560, 442), "#657184", radius=5)
text(400, 456, "Common ground", 16, "#657184")
text(36, 510, "Divider: 5 V × 2 kΩ / (1 kΩ + 2 kΩ) = 3.33 V", 20, bold=True)
text(36, 542, "Signal wiring only. Power connections are described in the hardware guide.", 15)
image.save(OUT / "uart-voltage-divider.png")

image, text, line, box = canvas(416, 647, "#111111")
box((5, 5, 410, 641), "#1c1c1c", "#383838", 14)
text(24, 24, "Jura E8", 25, "#eeeeee", True)
text(24, 60, "Native monitoring · illustration", 14, "#9aa7b2")
box((24, 94, 391, 137), "#163b36", radius=8)
text(38, 104, "●  Connected", 18, "#78d4b7", True)


def row(y, label, value, color="#eeeeee"):
    text(25, y, label, 16, "#aeb8c1")
    text(25, y + 24, value, 20, color, True)


row(157, "Detected model", "EF533M V02.09")
row(224, "Machine status", "Ready", "#78d4b7")
row(291, "Saved espresso recipe", "8  ·  45 ml  ·  High", "#45c4ed")
line([(25, 359), (390, 359)], "#383838", 1)
text(25, 379, "PROFILE-BASED DRINK COUNTERS", 13, "#aeb8c1", True)
for y, label, value in [(415, "Espresso", "245"), (452, "Coffee", "333"),
                         (489, "Flat White", "11,583"), (526, "Cappuccino", "58")]:
    text(25, y, label, 17, "#dddddd")
    text(292, y, value, 18, "#eeeeee", True)
text(25, 586, "Example layout; values from E8 testing.", 13, "#9aa7b2")
text(25, 608, "Readings are not a simultaneous snapshot.", 12, "#9aa7b2")
image.save(OUT / "ha-monitoring-illustration.png")
print("Rendered voltage divider and monitoring illustration")
