import sys
import unittest
from pathlib import Path


sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))

from generate_10bit_avm_references import (  # noqa: E402
    coding_tool_profile,
    encode_command,
)
from generate_10bit_synthetic_references import (  # noqa: E402
    coverage_atoms,
    lossless_encode_command,
    select_qps,
)
from generate_10bit_negative_fixtures import (  # noqa: E402
    specialized_encode_command,
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

    def test_selection_retains_new_coverage_and_final_endpoint(self) -> None:
        candidates = [
            {"qp": 90, "coverage_atoms": ["qband:1", "prediction:DC"]},
            {"qp": 91, "coverage_atoms": ["qband:1", "prediction:DC"]},
            {"qp": 92, "coverage_atoms": ["qband:1", "prediction:D45"]},
            {"qp": 93, "coverage_atoms": ["qband:1", "prediction:D45"]},
        ]
        self.assertEqual(select_qps(candidates), [90, 92, 93])

    def test_coverage_atoms_include_required_evidence_classes(self) -> None:
        census = {
            "base_qindex": 161,
            "features": [
                {
                    "mode": {"name": "D45"},
                    "uv_mode": {"name": "CFL"},
                    "partition": 3,
                    "block_size": {"name": "BLOCK_16X16"},
                    "tx_size": {"name": "TX_8X8"},
                    "primary_tx_type": {"name": "DCT_DCT"},
                    "skip_transform": 0,
                    "fsc_y": 1,
                    "fsc_uv": 0,
                    "use_intrabc": 0,
                    "palette_y": 0,
                    "palette_uv": 0,
                    "cfl_index": 2,
                    "cfl_alpha_index": 7,
                    "cfl_alpha_signs": 1,
                }
            ],
            "accounting_symbols": [
                {
                    "function": "read_filter_control",
                    "file": "decoder.c",
                    "tags": ["strength"],
                    "minimum_value": 0,
                    "maximum_value": 3,
                }
            ],
        }
        atoms = coverage_atoms(census)
        self.assertIn("qband:10", atoms)
        self.assertIn("prediction:D45", atoms)
        self.assertIn("transform:DCT_DCT", atoms)
        self.assertIn("cfl:2:7:1", atoms)
        self.assertIn("filter:read_filter_control:strength:0:3", atoms)

    def test_explicit_lossless_command_is_not_qp_zero(self) -> None:
        command = lossless_encode_command(
            Path("avmenc"),
            Path("input.yuv"),
            Path("output.obu"),
            "yuv420p10le",
            16,
            16,
        )
        self.assertIn("--lossless=1", command)
        self.assertNotIn("--end-usage=q", command)
        self.assertNotIn("--qp=0", command)

    def test_specialized_negative_profiles_are_explicit(self) -> None:
        twelve_bit = specialized_encode_command(
            Path("avmenc"), Path("input.yuv"), Path("output.obu"), "12bit-420"
        )
        i422 = specialized_encode_command(
            Path("avmenc"), Path("input.yuv"), Path("output.obu"), "10bit-422"
        )
        self.assertIn("--profile=2", twelve_bit)
        self.assertIn("--bit-depth=12", twelve_bit)
        self.assertIn("--input-bit-depth=12", twelve_bit)
        self.assertIn("--profile=2", i422)
        self.assertIn("--i422", i422)
        self.assertIn("--input-chroma-subsampling-y=0", i422)


if __name__ == "__main__":
    unittest.main()