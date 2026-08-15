#!/usr/bin/env python3

import json
import shutil
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "experimental/encoder/tools"))

from raw_planar_input import (
    RawInputError,
    dimensions_from_prepared_manifest,
    dimensions_from_prepared_name,
    expected_size,
)


class RawPlanarInputTests(unittest.TestCase):
    def setUp(self) -> None:
        self.work = ROOT / "experimental/encoder/build/raw-planar-python"
        shutil.rmtree(self.work, ignore_errors=True)
        self.work.mkdir(parents=True)

    def tearDown(self) -> None:
        shutil.rmtree(self.work, ignore_errors=True)

    def write_manifest(self, name: str, width: int, height: int,
                       key: str, byte_size: int) -> Path:
        path = self.work / name
        manifest = {
            "dimensions": [width, height],
            "outputs": {
                key: {"filename": name, "byte_size": byte_size}
            },
        }
        (self.work / "manifest.json").write_text(json.dumps(manifest))
        return path

    def test_even_and_odd_i420_sizes_use_ceil_chroma(self) -> None:
        self.assertEqual(expected_size("i420-8", 4, 2), 12)
        self.assertEqual(expected_size("i420-8", 3, 3), 17)
        self.assertEqual(expected_size("i420-8", 17, 15), 399)

    def test_strict_prepared_names(self) -> None:
        metadata = dimensions_from_prepared_name(
            Path("source-17x15-yuv420p8.i420")
        )
        self.assertEqual(
            (metadata.pixel_format, metadata.width, metadata.height,
             metadata.byte_size),
            ("i420-8", 17, 15, 399),
        )
        for name in (
            "17x15.i420",
            "source-017x15-yuv420p8.i420",
            "source-17x15-yuv420p8.raw",
            "source-17x15-gray8.i420",
            "source-16385x1-gray8.i400",
        ):
            with self.subTest(name=name), self.assertRaises(RawInputError):
                dimensions_from_prepared_name(Path(name))

    def test_manifest_must_agree_exactly(self) -> None:
        path = self.write_manifest(
            "source-3x3-yuv420p8.i420", 3, 3, "yuv420p8", 17
        )
        metadata = dimensions_from_prepared_manifest(path)
        self.assertEqual(metadata.byte_size, 17)

        malformed = [
            {"dimensions": [3, 4], "outputs": {
                "yuv420p8": {"filename": path.name, "byte_size": 17}}},
            {"dimensions": [3, 3], "outputs": {
                "yuv420p8": {"filename": "other.i420", "byte_size": 17}}},
            {"dimensions": [3, 3], "outputs": {
                "yuv420p8": {"filename": path.name, "byte_size": 18}}},
            {"dimensions": ["3", 3], "outputs": {
                "yuv420p8": {"filename": path.name, "byte_size": 17}}},
            [],
        ]
        for record in malformed:
            with self.subTest(record=record):
                (self.work / "manifest.json").write_text(json.dumps(record))
                with self.assertRaises(RawInputError):
                    dimensions_from_prepared_manifest(path)

    def test_generic_raw_is_never_guessed_even_with_manifest(self) -> None:
        path = self.write_manifest("frame.i444", 2, 2, "yuv444p8", 12)
        with self.assertRaisesRegex(RawInputError, "not discoverable"):
            dimensions_from_prepared_manifest(path)


if __name__ == "__main__":
    unittest.main()
