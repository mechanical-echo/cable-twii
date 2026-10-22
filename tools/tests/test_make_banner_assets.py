import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import make_banner_assets as assets


class BannerAssets(unittest.TestCase):
    def test_banner_is_608_by_456_rgb(self):
        image = assets.render_banner()
        self.assertEqual((assets.BANNER_W, assets.BANNER_H), image.size)
        self.assertEqual((608, 456), image.size)
        self.assertEqual("RGB", image.mode)

    def test_icon_is_128_by_96_rgb(self):
        image = assets.render_icon()
        self.assertEqual((128, 96), image.size)
        self.assertEqual("RGB", image.mode)

    def test_banner_corners_are_stripes(self):
        image = assets.render_banner()
        for x, y in [(1, 1), (606, 1), (1, 454), (606, 454)]:
            self.assertIn(image.getpixel((x, y)), (assets.STRIPE_A[:3], assets.STRIPE_B[:3]))

    def test_banner_panel_is_white_inside(self):
        image = assets.render_banner()
        x0, y0, x1, y1 = assets.banner_panel_rect()
        self.assertEqual(assets.PANEL_FILL[:3], image.getpixel((x0 + 12, (y0 + y1) // 2)))
        self.assertEqual(assets.PANEL_FILL[:3], image.getpixel((x1 - 12, (y0 + y1) // 2)))

    def test_banner_has_blue_logo_ink(self):
        image = assets.render_banner()
        blue = assets.BLUE[:3]
        found = any(image.getpixel((x, y)) == blue for x in range(0, 608, 2) for y in range(0, 456, 2))
        self.assertTrue(found)

    def test_icon_panel_is_white_inside(self):
        image = assets.render_icon()
        x0, y0, x1, y1 = assets.icon_panel_rect()
        self.assertEqual(assets.PANEL_FILL[:3], image.getpixel((x0 + 6, (y0 + y1) // 2)))


if __name__ == "__main__":
    unittest.main()
