import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
TOOL_CANDIDATES = (
    ROOT / "build" / "host" / "av2heif-artifact",
    ROOT / "build" / "host-sanitized" / "av2heif-artifact",
)
TOOL = next((candidate for candidate in TOOL_CANDIDATES if candidate.is_file()),
            TOOL_CANDIDATES[0])
sys.path.insert(0, str(ROOT / "tools"))

from run_m3_lab import (  # noqa: E402
    encoded_byte_comparison,
    interpreted_metadata_comparison,
)
from m0_carriage import parse_obus  # noqa: E402


FIXTURE_OBUS = parse_obus(
    (ROOT / "tests" / "fixtures" / "i444-tiny.obu").read_bytes()
)
CONFIG_OBU = next(
    entry.encoded for entry in FIXTURE_OBUS if entry.name == "sequence_header"
)
ITEM_PAYLOAD = next(
    entry.encoded for entry in FIXTURE_OBUS if entry.name == "closed_loop_key"
)


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
            self.skipTest("av2heif-artifact has not been built")
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
            self.assertEqual(document["query"]["output_bytes"], 768)
            self.assertGreater(document["query"]["workspace_bytes"], 0)
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
                    "pixel_format": "I444",
                    "channels": 3,
                    "bits_per_channel": 8,
                    "bytes_per_sample": 1,
                    "sample_storage": "u8",
                    "byte_order": "not-applicable",
                    "plane_order": "YUV",
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