import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL = ROOT / "build" / "host" / "av2heif-artifact"
sys.path.insert(0, str(ROOT / "tools"))

from run_m3_lab import (  # noqa: E402
    encoded_byte_comparison,
    interpreted_metadata_comparison,
)
CONFIG_OBU = bytes(
    [
        0x0C,
        0x04,
        0x82,
        0x0A,
        0x33,
        0xFF,
        0x1C,
        0x39,
        0xCD,
        0xE4,
        0x6E,
        0x42,
        0x20,
    ]
)
ITEM_PAYLOAD = bytes([0x02, 0x10, 0xAA])


class ArtifactToolTests(unittest.TestCase):
    def run_tool(self, *arguments: Path | str) -> None:
        subprocess.run(
            [str(TOOL), *(str(argument) for argument in arguments)],
            cwd=ROOT,
            check=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )

    def test_pack_inspect_extract_round_trip(self) -> None:
        if not TOOL.is_file():
            self.skipTest("build/host/av2heif-artifact has not been built")
        with tempfile.TemporaryDirectory() as directory_name:
            directory = Path(directory_name)
            config = directory / "config.obu"
            payload = directory / "payload.obu"
            container = directory / "canonical.av2h"
            inspection = directory / "inspection.json"
            extracted_config = directory / "extracted-config.obu"
            extracted_payload = directory / "extracted-payload.obu"
            reconstructed = directory / "reconstructed.obu"
            config.write_bytes(CONFIG_OBU)
            payload.write_bytes(ITEM_PAYLOAD)

            self.run_tool(
                "pack", config, payload, container, "2", "2", "2", "limited"
            )
            self.run_tool("inspect", container, inspection)
            self.run_tool(
                "extract",
                container,
                extracted_config,
                extracted_payload,
                reconstructed,
            )

            document = json.loads(inspection.read_text(encoding="ascii"))
            self.assertEqual(document["schema"], "av2heif-artifact-inspection-v2")
            self.assertEqual(document["query"]["dialect"], "canonical-v0")
            self.assertEqual(document["query"]["output_bytes"], 384)
            self.assertEqual(document["query"]["workspace_bytes"], 0)
            self.assertEqual(
                [box["type"] for box in document["container"]["box_tree"]],
                ["ftyp", "meta", "mdat"],
            )
            self.assertEqual(
                [
                    (property_["type"], property_["essential"])
                    for property_ in document["container"]["properties"]
                ],
                [
                    ("av2C", True),
                    ("ispe", False),
                    ("pixi", False),
                    ("colr", False),
                ],
            )
            self.assertEqual(
                document["codec"]["effective"],
                {
                    "width": 16,
                    "height": 16,
                    "pixel_format": "I420",
                    "channels": 3,
                    "bits_per_channel": 8,
                    "range": "limited",
                    "cicp": {
                        "color_primaries": 2,
                        "transfer_characteristics": 2,
                        "matrix_coefficients": 2,
                    },
                },
            )
            self.assertEqual(extracted_config.read_bytes(), CONFIG_OBU)
            self.assertEqual(extracted_payload.read_bytes(), ITEM_PAYLOAD)
            self.assertEqual(reconstructed.read_bytes(), CONFIG_OBU + ITEM_PAYLOAD)

    def test_comparison_classifications_are_data_driven(self) -> None:
        self.assertEqual(
            encoded_byte_comparison("same", "same")["classification"], "identical"
        )
        self.assertEqual(
            encoded_byte_comparison("left", "right")["classification"],
            "different-after-normalization",
        )
        source = {
            "width": 16,
            "height": 16,
            "pixel_format": "I420",
            "bit_depth": 8,
            "range": "limited",
            "cicp": {
                "color_primaries": 2,
                "transfer_characteristics": 2,
                "matrix_coefficients": 2,
            },
        }
        effective = {
            "width": 16,
            "height": 16,
            "pixel_format": "I420",
            "bits_per_channel": 8,
            "range": "limited",
            "cicp": source["cicp"],
        }
        self.assertTrue(interpreted_metadata_comparison(source, effective)["match"])
        effective["range"] = "full"
        mismatch = interpreted_metadata_comparison(source, effective)
        self.assertFalse(mismatch["match"])
        self.assertEqual(mismatch["classification"], "mismatch")


if __name__ == "__main__":
    unittest.main()