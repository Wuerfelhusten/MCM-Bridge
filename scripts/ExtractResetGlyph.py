"""Rasterize SMF's original reset glyph to a supersampled alpha texture.

Development-only dependencies: fonttools, shapely and Pillow. Only the derived
PNG is shipped; no frontend-owned font atlas crosses the API boundary.
"""

import math
from pathlib import Path
import sys

from fontTools.pens.basePen import BasePen
from fontTools.ttLib import TTFont
from PIL import Image, ImageDraw
from shapely import Polygon


class OutlinePen(BasePen):
    def __init__(self, glyphs):
        super().__init__(glyphs)
        self.points = []

    def _moveTo(self, point):
        self.points.append(point)

    def _lineTo(self, point):
        self.points.append(point)

    def _qCurveToOne(self, control, end):
        start = self._getCurrentPoint()
        for step in range(1, 65):
            t = step / 64
            self.points.append(tuple(
                (1 - t) ** 2 * start[axis] + 2 * (1 - t) * t * control[axis] + t ** 2 * end[axis]
                for axis in (0, 1)
            ))

    def _closePath(self):
        pass


font = TTFont(sys.argv[1])
glyphs = font.getGlyphSet()
pen = OutlinePen(glyphs)
glyphs[font.getBestCmap()[0xF0E2]].draw(pen)
polygon = Polygon(pen.points).buffer(0)
assert polygon.geom_type == "Polygon"
minimum_x, minimum_y, maximum_x, maximum_y = polygon.bounds
scale = 252 / max(maximum_x - minimum_x, maximum_y - minimum_y)
width = math.ceil((maximum_x - minimum_x) * scale) + 4
height = math.ceil((maximum_y - minimum_y) * scale) + 4
supersample = 8
alpha = Image.new("L", (width * supersample, height * supersample), 0)
draw = ImageDraw.Draw(alpha)


def pixels(points):
    return [((2 + (x - minimum_x) * scale) * supersample,
             (2 + (maximum_y - y) * scale) * supersample) for x, y in points]


draw.polygon(pixels(polygon.exterior.coords), fill=255)
for interior in polygon.interiors:
    draw.polygon(pixels(interior.coords), fill=0)
alpha = alpha.resize((width, height), Image.Resampling.LANCZOS)
image = Image.new("RGBA", (width, height), (255, 255, 255, 0))
image.putalpha(alpha)
output = Path(sys.argv[2])
output.parent.mkdir(parents=True, exist_ok=True)
image.save(output, optimize=True)
print(f"{output}: {width}x{height}, original U+F0E2, 8x supersampling")
