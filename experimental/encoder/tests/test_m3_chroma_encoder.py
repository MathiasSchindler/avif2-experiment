#!/usr/bin/env python3

import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path
from unittest import TestCase

from .validator_support import require_validator


ROOT = Path(__file__).resolve().parents[3]
ENCODER = ROOT / "experimental" / "encoder"
DRIVER = ENCODER / "build" / "m3-chroma-driver"
VECTORS = ENCODER / "tests" / "m3_chroma_vectors.json"
WORK = ENCODER / "build" / "m3-chroma-parity"
PINNED_AVMDEC_SHA256 = (
    "c247cbb4e71e48bdec89f3c8b2b8efdce17f673aeaa39e3c116a276d16286aa0"
)
PINNED_PROJECT_SHA256 = (
    "7d17f598f2cdffe7dfb4ab46982cf5c4822215a5f429cb39e7748a60ce5ef535"
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


class M3ChromaEncoderTests(TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if WORK.exists():
            shutil.rmtree(WORK)
        WORK.mkdir(parents=True)

    @classmethod
    def tearDownClass(cls) -> None:
        if WORK.exists():
            shutil.rmtree(WORK)

    def validators(self) -> tuple[Path, Path]:
        avmdec = require_validator(
            self,
            Path(os.environ.get(
                "AVMDEC", ROOT / "build/reference/avm/avmdec")),
            "pinned avmdec")
        project = require_validator(
            self,
            Path(os.environ.get(
                "PROJECT_DECODER",
                ROOT / "build/host-cmake/av2heif-decoder")),
            "project decoder")
        self.assertEqual(sha256(avmdec), PINNED_AVMDEC_SHA256)
        self.assertEqual(sha256(project), PINNED_PROJECT_SHA256)
        return avmdec, project

    def generate(
        self, name: str, format_id: int, qindex: int,
        values: tuple[int, int, int], width: int = 8, height: int = 8
    ) -> tuple[Path, Path]:
        obu = WORK / f"{name}.obu"
        local = WORK / f"{name}.local"
        subprocess.run(
            [
                str(DRIVER), str(format_id), str(qindex),
                str(width), str(height),
                *(str(value) for value in values),
                str(obu), str(local),
            ],
            cwd=ROOT, check=True)
        return obu, local

    def assert_decoder_parity(
        self, name: str, format_id: int, qindex: int,
        values: tuple[int, int, int], width: int = 8, height: int = 8
    ) -> None:
        avmdec, project = self.validators()
        obu, local = self.generate(
            name, format_id, qindex, values, width, height)
        avm = WORK / f"{name}.avm"
        extension = "i420" if format_id == 0 else "i444"
        project_output = WORK / f"{name}.project.{extension}"
        subprocess.run(
            [
                str(avmdec), "--codec=av2", "--rawvideo",
                "--output-bit-depth=8", "--limit=1",
                "-o", str(avm), str(obu),
            ],
            cwd=ROOT, check=True, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL)
        subprocess.run(
            [str(project), str(obu), "-o", str(project_output)],
            cwd=ROOT, check=True, stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL)
        self.assertEqual(avm.read_bytes(), local.read_bytes())
        self.assertEqual(project_output.read_bytes(), local.read_bytes())

    def test_frozen_vectors(self) -> None:
        manifest = json.loads(VECTORS.read_text())
        self.assertEqual(manifest["avmdec_sha256"], PINNED_AVMDEC_SHA256)
        self.assertEqual(
            manifest["project_decoder_sha256"], PINNED_PROJECT_SHA256)
        for case in manifest["cases"]:
            obu, local = self.generate(
                case["name"], case["format"], case["qindex"],
                tuple(case["values"]), case.get("width", 8),
                case.get("height", 8))
            self.assertEqual(obu.stat().st_size, case["obu_bytes"])
            self.assertEqual(sha256(obu), case["obu_sha256"])
            self.assertEqual(
                local.stat().st_size, case["reconstruction_bytes"])
            self.assertEqual(
                sha256(local), case["reconstruction_sha256"])
            self.assert_decoder_parity(
                case["name"], case["format"], case["qindex"],
                tuple(case["values"]), case.get("width", 8),
                case.get("height", 8))

    def test_representative_q_context_parity(self) -> None:
        qindices = (1, 90, 91, 140, 141, 190, 191, 255)
        for format_id in (0, 2):
            for qindex in qindices:
                if (format_id == 0 and qindex == 1) or (
                    format_id == 2 and qindex == 255
                ):
                    continue
                values = (128, 128, 128) if qindex in (1, 141) else (
                    256, 256, 256)
                with self.subTest(format=format_id, qindex=qindex):
                    self.assert_decoder_parity(
                        f"f{format_id}-q{qindex}", format_id, qindex, values)

    def test_odd_i420_single_leaf_parity(self) -> None:
        for width, height in (
            (1, 1), (2, 7), (7, 2), (7, 7),
            (9, 9), (9, 15), (15, 9), (15, 15),
        ):
            with self.subTest(width=width, height=height):
                self.assert_decoder_parity(
                    f"i420-{width}x{height}", 0, 100,
                    (128, 131, 125), width, height)

    def test_even_multileaf_and_multisuperblock_parity(self) -> None:
        for width, height in ((64, 64), (128, 64), (128, 128)):
            for format_id in (0, 2):
                for values in ((128, 128, 128), (256, 256, 256)):
                    with self.subTest(
                        width=width, height=height,
                        format=format_id, values=values
                    ):
                        self.assert_decoder_parity(
                            f"multi-{width}x{height}-f{format_id}-{values[0]}",
                            format_id, 100, values, width, height)

    def test_even_multileaf_q_context_parity(self) -> None:
        for format_id in (0, 2):
            for qindex in (1, 90, 91, 140, 141, 190, 191, 255):
                with self.subTest(format=format_id, qindex=qindex):
                    self.assert_decoder_parity(
                        f"multi-q{qindex}-f{format_id}",
                        format_id, qindex, (256, 256, 256), 64, 64)

    def test_full_admission_dimension_parity(self) -> None:
        cases = (
            (9, 16, 100, (128, 131, 125)),
            (16, 9, 100, (128, 125, 131)),
            (8, 9, 100, (256, 256, 256)),
            (17, 16, 100, (256, 256, 256)),
            (24, 24, 100, (256, 256, 256)),
            (64, 65, 100, (256, 256, 256)),
            (25, 32, 91, (256, 256, 256)),
            (32, 25, 140, (128, 256, 128)),
            (47, 48, 141, (128, 128, 256)),
            (64, 63, 190, (256, 256, 256)),
            (79, 80, 191, (128, 131, 125)),
            (96, 95, 255, (128, 128, 128)),
            (143, 144, 1, (128, 131, 125)),
            (255, 256, 100, (128, 128, 128)),
            (447, 448, 100, (128, 128, 128)),
            (896, 424, 100, (128, 128, 128)),
            (896, 448, 100, (128, 128, 128)),
            (896, 504, 100, (128, 128, 128)),
            (896, 598, 100, (128, 128, 128)),
            (598, 896, 100, (128, 128, 128)),
            (896, 672, 100, (128, 128, 128)),
            (889, 896, 100, (128, 128, 128)),
            (896, 889, 100, (128, 128, 128)),
            (896, 896, 100, (128, 128, 128)),
            (1440, 960, 100, (128, 128, 128)),
            (960, 1440, 100, (128, 128, 128)),
            (1440, 1440, 100, (128, 128, 128)),
        )
        for format_id in (0, 2):
            for width, height, qindex, values in cases:
                with self.subTest(
                    format=format_id, width=width, height=height,
                    qindex=qindex, values=values
                ):
                    self.assert_decoder_parity(
                        f"admit-f{format_id}-{width}x{height}-q{qindex}",
                        format_id, qindex, values, width, height)

    def test_closed_dimension_bounds(self) -> None:
        for format_id in (0, 2):
            for width, height in (
                (1441, 1440), (1440, 1441),
            ):
                with self.subTest(
                    format=format_id, width=width, height=height
                ):
                    completed = subprocess.run(
                        [
                            str(DRIVER), str(format_id), "100",
                            str(width), str(height), "128", "128", "128",
                            str(WORK / "closed.obu"),
                            str(WORK / "closed.local"),
                        ],
                        cwd=ROOT, check=False,
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL)
                    self.assertEqual(completed.returncode, 3)

    def test_admitted_streams_are_deterministic(self) -> None:
        for format_id, width, height, values in (
            (0, 8, 9, (256, 256, 256)),
            (2, 64, 65, (256, 256, 256)),
            (0, 25, 32, (256, 256, 256)),
            (2, 47, 48, (128, 131, 125)),
            (0, 896, 598, (128, 128, 128)),
            (0, 896, 672, (128, 128, 128)),
            (2, 896, 896, (128, 128, 128)),
        ):
            name = f"det-f{format_id}-{width}x{height}"
            obu, local = self.generate(
                name, format_id, 100, values, width, height)
            first = (sha256(obu), sha256(local))
            obu, local = self.generate(
                name, format_id, 100, values, width, height)
            self.assertEqual((sha256(obu), sha256(local)), first)
