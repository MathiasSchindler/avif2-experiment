import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from generate_10bit_avm_references import (  # noqa: E402
    coding_tool_profile,
    encode_command,
)


class AvmReferenceProfileTests(unittest.TestCase):
    def command_for(self, pixel_format: str) -> list[str]:
        return encode_command(
            Path("avmenc"),
            Path("input.yuv"),
            Path("output.obu"),
            pixel_format,
            17,
            15,
            128,
        )

    def test_common_coding_tools_are_explicitly_disabled(self) -> None:
        expected = {
            "--film-grain-test=0",
            "--enable-lcr=0",
            "--enable-operating-point-sets=0",
            "--enable-atlas=0",
            "--enable-restoration=0",
        }
        for pixel_format in ("yuv420p10le", "yuv444p10le"):
            with self.subTest(pixel_format=pixel_format):
                command = self.command_for(pixel_format)
                self.assertTrue(expected.issubset(command))
                self.assertEqual(command.count("--enable-restoration=0"), 1)

    def test_format_specific_profile_is_explicit(self) -> None:
        i420 = self.command_for("yuv420p10le")
        i444 = self.command_for("yuv444p10le")
        self.assertIn("--profile=0", i420)
        self.assertIn("--i420", i420)
        self.assertIn("--chroma-sample-position=unspecified", i420)
        self.assertNotIn("--enable-intrabc=0", i420)
        self.assertIn("--profile=1", i444)
        self.assertIn("--i444", i444)
        self.assertIn("--enable-intrabc=0", i444)

    def test_manifest_profile_matches_commands(self) -> None:
        profile = coding_tool_profile()
        self.assertEqual(profile["enabled"], [])
        self.assertEqual(profile["unlisted_tools"], "pinned AVM defaults")
        self.assertIn("restoration", profile["disabled_for_all_formats"])
        self.assertEqual(profile["disabled_by_format"]["yuv420p10le"], [])
        self.assertEqual(profile["disabled_by_format"]["yuv444p10le"], ["intrabc"])

    def test_unknown_pixel_format_fails(self) -> None:
        with self.assertRaisesRegex(RuntimeError, "unsupported source format"):
            self.command_for("yuv422p10le")


if __name__ == "__main__":
    unittest.main()